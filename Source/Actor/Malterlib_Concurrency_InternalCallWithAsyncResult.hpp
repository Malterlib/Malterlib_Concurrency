// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CResultFunctor, typename tf_CResult>
	void fg_CallResultFunctor(CConcurrencyThreadLocal &_ThreadLocal, tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result)
	{
#if DMibConfig_Concurrency_DebugActorCallstacks
		auto &Callstack = _Result.m_Callstacks;
		CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
		(void)_ResultFunctor(fg_Forward<tf_CResult>(_Result));
	}

	template <typename tf_CResultFunctor, typename tf_CResult>
	void fg_CallResultFunctorDirect(tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result)
	{
#if DMibConfig_Concurrency_DebugActorCallstacks
		auto &Callstack = _Result.m_Callstacks;
		CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
		CCurrentActorScope CurrentActor(fg_ConcurrencyThreadLocal(), nullptr);
		(void)_ResultFunctor(fg_Forward<tf_CResult>(_Result));
	}
}
