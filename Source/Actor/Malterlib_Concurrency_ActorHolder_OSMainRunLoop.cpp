// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorHolder_OSMainRunLoop.h"

#if defined(DPlatformFamily_macOS)
#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <dispatch/dispatch.h>
#endif

namespace NMib::NConcurrency
{
	COSMainRunLoopActorHolder::COSMainRunLoopActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
#if defined(DPlatformFamily_macOS)
#else
		DMibPDebugBreak; // Not implemented
#endif
	}

	COSMainRunLoopActorHolder::~COSMainRunLoopActorHolder()
	{
	}

	void COSMainRunLoopActorHolder::fp_StartQueueProcessing()
	{
	}

	void COSMainRunLoopActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		TCActorHolderWeakPointer<COSMainRunLoopActorHolder> pThisReference = fg_Explicit(this); // Keep memory alive
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			auto *pThis = this;
			dispatch_async
				(
					dispatch_get_main_queue()
					, ^()
					{
						(void)pThisReference;
						if (pThis->mp_bDestroyed.f_Load() >= 4)
							return;
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
#endif
		}
	}

	void COSMainRunLoopActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
#if defined(DPlatformFamily_macOS)
		TCActorHolderSharedPointer<COSMainRunLoopActorHolder> pThis = fg_Explicit(this);
		dispatch_async
			(
				dispatch_get_main_queue()
				, ^()
				{
					pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
				}
			)
		;
#endif
	}

	void COSMainRunLoopActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			TCActorHolderSharedPointer<COSMainRunLoopActorHolder> pThis = fg_Explicit(this);
			dispatch_async
				(
					dispatch_get_main_queue()
					, ^()
					{
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
#endif
		}
	}

	void COSMainRunLoopActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Entry), _ThreadLocal))
		{
#if defined(DPlatformFamily_macOS)
			TCActorHolderSharedPointer<COSMainRunLoopActorHolder> pThis = fg_Explicit(this);
			dispatch_async
				(
					dispatch_get_main_queue()
					, ^()
					{
						pThis->fp_RunProcess(fg_ConcurrencyThreadLocal());
					}
				)
			;
#endif
		}
	}



	void COSMainRunLoopActorHolder::fp_DestroyThreaded()
	{
		mp_bDestroyed.f_Exchange(4);
		CDefaultActorHolder::fp_DestroyThreaded();
	}
}
