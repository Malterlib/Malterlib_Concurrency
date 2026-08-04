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
	enum class EOSMainRunLoopMode : uint8
	{
		mc_RunLoop // Pumps the OS run loop only

		// Also dequeues and routes application events (on macOS through NSApp) once the
		// application object exists, so windows receive input without the application run loop
		// owning the thread
		, mc_ApplicationEvents
	};

	struct COSMainRunLoop : public CRunLoop
	{
		COSMainRunLoop(EOSMainRunLoopMode _Mode = EOSMainRunLoopMode::mc_RunLoop);
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
