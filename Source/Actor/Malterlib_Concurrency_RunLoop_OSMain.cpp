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
	struct NPrivate::COSMainRunLoopWakeState
	{
		COSMainRunLoopWakeState();
		~COSMainRunLoopWakeState();

		void f_Wake();

#ifdef DPlatformFamily_macOS
		CFRunLoopRef m_RunLoopRef = nullptr;
		CFRunLoopSourceRef m_pRunLoopSourceRef = nullptr;
#endif
	};

	NPrivate::COSMainRunLoopWakeState::COSMainRunLoopWakeState()
	{
#if defined(DPlatformFamily_macOS)
#else
		DMibPDebugBreak; // Not implemented
#endif

#if defined(DPlatformFamily_macOS)
		m_RunLoopRef = CFRunLoopGetCurrent();
		CFRetain(m_RunLoopRef);

		// Source equality uses context identity. Keep a distinct retained context
		// for each wrapper, even when several wrappers use the same native run loop.
		auto ContextInfo = CFArrayCreateMutable(nullptr, 1, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(ContextInfo, m_RunLoopRef);
		CFRunLoopSourceContext Context
			{
				0
				, ContextInfo
				, [](void const *_pInfo)
				{
					return CFRetain(_pInfo);
				}
				, [](void const *_pInfo)
				{
					CFRelease(_pInfo);
				}
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, nullptr
				, [](void *_pInfo)
				{
					auto Value = CFArrayGetValueAtIndex(static_cast<CFArrayRef>(_pInfo), 0);
					CFRunLoopStop(static_cast<CFRunLoopRef>(const_cast<void *>(Value)));
				}
			}
		;
		m_pRunLoopSourceRef = CFRunLoopSourceCreate(nullptr, 0, &Context);
		CFRelease(ContextInfo);
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
#ifdef DPlatformFamily_macOS
		CFRunLoopSourceSignal(m_pRunLoopSourceRef);
		CFRunLoopWakeUp(m_RunLoopRef);
#endif
	}

	COSMainRunLoop::COSMainRunLoop()
		: mp_pWakeState(fg_Construct())
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
		CFRunLoopRun();
#endif
	}

	bool COSMainRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		f_Process();

#if defined(DPlatformFamily_macOS)
		// A handled source must win over the deadline, including a zero-duration poll.
		return CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), true) == kCFRunLoopRunTimedOut;
#else
		return false;
#endif
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
				pThis->f_Wake();
			}
		;
	}
}
