// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Memory/Allocators/Placement>

namespace NMib
{
	namespace NConcurrency
	{
		constexpr static const mint gc_ActorQueueDispatchFunctionMemory = fg_AlignUpConstExpr(sizeof(void*)*8*2, mint(DMibPMemoryCacheLineSize));
		using FActorQueueDispatch = NFunction::TCFunction
			<
				void (NFunction::CThisTag &)
				, NFunction::CFunctionNoCopyTag
				, NFunction::TCFunctionNoAllocOptions<true, gc_ActorQueueDispatchFunctionMemory - sizeof(void *)*(2 + 2)>
			>
		;
		class CConcurrentRunQueue
		{
		public:
			CConcurrentRunQueue();
			~CConcurrentRunQueue();
			void f_AddToQueue(FActorQueueDispatch &&_Functor);
			bool f_AddToQueueLocal(FActorQueueDispatch &&_Functor);
			bool f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor);
			FActorQueueDispatch *f_FirstQueueEntry();
			void f_PopQueueEntry(FActorQueueDispatch *_pEntry);
			bool f_TransferThreadSafeQueue();
			bool f_IsEmpty();
		private:
 			struct align_cacheline CQueueEntry
			{
				DMibListLinkD_Trans(CQueueEntry, m_Link);
				union
				{
					NAtomic::TCAtomic<CQueueEntry *> m_pNextQueued;
					DMibListLinkDSA_LinkType m_Link;
				};
				FActorQueueDispatch m_fToCall;
				CQueueEntry(FActorQueueDispatch &&_fToCall);
				~CQueueEntry();
			};	
			
			DMibListLinkDS_List(CQueueEntry, m_Link) mp_LocalQueue;
			align_cacheline NAtomic::TCAtomic<CQueueEntry *> mp_pFirstQueued;
		};
		
		struct ICDistributedActorData : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
		{
			virtual ~ICDistributedActorData();
			virtual bool f_IsValidForCall() const = 0;
		};

		/// \brief Manages actor execution and lifetime
		class CActorHolder : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
		{
			typedef NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer> CSuper;
			friend class CConcurrencyManager;
			friend class CActor;
			friend class CDelegatedActorHolder;
		protected:
			bool fp_AddToQueue(FActorQueueDispatch &&_Functor);
			virtual void fp_StartQueueProcessing();
			
			template <typename tf_CActor>
			TCActor<tf_CActor> fp_GetAsActor();

			virtual void fp_DestroyThreaded();
			virtual void fp_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame) = 0;
			void fp_RunProcess();

		public:
			CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);
			virtual ~CActorHolder();

			aint f_RefCountDecrease();
			
			void f_SetFixedCore(mint _iFixedCore);
			void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false);
			bool f_ImmediateDelete() const;
			bool f_IsDestroyed() const;
			
			TCDispatchedActorCall<void> f_Destroy2();
			void f_DestroyNoResult(ch8 const *_pFile, uint32 _Line);
			void f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop = CActorDestroyEventLoop());
			
			inline_always CConcurrencyManager &f_ConcurrencyManager() const;
			inline_always EPriority f_GetPriority() const;
			
			NPtr::TCSharedPointer<ICDistributedActorData> &f_GetDistributedActorData();
			NPtr::TCSharedPointer<ICDistributedActorData> const &f_GetDistributedActorData() const;
			
		private:
			void fp_ConstructActor(NFunction::TCFunctionNoAllocMutable<void ()> &&_fConstruct, void *_pActorMemory);
			
			template <typename tf_CActor, typename tf_CFunctor>
			void fp_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> &&_ResultCall, NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed);
			
			void fp_Destroy(NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed);
			bool fp_Terminate(NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed);

			bool fp_DequeueProcess(bool _bRun);
			
		private:
#ifdef DMibDebug
			DMibListLinkDS_Link(CActorHolder, m_ActorLink);
		public:
			NStr::CStr m_ActorTypeName;
#endif
			
		protected:
			struct COnTerminate
			{
				COnTerminate() = default;
				COnTerminate(COnTerminate const &) = delete;
				COnTerminate(COnTerminate &&) = default;
				
				NFunction::TCFunctionMutable<void ()> m_fOnTerminate;
				
				bool operator < (COnTerminate const &_Other) const
				{
					return this < &_Other;
				}
			};
		
			// Alignment zone 1: 
			// vptr
			// refcount
			NPtr::TCSharedPointer<ICDistributedActorData> mp_pDistributedActorData;
			NContainer::TCSet<COnTerminate> mp_OnTerminate;
			
			// Alignment zone 2
			CConcurrentRunQueue mp_ConcurrentRunQueue;
			CConcurrencyManager *mp_pConcurrencyManager;
			EPriority mp_Priority:1;
			mint mp_bImmediateDelete:1;
			mint mp_iFixedCore:sizeof(mint)*8 - 2;
			NPtr::TCUniquePointer<CActor, NMem::CAllocator_Placement> mp_pActor;
			mutable NAtomic::TCAtomic<smint> mp_bDestroyed;

			// Alignment zone 3
			align_cacheline NAtomic::TCAtomic<mint> mp_Working;
			
			// Alignment zone 4: Actor storage, other actor holder data
		};

		class CDefaultActorHolder : public CActorHolder
		{
		public:
			CDefaultActorHolder
				(
					CConcurrencyManager *_pConcurrencyManager
					, bool _bImmediateDelete
					, EPriority _Priority
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				)
			;
			~CDefaultActorHolder();
			
		protected:
			void fp_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame) override;
		};

		class CDispatchingActorHolder : public CDefaultActorHolder
		{
		public:
			CDispatchingActorHolder
				(
					CConcurrencyManager *_pConcurrencyManager
					, bool _bImmediateDelete
					, EPriority _Priority
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
					, NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> &&_Dispatcher
				)
			;
			
		protected:
			void fp_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame) override;

			NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> m_Dispatcher;
		};

		class CDelegatedActorHolder : public CDefaultActorHolder
		{
		public:
			CDelegatedActorHolder
				(
					CConcurrencyManager *_pConcurrencyManager
					, bool _bImmediateDelete
					, EPriority _Priority
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
					, TCActor<> const &_DelegateToActor
				)
			;
			~CDelegatedActorHolder();
			
		protected:
			void fp_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame) override;
			
			TCActorHolderWeakPointer<CActorHolder> mp_pDelegateTo;
			COnTerminate *mp_pOnTerminateEntry = nullptr;
		};

		class CSeparateThreadActorHolder : public CDefaultActorHolder
		{
		public:
			CSeparateThreadActorHolder
				(
					CConcurrencyManager *_pConcurrencyManager
					, bool _bImmediateDelete
					, EPriority _Priority
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
					, NStr::CStr const &_ThreadName
				)
			;
			~CSeparateThreadActorHolder();

		protected:
			void fp_StartQueueProcessing() override;
			void fp_DestroyThreaded() override;
			void fp_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame) override;
			
			NPtr::TCUniquePointer<NThread::CThreadObject> m_pThread;
			NStr::CStr mp_ThreadName;
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
