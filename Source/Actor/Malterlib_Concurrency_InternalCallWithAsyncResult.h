// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCAsyncGenerator;
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturn>
	struct TCGetReturnType
	{
		using CType = t_CReturn;
	};

	template <typename t_CReturn>
	struct TCGetReturnType<TCFuture<t_CReturn>>
	{
		using CType = t_CReturn;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>>
	{
		using CType = t_CReturn;
	};

	template <typename t_CReturn, typename t_CPromiseParam>
	struct TCCallActorGetReturnType
	{
		// Assumed to be on result function
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CPromiseConstructDiscardResult>
	{
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CVoidTag>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = t_CReturn;
	};

	template <typename t_CReturn, typename t_CPromiseParam>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, t_CPromiseParam>
	{
		// Assumed to be on result function
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags, typename t_CPromiseParam>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, t_CPromiseParam>
	{
		// Assumed to be on result function
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CVoidTag>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = TCFuture<t_CReturn>;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CVoidTag>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = TCFuture<t_CReturn>;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CPromiseConstructDiscardResult>
	{
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CPromiseConstructDiscardResult>
	{
		constexpr static bool mc_bIsVoid = true;
		using CType = void;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CPromiseConstructNoConsume>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = TCFuture<t_CReturn>;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CPromiseConstructNoConsume>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = TCFuture<t_CReturn>;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CPromiseConstructNoConsume>
	{
		constexpr static bool mc_bIsVoid = false;
		using CType = TCFuture<t_CReturn>;
	};

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
