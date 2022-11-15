// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorHolder_OSThreadRunLoop.h"

#if defined(DPlatformFamily_macOS)
#include <Mib/Core/PlatformSpecific/PosixErrNo>
#endif

namespace NMib::NConcurrency
{
	COSThreadRunLoopActorHolder::COSThreadRunLoopActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			, NStr::CStr const &_ThreadName
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		, mp_ThreadName(_ThreadName)
	{
#if defined(DPlatformFamily_macOS)
#else
		DMibPDebugBreak; // Not implemented
#endif
	}

	COSThreadRunLoopActorHolder::~COSThreadRunLoopActorHolder()
	{
		DMibRequire(mp_pThread.f_IsEmpty());
	}

#if defined(DPlatformFamily_macOS)
	CFRunLoopRef const &COSThreadRunLoopActorHolder::f_GetRunLoop() const
	{
		return mp_RunLoopRef;
	}
#endif

	void COSThreadRunLoopActorHolder::fp_StartQueueProcessing()
	{
		mp_pThread = NThread::CThreadObject::fs_StartThread
			(
				[this](NThread::CThreadObject *_pThread) -> aint
				{
					(void)this;
#if defined(DPlatformFamily_macOS)
					CFRunLoopSourceContext RunLoopSourceContext
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
								auto &This = *((COSThreadRunLoopActorHolder *)_pThis);
								CFRunLoopStop(This.mp_RunLoopRef);
							}
						}
					;

					CFRunLoopSourceRef pRunLoopSource = CFRunLoopSourceCreate
						(
							nullptr
							, 0
							, &RunLoopSourceContext
						)
					;

					mp_RunLoopRef = CFRunLoopGetCurrent();
					CFRunLoopAddSource(mp_RunLoopRef, pRunLoopSource, kCFRunLoopDefaultMode);
					mp_RunLoopSourceRef = pRunLoopSource;

					auto Cleanup = g_OnScopeExit / [&]
						{
							CFRunLoopRemoveSource(mp_RunLoopRef, pRunLoopSource, kCFRunLoopDefaultMode);
							CFRelease(pRunLoopSource);
							mp_RunLoopRef = nullptr;
							mp_RunLoopSourceRef = nullptr;
						}
					;

					mp_ThreadStartedEvent.f_SetSignaled();

					fp_RunProcess();

					while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
						CFRunLoopRun();

#endif
					return 0;
				}
				, mp_ThreadName
				, f_ConcurrencyManager().f_GetExecutionPriority(f_GetPriority())
			)
		;

		mp_ThreadStartedEvent.f_Wait();
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcess()
	{
#if defined(DPlatformFamily_macOS)
		DMibFastCheck(mp_RunLoopRef);
		auto pThis = this;
		CFRunLoopPerformBlock
			(
				mp_RunLoopRef
				, kCFRunLoopDefaultMode
				, ^()
				{
					NTime::CCyclesClock Clock;
					Clock.f_Start();
					while (true)
					{
						pThis->fp_RunProcess();
						if (Clock.f_GetTime() > 0.000035) // Run for at least 35 µs
							break;
					}
				}
			)
		;
		CFRunLoopWakeUp(mp_RunLoopRef);
#endif
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
			fp_QueueProcess();
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		// Reference this so it doesn't go out of scope if queue is processed before thread has been notified
		TCActorHolderSharedPointer<COSThreadRunLoopActorHolder> pThis = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
			fp_QueueProcess();
	}

	void COSThreadRunLoopActorHolder::fp_DestroyThreaded()
	{
		mp_pThread->f_Stop(false);
#if defined(DPlatformFamily_macOS)
		{
			if (mp_RunLoopRef)
			{
				CFRunLoopSourceSignal(mp_RunLoopSourceRef);
				CFRunLoopWakeUp(mp_RunLoopRef);
			}
		}
#endif
		mp_pThread.f_Clear();

		CDefaultActorHolder::fp_DestroyThreaded();
	}
}
