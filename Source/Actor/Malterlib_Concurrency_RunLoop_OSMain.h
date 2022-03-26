// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/RunLoop>

#ifdef DPlatformFamily_OSX
#	include <CoreFoundation/CFRunLoop.h>
#endif

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
		NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> f_Dispatcher() override;

	private:
		align_cacheline CConcurrentRunQueue mp_RunQueue;
		align_cacheline CConcurrentRunQueue::CLocalQueueData mp_RunQueueLocal;

		NAtomic::TCAtomic<bool> mp_bPendingWake = false;
#if DMibEnableSafeCheck > 0
		bool mp_bProcessing = false;
#endif

#ifdef DPlatformFamily_OSX
		CFRunLoopRef mp_RunLoopRef = nullptr;
		CFRunLoopSourceContext mp_RunLoopSourceContext
			{
				0
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, [](void *info)
				{
				}
			}
		;
		CFRunLoopSourceRef mp_pDummyRunLoopSource = CFRunLoopSourceCreate
			(
				nullptr
				, 0
				, &mp_RunLoopSourceContext
			)
		;
#endif
	};
}
