// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CPromiseConstructDiscardResult>
	{
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CVoidTag>
	{
		using CType = t_CReturn;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename t_CReturn, typename t_CPromiseParam>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, t_CPromiseParam>
	{
		// Assumed to be on result function
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags, typename t_CPromiseParam>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, t_CPromiseParam>
	{
		// Assumed to be on result function
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CVoidTag>
	{
		using CType = TCFuture<t_CReturn>;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CVoidTag>
	{
		using CType = TCFuture<t_CReturn>;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CPromiseConstructDiscardResult>
	{
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CPromiseConstructDiscardResult>
	{
		using CType = void;

		static constexpr bool mc_bIsVoid = true;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<TCFuture<t_CReturn>, CPromiseConstructNoConsume>
	{
		using CType = TCFuture<t_CReturn>;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename t_CReturn, ECoroutineFlag t_Flags>
	struct TCCallActorGetReturnType<TCFutureWithFlags<t_CReturn, t_Flags>, CPromiseConstructNoConsume>
	{
		using CType = TCFuture<t_CReturn>;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename t_CReturn>
	struct TCCallActorGetReturnType<t_CReturn, CPromiseConstructNoConsume>
	{
		using CType = TCFuture<t_CReturn>;

		static constexpr bool mc_bIsVoid = false;
	};

	template <typename tf_CResultFunctor, typename tf_CResult>
	void fg_CallResultFunctor(CConcurrencyThreadLocal &_ThreadLocal, tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result);

	template <typename tf_CResultFunctor, typename tf_CResult>
	void fg_CallResultFunctorDirect(tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result);
}

#include "Malterlib_Concurrency_InternalCallWithAsyncResult.hpp"
