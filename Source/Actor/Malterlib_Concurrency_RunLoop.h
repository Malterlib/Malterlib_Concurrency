// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CRunLoop : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		virtual ~CRunLoop();
		virtual void f_Process() = 0;
		virtual void f_WaitOnce() = 0;
		virtual bool f_WaitOnceTimeout(fp64 _Timeout) = 0;
		virtual void f_Wake() = 0;
		virtual CActorDestroyEventLoop f_ActorDestroyLoop() = 0;
		virtual NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> f_Dispatcher() = 0;
	};

	struct CDefaultRunLoop : public CRunLoop
	{
		void f_Process() override;
		void f_WaitOnce() override;
		bool f_WaitOnceTimeout(fp64 _Timeout) override;
		void f_Wake() override;
		CActorDestroyEventLoop f_ActorDestroyLoop() override;
		NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> f_Dispatcher() override;

		align_cacheline CConcurrentRunQueue m_RunQueue;
		NThread::CEventAutoReset m_Event;
		align_cacheline CConcurrentRunQueue::CLocalQueueData m_RunQueueLocal;
#if DMibEnableSafeCheck > 0
		bool m_bProcessing = false;
#endif
	};
}
