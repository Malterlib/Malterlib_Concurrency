// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NConcurrency
	{
		class CActorHolder : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
		{
			typedef NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer> CSuper;
			friend class CConcurrencyManager;
			friend class CActor;
		protected:
			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) pure;
			virtual void fp_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) pure;
			virtual bool fp_DequeueProcess(bool _bRun) pure;
			virtual bool fp_DequeueProcessConcurrent(bool _bRun) pure;
			virtual bool fp_QueueProcessIsEmpty() const pure;
			virtual void fp_Construct();
		public:

			virtual bool f_Concurrent() pure;
			
			CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete);
			virtual ~CActorHolder();

			void f_RunProcess();
			void f_RunProcessConcurrent();
			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			void f_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			bool f_DequeueProcess(bool _bRun);
			bool f_DequeueProcessConcurrent(bool _bRun);
			bool f_QueueProcessIsEmpty() const;
			bool f_ImmediateDelete() const;
			void f_DestroyThreaded();
			bool f_IsDestroyed() const;
			void f_Destroy();
			void f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop = CActorDestroyEventLoop());
			aint f_RefCountDecrease();

			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> const &_ResultCall);
			
			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(tf_CActor &&_Actor, tf_CFunctor &&_Functor);
			
			template <typename tf_CActor, typename tf_CFunctor>
			void f_Destroy(tf_CActor *_pActor, tf_CFunctor &&_Functor);
			
		private:
			void fp_Destroy();
			void fp_Terminate();
			
		private:
			DMibListLinkDS_Link(CActorHolder, m_ActorLink);
			
		protected:
			CConcurrencyManager *m_pConcurrencyManager;
			bool mp_bImmediateDelete;
			NPtr::TCUniquePointer<CActor> mp_pActor;
			NThread::CMutual m_WorkLock;
			NThread::CMutualManyRead m_DeleteLock;
			mutable NAtomic::TCAtomic<smint> m_bDestroyed;
		};

		class CDefaultActorHolder : public CActorHolder
		{
		protected:
			NContainer::TCThreadSafeQueue<NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag>> m_ActorQueue;
			NContainer::TCThreadSafeQueue<NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag>> m_ActorQueueConcurrent;

			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
			virtual void fp_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
			virtual bool fp_DequeueProcess(bool _bRun) override;
			virtual bool fp_DequeueProcessConcurrent(bool _bRun) override;
			virtual bool fp_QueueProcessIsEmpty() const override;
			virtual void fp_Construct()	override;
		public:
			CDefaultActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete);

			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			void f_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
			bool f_DequeueProcess(bool _bRun);
			bool f_DequeueProcessConcurrent(bool _bRun);
			bool f_QueueProcessIsEmpty() const;
		};

		class CDispatchingActorHolder : public CDefaultActorHolder
		{
		protected:
			NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> m_Dispatcher;

			virtual void fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor) override;
		public:
			CDispatchingActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> const &_Dispatcher);
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
			CSeparateThreadActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, NStr::CStr const &_ThreadName);
			~CSeparateThreadActorHolder();
			void f_DestroyThreaded();
			void f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor);
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
