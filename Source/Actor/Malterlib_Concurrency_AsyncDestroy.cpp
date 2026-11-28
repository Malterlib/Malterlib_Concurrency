// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_AsyncDestroy.h"

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	constinit CAsyncDestroyHelper g_AsyncDestroy;

	TCFutureAwaiter<void, true, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelper _Helper)
	{
		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &AsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(AsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
				return {fg_AllDone(DestroyResults), {}};
		}

		return {g_Void, {}};
	}

	TCFutureAwaiter<NContainer::TCVector<TCAsyncResult<void>>, true, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelperNoUnWrap _Helper)
	{
		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &AsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(AsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
				return {fg_AllDoneWrapped(DestroyResults), {}};
		}

		return {NContainer::TCVector<TCAsyncResult<void>>{}, {}};
	}

	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(FUnitVoidFutureFunction &&_fDestroy)
	{
		return fg_Move(_fDestroy);
	}

	TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction> fg_AsyncDestroy(CActorSubscription &&_ToDestroy)
	{
		return fg_AsyncDestroyByValue
			(
				FUnitVoidFutureFunction
				(
					[ToDestroy = fg_Move(_ToDestroy)]() mutable -> TCFuture<void>
					{
						if (ToDestroy)
							co_await fg_Exchange(ToDestroy, nullptr)->f_Destroy();

						co_return {};
					}
				)
			)
		;
	}

	namespace NPrivate
	{
		TCFuture<void> fg_SynchronizeWithActor(TCActor<> const &_Actor)
		{
			if (_Actor.f_IsEmpty())
				return g_Void;

			DMibFastCheck(fg_GetActorInternal(_Actor)->f_RunsCallsOneAtATime());

			// The call holds the actor, which a delegated actor's queue only refers to weakly, also when the caller lets go of it first
			return g_Dispatch(_Actor) / [Actor = _Actor]
				{
				}
			;
		}

		void fg_AsyncDestroyLogErrorHelper(CAsyncResult const &_Result)
		{
			_Result > fg_LogError("Mib/Concurrency", "Failed to async destroy");
		}
	}
}
