// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_AsyncDestroy.h"

#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	constinit CAsyncDestroyHelper g_AsyncDestroy;

	TCFutureAwaiter<void, true, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelper)
	{
		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
				return {fg_AllDone(DestroyResults), {}};
		}

		return {g_Void, {}};
	}

	TCFutureAwaiter<NContainer::TCVector<TCAsyncResult<void>>, true, CVoidTag> CFutureCoroutineContext::await_transform(CAsyncDestroyHelperNoUnWrap)
	{
		DMibFastCheck(m_bRuntimeStateConstructed);

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

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

	TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction> fg_AsyncDestroy(CActorSubscription &&_pToDestroy)
	{
		return fg_AsyncDestroyByValue
			(
				FUnitVoidFutureFunction
				(
					[pToDestroy = fg_Move(_pToDestroy)]() mutable -> TCFuture<void>
					{
						if (pToDestroy)
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
