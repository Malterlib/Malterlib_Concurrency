// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorHolder_OSThreadRunLoop.h"

#if defined(DPlatformFamily_macOS)
#	include <Mib/Core/PlatformSpecific/PosixErrNo>
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
		return mp_pRunLoopRef;
	}
#endif

	void COSThreadRunLoopActorHolder::fp_StartQueueProcessing()
	{
		{
			DMibLock(mp_ThreadLock);
			mp_pThread = NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						{
							DMibLock(mp_ThreadLock);
						}
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
									CFRunLoopStop(This.mp_pRunLoopRef);
								}
							}
						;

						CFRunLoopSourceRef pRunLoopSource = CFRunLoopSourceCreate(nullptr, 0, &RunLoopSourceContext);

						mp_pRunLoopRef = CFRunLoopGetCurrent();
						CFRunLoopAddSource(mp_pRunLoopRef, pRunLoopSource, kCFRunLoopDefaultMode);
						mp_pRunLoopSourceRef = pRunLoopSource;

						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibLock(mp_RunLoopDestroyLock);
								CFRunLoopRemoveSource(mp_pRunLoopRef, pRunLoopSource, kCFRunLoopDefaultMode);
								CFRelease(pRunLoopSource);
								mp_pRunLoopRef = nullptr;
								mp_pRunLoopSourceRef = nullptr;
							}
						;

						mp_ThreadStartedEvent.f_SetSignaled();

						fp_RunProcess(fg_ConcurrencyThreadLocal());

						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
							CFRunLoopRun();
#else
						mp_ThreadStartedEvent.f_SetSignaled();
#endif
						return 0;
					}
					, mp_ThreadName
					, f_ConcurrencyManager().f_GetExecutionPriority(f_GetPriority())
				)
			;
		}

		mp_ThreadStartedEvent.f_Wait();
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_fFunctor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		TCActorHolderWeakPointer<COSThreadRunLoopActorHolder> pThisReference = fg_Explicit(this); // Keep memory alive
		DMibLock(mp_ThreadLock);
		if (fp_AddToQueue(fg_Move(_fFunctor), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			DMibFastCheck(mp_pRunLoopRef);
			auto *pThis = this;
			CFRunLoopPerformBlock
				(
					mp_pRunLoopRef
					, kCFRunLoopDefaultMode
					, ^()
					{
						(void)pThisReference;
						if (pThis->mp_Destroyed.f_Load() >= 4)
							return;
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
			CFRunLoopWakeUp(mp_pRunLoopRef);
#endif
		}
	}

	void COSThreadRunLoopActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
#if defined(DPlatformFamily_macOS)
		DMibFastCheck(mp_pRunLoopRef);
		TCActorHolderSharedPointer<COSThreadRunLoopActorHolder> pThis = fg_Explicit(this);
		CFRunLoopPerformBlock
			(
				mp_pRunLoopRef
				, kCFRunLoopDefaultMode
				, ^()
				{
					pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
				}
			)
		;
		CFRunLoopWakeUp(mp_pRunLoopRef);
#endif
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcess(FActorQueueDispatch &&_fFunctor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_fFunctor), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			DMibFastCheck(mp_pRunLoopRef);
			TCActorHolderSharedPointer<COSThreadRunLoopActorHolder> pThis = fg_Explicit(this);
			CFRunLoopPerformBlock
				(
					mp_pRunLoopRef
					, kCFRunLoopDefaultMode
					, ^()
					{
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
			CFRunLoopWakeUp(mp_pRunLoopRef);
#endif
		}
	}

	void COSThreadRunLoopActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Entry), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			DMibFastCheck(mp_pRunLoopRef);
			TCActorHolderSharedPointer<COSThreadRunLoopActorHolder> pThis = fg_Explicit(this);
			CFRunLoopPerformBlock
				(
					mp_pRunLoopRef
					, kCFRunLoopDefaultMode
					, ^()
					{
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
			CFRunLoopWakeUp(mp_pRunLoopRef);
#endif
		}
	}

	void COSThreadRunLoopActorHolder::fp_DestroyThreaded()
	{
		{
			DMibLock(mp_ThreadLock);
			mp_pThread->f_Stop(false);
#if defined(DPlatformFamily_macOS)
			{
				DMibLock(mp_RunLoopDestroyLock);
				if (mp_pRunLoopRef)
				{
					CFRunLoopSourceSignal(mp_pRunLoopSourceRef);
					CFRunLoopWakeUp(mp_pRunLoopRef);
				}
			}
#endif
			mp_Destroyed.f_Exchange(4);
			mp_pThread.f_Clear();
		}

		CDefaultActorHolder::fp_DestroyThreaded();
	}
}
