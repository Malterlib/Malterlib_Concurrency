// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorHolder_OSMainRunLoop.h"

#if defined(DPlatformFamily_OSX)
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
#if defined(DPlatformFamily_OSX)
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

	void COSMainRunLoopActorHolder::fp_QueueProcess()
	{
#if defined(DPlatformFamily_OSX)
		auto pThis = this;
		dispatch_async
			(
				dispatch_get_main_queue()
				, ^()
				{
					pThis->fp_RunProcess();
 				}
			)
		;
#endif
	}

	void COSMainRunLoopActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		if (fp_AddToQueue(fg_Move(_Functor)))
			fp_QueueProcess();
	}

	void COSMainRunLoopActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		// Reference this so it doesn't go out of scope if queue is processed before thread has been notified
		TCActorHolderSharedPointer<COSMainRunLoopActorHolder> pThis = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor)))
			fp_QueueProcess();
	}

	void COSMainRunLoopActorHolder::fp_DestroyThreaded()
	{
		CDefaultActorHolder::fp_DestroyThreaded();
	}
}
