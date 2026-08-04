// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/OSMainRunLoop>

#ifdef DPlatformFamily_macOS
#	include <CoreFoundation/CFArray.h>
#	include <CoreFoundation/CFRunLoop.h>
#endif

namespace NMib::NConcurrency
{
#if defined(DPlatformFamily_macOS)
	namespace NPrivate
	{
		// Implemented in Malterlib_Concurrency_RunLoop_OSMain_MacOS.mm; a negative timeout waits
		// indefinitely, the return value is true when the wait timed out
		bool fg_OSMainRunLoop_WaitApplicationEvent(fp64 _Timeout);
		void fg_OSMainRunLoop_WakeApplication();
	}
#endif

	namespace NPrivate
	{
		// Shared between a wake state and its run loop source. The source performs on the run
		// loop thread while the wake state may be released on another, so the source keeps this
		// alive through its context's retain and release callbacks and touches nothing else
		struct COSMainRunLoopWakeSource
		{
			NStorage::CIntrusiveRefCount m_RefCount;

			// Set by a wake and cleared once the signal it sent has been consumed: while it is
			// set a signal is pending or about to be delivered, so further wakes need not signal
			// again. Several wrappers can share one native run loop, so a wait through another
			// wrapper consumes this source's signal too; the perform is the one point common to
			// every such wait, and clears the flag as well as the waiter itself
			NAtomic::TCAtomic<bool> m_bPendingWake = false;

#ifdef DPlatformFamily_macOS
			CFRunLoopRef m_RunLoopRef = nullptr;
#endif
		};
	}

	struct NPrivate::COSMainRunLoopWakeState
	{
		COSMainRunLoopWakeState(EOSMainRunLoopMode _Mode);
		~COSMainRunLoopWakeState();

		void f_Wake();

		EOSMainRunLoopMode m_Mode;
		NStorage::TCSharedPointer<COSMainRunLoopWakeSource> m_pSource = fg_Construct();

#ifdef DPlatformFamily_macOS
		CFRunLoopRef m_RunLoopRef = nullptr;
		CFRunLoopSourceRef m_pRunLoopSourceRef = nullptr;
#endif
	};

	NPrivate::COSMainRunLoopWakeState::COSMainRunLoopWakeState(EOSMainRunLoopMode _Mode)
		: m_Mode(_Mode)
	{
#if defined(DPlatformFamily_macOS)
#else
		DMibPDebugBreak; // Not implemented
#endif

#if defined(DPlatformFamily_macOS)
		m_RunLoopRef = CFRunLoopGetCurrent();
		CFRetain(m_RunLoopRef);
		m_pSource->m_RunLoopRef = m_RunLoopRef;

		// Source equality uses context identity, so each wrapper gets a context of its own even
		// when several wrappers use the same native run loop. The source takes its own reference
		// to the shared wake source through the retain and release callbacks
		CFRunLoopSourceContext Context
			{
				0
				, m_pSource.f_Get()
				, [](void const *_pInfo) -> void const *
				{
					auto *pSource = (COSMainRunLoopWakeSource *)_pInfo;
					return NStorage::TCSharedPointer<COSMainRunLoopWakeSource>(fg_Explicit(pSource)).f_Detach();
				}
				, [](void const *_pInfo)
				{
					auto *pSource = (COSMainRunLoopWakeSource *)_pInfo;
					NStorage::TCSharedPointer<COSMainRunLoopWakeSource>(fg_Attach(pSource));
				}
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, [](void *_pInfo)
				{
					auto &Source = *(COSMainRunLoopWakeSource *)_pInfo;

					// Whichever wrapper's wait runs the loop, this perform is the signal being
					// consumed, so the next wake must signal again
					Source.m_bPendingWake = false;
					CFRunLoopStop(Source.m_RunLoopRef);
				}
			}
		;
		m_pRunLoopSourceRef = CFRunLoopSourceCreate(nullptr, 0, &Context);
		CFRunLoopAddSource(m_RunLoopRef, m_pRunLoopSourceRef, kCFRunLoopDefaultMode);
#endif
	}

	NPrivate::COSMainRunLoopWakeState::~COSMainRunLoopWakeState()
	{
#if defined(DPlatformFamily_macOS)
		CFRunLoopRemoveSource(m_RunLoopRef, m_pRunLoopSourceRef, kCFRunLoopDefaultMode);
		CFRelease(m_pRunLoopSourceRef);
		CFRelease(m_RunLoopRef);
#endif
	}

	void NPrivate::COSMainRunLoopWakeState::f_Wake()
	{
		// Coalesce: an unconsumed wake already guarantees the waiter will not block, so repeated
		// wakes need no further signaling (posting application events in particular is not cheap)
		if (m_pSource->m_bPendingWake.f_Exchange(true))
			return;

#ifdef DPlatformFamily_macOS
		CFRunLoopSourceSignal(m_pRunLoopSourceRef);
		CFRunLoopWakeUp(m_RunLoopRef);

		if (m_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
			NPrivate::fg_OSMainRunLoop_WakeApplication();
#endif
	}

	COSMainRunLoop::COSMainRunLoop(EOSMainRunLoopMode _Mode)
		: mp_pWakeState(fg_Construct(_Mode))
	{
		m_RefCount.m_fWakeOnReferenceRelease = [pState = mp_pWakeState]
			{
				pState->f_Wake();
			}
		;
	}

	COSMainRunLoop::~COSMainRunLoop() = default;

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

			while (auto pEntry = mp_RunQueueLocal.f_PopFirst())
			{
				pEntry->f_Call(ThreadLocal);
				bDoneSomething = true;
			}
		}
	}

	void COSMainRunLoop::f_WaitOnce()
	{
		f_Process();

#if defined(DPlatformFamily_macOS)
		if (mp_pWakeState->m_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
			NPrivate::fg_OSMainRunLoop_WaitApplicationEvent(-1.0);
		else
			CFRunLoopRun();
#endif

		// The wait has returned, so the wake it consumed is spent and the next wake must signal
		mp_pWakeState->m_pSource->m_bPendingWake = false;
	}

	bool COSMainRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		f_Process();

		bool bTimedOut = false;
#if defined(DPlatformFamily_macOS)
		if (mp_pWakeState->m_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
			bTimedOut = NPrivate::fg_OSMainRunLoop_WaitApplicationEvent(_Timeout);
		else
		{
			// A handled source must win over the deadline, including a zero-duration poll.
			bTimedOut = CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), true) == kCFRunLoopRunTimedOut;
		}
#endif

		mp_pWakeState->m_pSource->m_bPendingWake = false;

		return bTimedOut;
	}

	void COSMainRunLoop::f_Wake()
	{
		mp_pWakeState->f_Wake();
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

				// The wake knows the mode: stopping the run loop is not enough once the wait is the
				// application's event dequeue, which only a posted event returns from
				pThis->f_Wake();
			}
		;
	}
}
