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
		bool fg_OSMainRunLoop_WaitApplicationEvent(fp64 _Timeout);
		void fg_OSMainRunLoop_WakeApplication();
		void *fg_OSMainRunLoop_PushAutoreleasePool();
		void fg_OSMainRunLoop_PopAutoreleasePool(void *_pPool);
	}
#endif

	namespace NPrivate
	{
		// The source callback and wake-state destruction may run on different threads; CF context callbacks retain this state.
		struct COSMainRunLoopWakeSource
		{
			NStorage::CIntrusiveRefCount m_RefCount;

			NAtomic::TCAtomic<bool> m_bPendingWake = false; // Set until a pending wake is consumed, including through another wrapper of the native run loop.

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

		// Each wrapper needs a distinct context because CF source equality uses context identity.
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

					// Any wrapper can consume this source; clear the flag here so the next wake signals again.
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
		// An unconsumed wake already prevents blocking; coalesce further signals.
		if (m_pSource->m_bPendingWake.f_Exchange(true))
			return;

#ifdef DPlatformFamily_macOS
		CFRunLoopSourceSignal(m_pRunLoopSourceRef);
		CFRunLoopWakeUp(m_RunLoopRef);

		if (m_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
			NPrivate::fg_OSMainRunLoop_WakeApplication();
#endif
	}

	namespace
	{
		// One autorelease pool per pass, as -[NSApplication run] drains one per event. The pool that is
		// current when the loop starts never drains while it runs, so the actor work and the event
		// dispatch of a pass must not autorelease into it
		struct CApplicationEventPassPool
		{
			explicit CApplicationEventPassPool(EOSMainRunLoopMode _Mode);
			~CApplicationEventPassPool();

			void *m_pPool = nullptr;
		};

		CApplicationEventPassPool::CApplicationEventPassPool(EOSMainRunLoopMode _Mode)
		{
#if defined(DPlatformFamily_macOS)
			if (_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
				m_pPool = NPrivate::fg_OSMainRunLoop_PushAutoreleasePool();
#endif
		}

		CApplicationEventPassPool::~CApplicationEventPassPool()
		{
#if defined(DPlatformFamily_macOS)
			if (m_pPool)
				NPrivate::fg_OSMainRunLoop_PopAutoreleasePool(m_pPool);
#endif
		}
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
		CApplicationEventPassPool PassPool(mp_pWakeState->m_Mode);
		f_Process();

#if defined(DPlatformFamily_macOS)
		if (mp_pWakeState->m_Mode == EOSMainRunLoopMode::mc_ApplicationEvents)
			NPrivate::fg_OSMainRunLoop_WaitApplicationEvent(-1.0);
		else
			CFRunLoopRun();
#endif

		mp_pWakeState->m_pSource->m_bPendingWake = false;
	}

	bool COSMainRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		CApplicationEventPassPool PassPool(mp_pWakeState->m_Mode);
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

				// An application-event wait needs a posted event; stopping the run loop alone does not return it.
				pThis->f_Wake();
			}
		;
	}
}
