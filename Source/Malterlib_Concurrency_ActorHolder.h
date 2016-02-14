// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NConcurrency
	{
		class CConcurrentRunQueue
		{
		public:
			CConcurrentRunQueue();
			~CConcurrentRunQueue();
			void f_AddToQueue(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			void f_AddToQueueLocal(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> *f_FirstQueueEntry();
			void f_PopFirstQueueEntry();
			bool f_TransferThreadSafeQueue();
			bool f_IsEmpty();
		private:
 			struct CQueueEntry
			{
				align_cacheline NAtomic::TCAtomic<CQueueEntry *> m_pNextQueued;
				NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> m_fToCall;
			};	
			
			align_cacheline NAtomic::TCAtomic<CQueueEntry *> mp_pFirstQueued;
			NContainer::TCLinkedList<NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag>> mp_LocalQueue;
		};
		
		class CActorHolder : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
		{
			typedef NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer> CSuper;
			friend class CConcurrencyManager;
			friend class CActor;
		protected:
			bool fp_AddToQueue(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) pure;
			virtual void fp_Construct();
		public:
			
			CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority);
			virtual ~CActorHolder();

			void f_RunProcess();
			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			bool f_ImmediateDelete() const;
			void f_DestroyThreaded();
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
			
			inline_always CConcurrencyManager &f_ConcurrencyManager() const
			{
				return *mp_pConcurrencyManager;
			}
			
		private:
			void fp_Destroy();
			void fp_Terminate();

			bool fp_DequeueProcess(bool _bRun);
			
		private:
#ifdef DMibDebug
			DMibListLinkDS_Link(CActorHolder, m_ActorLink);
#endif
			
		protected:
			CConcurrencyManager *mp_pConcurrencyManager;
			mint mp_bImmediateDelete:1;
			mint mp_iFixedCore:sizeof(mint)*8 - 1;
			EPriority mp_Priority;
			NPtr::TCUniquePointer<CActor> mp_pActor;
			
			align_cacheline NAtomic::TCAtomic<mint> mp_Working;
			align_cacheline mutable NAtomic::TCAtomic<smint> mp_bDestroyed;
			CConcurrentRunQueue mp_ConcurrentRunQueue;
		};

		class CDefaultActorHolder : public CActorHolder
		{
		protected:
			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
			virtual void fp_Construct()	override;
		public:
			CDefaultActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority);
			~CDefaultActorHolder();

			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
		};

		class CDispatchingActorHolder : public CDefaultActorHolder
		{
		protected:
			NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> m_Dispatcher;

			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
		public:
			CDispatchingActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> const &_Dispatcher);
			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
		};

		class CSeparateThreadActorHolder : public CDefaultActorHolder
		{
		protected:
			NPtr::TCUniquePointer<NThread::CThreadObject> m_pThread;
			NStr::CStr mp_ThreadName;

			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
			void fp_Construct() override;
		public:
			CSeparateThreadActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NStr::CStr const &_ThreadName);
			~CSeparateThreadActorHolder();
			void f_DestroyThreaded();
			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
