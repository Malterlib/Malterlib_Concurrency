// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	struct CRunLoop
	{
		virtual ~CRunLoop();
		virtual void f_Process() = 0;
		virtual void f_WaitOnce() = 0;
		virtual bool f_WaitOnceTimeout(fp64 _Timeout) = 0;
		virtual void f_Wake() = 0;
		virtual CActorDestroyEventLoop f_ActorDestroyLoop() = 0;
		virtual NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> f_Dispatcher() = 0;
		void f_Sleep(fp64 _Time);

		NStorage::CIntrusiveRefCount m_RefCount;
	};

	struct CDefaultRunLoop final : public CRunLoop
	{
		void f_Process() override;
		void f_WaitOnce() override;
		bool f_WaitOnceTimeout(fp64 _Timeout) override;
		void f_Wake() override;
		CActorDestroyEventLoop f_ActorDestroyLoop() override;
		NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> f_Dispatcher() override;

		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc m_RunQueue;
		NThread::CEventAutoReset m_Event;
		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData m_RunQueueLocal;
#if DMibEnableSafeCheck > 0
		bool m_bProcessing = false;
#endif
	};

	struct CActorRunLoopHelper
	{
		~CActorRunLoopHelper();

		NStorage::TCSharedPointer<CDefaultRunLoop> m_pRunLoop = fg_Construct();
		TCActor<CDispatchingActor> m_HelperActor{fg_Construct(), m_pRunLoop->f_Dispatcher()};
		NStorage::TCOptional<CCurrentActorScope> m_CurrentActor{fg_Construct(fg_ConcurrencyThreadLocal(), m_HelperActor)};
	};
}
