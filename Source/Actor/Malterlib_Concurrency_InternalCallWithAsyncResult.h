// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturn>
	struct TCGetReturnType
	{
		typedef t_CReturn CType;
	};

	template <typename t_CReturn>
	struct TCGetReturnType<TCContinuation<t_CReturn>>
	{
		typedef t_CReturn CType;
	};

	template <typename t_CReturn, EContinuationOption t_Options>
	struct TCGetReturnType<TCContinuationWithOptions<t_CReturn, t_Options>>
	{
		typedef t_CReturn CType;
	};

	template <typename t_CType>
	struct TCIsContinuation
	{
		enum
		{
			mc_Value = false
		};
	};

	template <typename t_CType>
	struct TCIsContinuation<TCContinuation<t_CType>>
	{
		using CType = t_CType;
		enum
		{
			mc_Value = true
		};
	};

	template <typename t_CType, EContinuationOption t_Options>
	struct TCIsContinuation<TCContinuationWithOptions<t_CType, t_Options>>
	{
		using CType = t_CType;
		enum
		{
			mc_Value = true
		};
	};

	template <typename t_CType>
	struct TCIsContinuationWithError
	{
		enum
		{
			mc_Value = false
		};
	};

	template <typename t_CReturnValue>
	struct TCIsContinuationWithError<TCContinuationWithError<t_CReturnValue>>
	{
		enum
		{
			mc_Value = true
		};
	};

	template <typename tf_CResultFunctor, typename tf_CResultActor, typename tf_CResult>
	void fg_CallResultFunctor(tf_CResultFunctor &_ResultFunctor, tf_CResultActor _pResultActor, tf_CResult &&_Result)
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
