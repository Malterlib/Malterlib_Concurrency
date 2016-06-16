// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Memory/Allocators/Placement>

namespace NMib
{
	namespace NConcurrency
	{
		constexpr static const mint gc_ActorQueueDispatchFunctionMemory = fg_AlignUpConstExpr(sizeof(void*)*8*2, DMibPMemoryCacheLineSize);
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
			void f_AddToQueueLocal(FActorQueueDispatch &&_Functor);
			void f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor);
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
					DMibListLinkDSA_Member(m_Link);
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
		};
		
		class CActorHolder : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
		{
			typedef NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer> CSuper;
			friend class CConcurrencyManager;
			friend class CActor;
		protected:
			bool fp_AddToQueue(FActorQueueDispatch &&_Functor);
			virtual void fp_Construct();
		public:
			
			CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);
			virtual ~CActorHolder();

			void f_RunProcess();
			virtual void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false) pure;
			bool f_ImmediateDelete() const;
			virtual void f_DestroyThreaded();
			bool f_IsDestroyed() const;
			void f_Destroy();
			void f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop = CActorDestroyEventLoop());
			aint f_RefCountDecrease();

			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> &&_ResultCall);
			
			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(tf_CActor &&_Actor, tf_CFunctor &&_Functor);
			
			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(tf_CActor *_pActor, tf_CFunctor &&_Functor);
			
			template <typename tf_CFunctor>
			void f_Destroy(tf_CFunctor &&_Functor);
			
			void f_SetFixedCore(mint _iFixedCore);
			
			inline_always CConcurrencyManager &f_ConcurrencyManager() const;
			inline_always EPriority f_GetPriority() const;
			
			NPtr::TCSharedPointer<ICDistributedActorData> &f_GetDistributedActorData();
			NPtr::TCSharedPointer<ICDistributedActorData> const &f_GetDistributedActorData() const;
			
		private:
			void fp_Destroy();
			void fp_Terminate();

			bool fp_DequeueProcess(bool _bRun);
			
		private:
#ifdef DMibDebug
			DMibListLinkDS_Link(CActorHolder, m_ActorLink);
		public:
			NStr::CStr m_ActorTypeName;
#endif
			
		protected:
			// Alignment zone 1: 
			// vptr
			// refcount
			NPtr::TCSharedPointer<ICDistributedActorData> mp_pDistributedActorData; 
			
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
		protected:
			void fp_Construct()	override;
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

			void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false) override;
		};

		class CDispatchingActorHolder : public CDefaultActorHolder
		{
		protected:
			NFunction::TCFunction<void (FActorQueueDispatch &&_Dispatch)> m_Dispatcher;
		public:
			CDispatchingActorHolder
				(
					CConcurrencyManager *_pConcurrencyManager
					, bool _bImmediateDelete
					, EPriority _Priority
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
					, NFunction::TCFunction<void (FActorQueueDispatch &&_Dispatch)> const &_Dispatcher
				)
			;
			void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false) override;
		};

		class CSeparateThreadActorHolder : public CDefaultActorHolder
		{
		protected:
			NPtr::TCUniquePointer<NThread::CThreadObject> m_pThread;
			NStr::CStr mp_ThreadName;

			void fp_Construct() override;
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
			void f_DestroyThreaded() override;
			void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false) override;
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
