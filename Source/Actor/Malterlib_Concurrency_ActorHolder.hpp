// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCActor<tf_CActor> CActorHolder::fp_GetAsActor()
	{
		return TCActorHolderSharedPointer<TCActorInternal<tf_CActor>>
			(
				fg_Explicit(static_cast<TCActorInternal<tf_CActor> *>(this))
			)
		;
	}

	inline_always CConcurrencyManager &CActorHolder::f_ConcurrencyManager() const
	{
		return *mp_pConcurrencyManager;
	}

	inline_always EPriority CActorHolder::f_GetPriority() const
	{
		return (EPriority)mp_Priority;
	}

	inline_always void CActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		fp_QueueProcess(fg_Move(_Functor), _ThreadLocal);
	}

	inline_always void CActorHolder::f_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		fp_QueueProcessDestroy(fg_Move(_Functor), _ThreadLocal);
	}

	inline_always void CActorHolder::f_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		fp_QueueProcessEntry(fg_Move(_Entry), _ThreadLocal);
	}
}
