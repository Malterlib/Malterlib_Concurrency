// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_AsyncDestroy.h"

#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	constinit CAsyncDestroyHelper g_AsyncDestroy;

	TCFutureAwaiter<void, true, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelper)
	{
		TCPromise<void> Promise{CPromiseConstructNoConsume()};

		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				fg_AllDoneWrapped(DestroyResults) > Promise.f_ReceiveAnyUnwrap();
				return {fg_Move(Promise.f_MoveFuture()), {}};
			}
		}

		Promise.f_SetResult();

		return {fg_Move(Promise.f_MoveFuture()), {}};
	}

	TCFutureAwaiter<void, false, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelperNoWrap)
	{
		TCPromise<void> Promise{CPromiseConstructNoConsume()};

		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				fg_AllDoneWrapped(DestroyResults) > Promise.f_ReceiveAnyUnwrap();
				return {fg_Move(Promise.f_MoveFuture()), {}};
			}
		}

		Promise.f_SetResult();

		return {fg_Move(Promise.f_MoveFuture()), {}};
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
