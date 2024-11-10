// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/OSMainRunLoop>

namespace NMib::NConcurrency
{
	COSMainRunLoop::COSMainRunLoop()
	{
#if defined(DPlatformFamily_macOS)
#else
		DMibPDebugBreak; // Not implemented
#endif

#if defined(DPlatformFamily_macOS)
		mp_RunLoopRef = CFRunLoopGetCurrent();
		CFRunLoopAddSource(mp_RunLoopRef, mp_pRunLoopSourceRef, kCFRunLoopDefaultMode);
#endif
	}

	COSMainRunLoop::~COSMainRunLoop()
	{
#if defined(DPlatformFamily_macOS)
		CFRunLoopRemoveSource(mp_RunLoopRef, mp_pRunLoopSourceRef, kCFRunLoopDefaultMode);
		CFRelease(mp_pRunLoopSourceRef);
		mp_RunLoopRef = nullptr;
#endif
	}

	void COSMainRunLoop::f_Process()
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(!mp_bProcessing); // Recursive processing is not safe
		mp_bProcessing = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				mp_bProcessing = false;
			}
		;
#endif

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		bool bDoneSomething = true;
		while (bDoneSomething)
		{
			bDoneSomething = false;

			if (mp_RunQueue.f_TransferThreadSafeQueue(mp_RunQueueLocal))
				bDoneSomething = true;

			while (auto pEntry = mp_RunQueueLocal.m_LocalQueue.f_GetFirst())
			{
				pEntry->m_Link.f_UnsafeUnlink();
				pEntry->f_Call(ThreadLocal);
				bDoneSomething = true;
			}
		}
	}

	void COSMainRunLoop::f_WaitOnce()
	{
		f_Process();

		if (!mp_bPendingWake.f_Exchange(false))
		{
#if defined(DPlatformFamily_macOS)
			CFRunLoopRun();
#endif
		}
	}

	bool COSMainRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		f_Process();

		if (!mp_bPendingWake.f_Exchange(false))
		{
#if defined(DPlatformFamily_macOS)
			return CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), false) == kCFRunLoopRunTimedOut;
#endif
		}
		return false;
	}

	void COSMainRunLoop::f_Wake()
	{
		mp_bPendingWake = true;
#if defined(DPlatformFamily_macOS)
		CFRunLoopSourceSignal(mp_pRunLoopSourceRef);
		CFRunLoopWakeUp(mp_RunLoopRef);
#endif
	}

	CActorDestroyEventLoop COSMainRunLoop::f_ActorDestroyLoop()
	{
		auto pThis = NStorage::TCSharedPointer<COSMainRunLoop>(fg_Explicit(this));
		return
			{
				[pThis]
				{
					pThis->f_WaitOnce();
				}
				,
				[pThis]
				{
					pThis->f_Wake();
				}
			}
		;
	}

	NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> COSMainRunLoop::f_Dispatcher()
	{
		return [pThis = NStorage::TCSharedPointer<COSMainRunLoop>(fg_Explicit(this))](FActorQueueDispatchNoAlloc &&_Dispatch)
			{
				pThis->mp_RunQueue.f_AddToQueue(fg_Move(_Dispatch));
#if defined(DPlatformFamily_macOS)
				CFRunLoopStop(pThis->mp_RunLoopRef);
#endif
			}
		;
	}
}
