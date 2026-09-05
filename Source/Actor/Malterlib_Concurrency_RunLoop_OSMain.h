// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/RunLoop>

namespace NMib::NConcurrency::NPrivate
{
	struct COSMainRunLoopWakeState;
}

DMibDefineSharedPointerType(NMib::NConcurrency::NPrivate::COSMainRunLoopWakeState, false, false);

namespace NMib::NConcurrency
{
	struct COSMainRunLoop : public CRunLoop
	{
		COSMainRunLoop();
		~COSMainRunLoop();

		void f_Process() override;
		void f_WaitOnce() override;
		bool f_WaitOnceTimeout(fp64 _Timeout) override;
		void f_Wake() override;
		CActorDestroyEventLoop f_ActorDestroyLoop() override;
		NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> f_Dispatcher() override;

	private:
		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc mp_RunQueue;
		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData mp_RunQueueLocal;

		NStorage::TCSharedPointer<NPrivate::COSMainRunLoopWakeState> mp_pWakeState;
#if DMibEnableSafeCheck > 0
		bool mp_bProcessing = false;
#endif
	};
}
