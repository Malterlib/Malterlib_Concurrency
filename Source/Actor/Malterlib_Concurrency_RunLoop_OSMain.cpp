// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/OSMainRunLoop>

namespace NMib::NConcurrency
{
	COSMainRunLoop::COSMainRunLoop()
	{
#if defined(DPlatformFamily_OSX)
#else
		DMibPDebugBreak; // Not implemented
#endif

#if defined(DPlatformFamily_OSX)
		mp_RunLoopRef = CFRunLoopGetCurrent();
		CFRunLoopAddSource(mp_RunLoopRef, mp_pDummyRunLoopSource, kCFRunLoopDefaultMode);
#endif
	}

	COSMainRunLoop::~COSMainRunLoop()
	{
#if defined(DPlatformFamily_OSX)
		CFRunLoopRemoveSource(mp_RunLoopRef, mp_pDummyRunLoopSource, kCFRunLoopDefaultMode);
		CFRelease(mp_pDummyRunLoopSource);
		mp_RunLoopRef = nullptr;
#endif
	}

	void COSMainRunLoop::f_Process()
	{
		bool bDoneSomething = true;
		while (bDoneSomething)
		{
			bDoneSomething = false;

			if (mp_RunQueue.f_TransferThreadSafeQueue(mp_RunQueueLocal))
				bDoneSomething = true;

			while (auto pEntry = mp_RunQueue.f_FirstQueueEntry(mp_RunQueueLocal))
			{
				(*pEntry)();
				mp_RunQueue.f_PopQueueEntry(pEntry);
				bDoneSomething = true;
			}
		}
	}

	void COSMainRunLoop::f_WaitOnce()
	{
		f_Process();

		if (!mp_bPendingWake.f_Exchange(false))
		{
#if defined(DPlatformFamily_OSX)
			CFRunLoopRun();
#endif
		}
	}

	bool COSMainRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		f_Process();

		if (!mp_bPendingWake.f_Exchange(false))
		{
#if defined(DPlatformFamily_OSX)
			return CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), false) == kCFRunLoopRunTimedOut;
#endif
		}
		return false;
	}

	void COSMainRunLoop::f_Wake()
	{
		mp_bPendingWake = true;
#if defined(DPlatformFamily_OSX)
		CFRunLoopStop(mp_RunLoopRef);
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

	NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> COSMainRunLoop::f_Dispatcher()
	{
		return [pThis = NStorage::TCSharedPointer<COSMainRunLoop>(fg_Explicit(this))](FActorQueueDispatch &&_Dispatch)
			{
				pThis->mp_RunQueue.f_AddToQueue(fg_Move(_Dispatch));
#if defined(DPlatformFamily_OSX)
				CFRunLoopStop(pThis->mp_RunLoopRef);
#endif
			}
		;
	}
}
