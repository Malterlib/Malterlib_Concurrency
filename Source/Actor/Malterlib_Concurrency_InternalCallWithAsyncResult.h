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
		typedef t_CReturn CType;
	};

	template <typename t_CReturn>
	struct TCGetReturnType<TCFuture<t_CReturn>>
	{
		typedef t_CReturn CType;
	};

	template <typename t_CReturn>
	struct TCGetAsyncType
	{
		typedef t_CReturn CType;
	};

	template <typename t_CReturn>
	struct TCGetAsyncType<TCFuture<t_CReturn>>
	{
		typedef t_CReturn CType;
	};

	template <typename t_CReturn>
	struct TCGetAsyncType<TCAsyncGenerator<t_CReturn>>
	{
		typedef t_CReturn CType;
	};

	template <typename t_CType>
	struct TCIsPromise
	{
		using CType = t_CType;
		static constexpr bool mc_Value = false;
	};

	template <typename t_CType>
	struct TCIsPromise<TCPromise<t_CType>>
	{
		using CType = t_CType;
		static constexpr bool mc_Value = true;
	};

	template <typename t_CType>
	struct TCIsFuture
	{
		using CType = t_CType;
		static constexpr bool mc_Value = false;
	};

	template <typename t_CType>
	struct TCIsFuture<TCFuture<t_CType>>
	{
		using CType = t_CType;
		static constexpr bool mc_Value = true;
	};

	template <typename t_CType>
	struct TCIsPromiseWithError
	{
		static constexpr bool mc_Value = false;
	};

	template <typename t_CReturnValue>
	struct TCIsPromiseWithError<TCPromiseWithError<t_CReturnValue>>
	{
		static constexpr bool mc_Value = true;
	};

	template <typename t_CType>
	struct TCIsAsyncGenerator
	{
		using CType = t_CType;
		static constexpr bool mc_Value = false;
	};

	template <typename t_CType>
	struct TCIsAsyncGenerator<TCAsyncGenerator<t_CType>>
	{
		using CType = t_CType;
		static constexpr bool mc_Value = true;
	};

	template <typename tf_CResultFunctor, typename tf_CResultActor, typename tf_CResult>
	void fg_CallResultFunctor(tf_CResultFunctor &_ResultFunctor, tf_CResultActor &&_pResultActor, tf_CResult &&_Result)
	{
#if DMibConfig_Concurrency_DebugActorCallstacks
		auto &Callstack = _Result.m_Callstacks;
		CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
		CCurrentActorScope CurrentActor(_pResultActor);
		(void)_ResultFunctor(fg_Forward<tf_CResult>(_Result));
	}

	template <typename tf_CResultFunctor, typename tf_CResult>
	void fg_CallResultFunctorDirect(tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result)
	{
#if DMibConfig_Concurrency_DebugActorCallstacks
		auto &Callstack = _Result.m_Callstacks;
		CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
		CCurrentActorScope CurrentActor(nullptr);
		(void)_ResultFunctor(fg_Forward<tf_CResult>(_Result));
	}
}
