// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_AsyncDestroy.h"

#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	constinit CAsyncDestroyHelper g_AsyncDestroy;

	TCFutureAwaiter<void, true, void *> CFutureCoroutineContext::await_transform(CAsyncDestroyHelper)
	{
		TCPromise<void> Promise;

		if (!m_AsyncDestructors.f_IsEmpty())
		{
			TCActorResultVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults.f_AddResult();

			m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromise<void> Promise;
				DestroyResults.f_GetResults() > Promise.f_ReceiveAnyUnwrap();
				return {fg_Move(Promise.f_MoveFuture()), nullptr};
			}
		}

		Promise.f_SetResult();

		return {fg_Move(Promise.f_MoveFuture()), nullptr};
	}

	TCFutureAwaiter<void, false, void *> CFutureCoroutineContext::await_transform(CAsyncDestroyHelperNoWrap)
	{
		TCPromise<void> Promise;

		if (!m_AsyncDestructors.f_IsEmpty())
		{
			TCActorResultVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults.f_AddResult();

			m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromise<void> Promise;
				DestroyResults.f_GetResults() > Promise.f_ReceiveAnyUnwrap();
				return {fg_Move(Promise.f_MoveFuture()), nullptr};
			}
		}

		Promise.f_SetResult();

		return {fg_Move(Promise.f_MoveFuture()), nullptr};
	}

	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(FUnitVoidFutureFunction &&_fDestroy)
	{
		return fg_Move(_fDestroy);
	}

	TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction> fg_AsyncDestroy(CActorSubscription &&_pToDestroy)
	{
		return fg_AsyncDestroyByValue
			(
				FUnitVoidFutureFunction
				(
					[pToDestroy = fg_Move(_pToDestroy)]() mutable -> TCFuture<void>
					{
						co_await fg_Exchange(pToDestroy, nullptr)->f_Destroy();

						co_return {};
					}
				)
			)
		;
	}

	namespace NPrivate
	{
		void fg_AsyncDestroyLogErrorHelper(CAsyncResult const &_Result)
		{
			_Result > fg_LogError("Mib/Concurrency", "Failed to async destroy");
		}
	}
}
