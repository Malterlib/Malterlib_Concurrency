// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/RunLoop>

#ifdef DPlatformFamily_macOS
#	include <CoreFoundation/CFRunLoop.h>
#endif

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
		EOSMainRunLoopMode mp_Mode = EOSMainRunLoopMode::mc_RunLoop;
		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc mp_RunQueue;
		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData mp_RunQueueLocal;

		NAtomic::TCAtomic<bool> mp_bPendingWake = false;
#if DMibEnableSafeCheck > 0
		bool mp_bProcessing = false;
#endif

#ifdef DPlatformFamily_macOS
		CFRunLoopRef mp_RunLoopRef = nullptr;
		CFRunLoopSourceContext mp_RunLoopSourceContext
			{
				0
				, this
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, [](void *_pThis)
				{
					auto &This = *((COSMainRunLoop *)_pThis);
					CFRunLoopStop(This.mp_RunLoopRef);
				}
			}
		;
		CFRunLoopSourceRef mp_pRunLoopSourceRef = CFRunLoopSourceCreate
			(
				nullptr
				, 0
				, &mp_RunLoopSourceContext
			)
		;
#endif
	};
}
