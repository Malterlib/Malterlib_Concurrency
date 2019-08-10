// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturn, typename tf_CFunction, typename ...tfp_CParams, mint... tfp_Indices>
	auto fg_CallSafeImpl(tf_CFunction &&_Function, NMeta::TCIndices<tfp_Indices...> const &_Indices, tfp_CParams &&...p_Params)
	{
		static_assert(NPrivate::TCIsFuture<tf_CReturn>::mc_Value);

		using CReturn = typename NPrivate::TCIsFuture<tf_CReturn>::CType;

		TCPromise<CReturn> Promise;

		struct CState
		{
			tf_CFunction m_Function;
			NStorage::TCTuple<typename NTraits::TCRemoveQualifiers<typename NTraits::TCDecay<tfp_CParams>::CType>::CType...> m_ParamList;
		};

		NStorage::TCUniquePointer<CState> pState = NStorage::TCUniquePointer<CState>(new CState{fg_Forward<tf_CFunction>(_Function), NStorage::fg_Tuple(fg_Forward<tfp_CParams>(p_Params)...)});

		auto &State = *pState;

		NFunction::TCFunctionMovable<void (TCAsyncResult<CReturn> &&_AsyncResult)> fOnResultSet
			= [Promise, pState = fg_Move(pState)](NConcurrency::TCAsyncResult<CReturn> &&_Result) mutable
			{
				Promise.f_SetResult(fg_Move(_Result));
			}
		;

		auto &ThreadLocal = **g_SystemThreadLocal;

#if DMibEnableSafeCheck > 0
		bool bPreviousExpectCoroutineCall = ThreadLocal.m_bExpectCoroutineCall;
		ThreadLocal.m_bExpectCoroutineCall = true;
		auto Cleanup = g_OnScopeExit > [&]
			{
				ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
			}
		;
#endif

		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;
		auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
		PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

		auto CleanupOnResultSet = g_OnScopeExit > [&]
			{
				PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
			}
		;

#if DMibEnableSafeCheck > 0
		auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
		PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

		auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy;
		PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = nullptr;

		auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
		PromiseThreadLocal.m_bCaptureDebugException = true;

		auto CleanupOnResultSetConsumedBy = g_OnScopeExit > [&]
			{
				PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
				PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
			}
		;

		PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<CReturn>();

		auto Future = State.m_Function(fg_Move(fg_Get<tfp_Indices>(State.m_ParamList))...);

		DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Future.f_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
		DMibFastCheck
			(
				ThreadLocal.m_bExpectCoroutineCall
				|| !PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy
				|| Future.f_HasData(PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy)
			)
		;

		ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
		Cleanup.f_Clear();
#else
		auto Future = State.m_Function(fg_Move(fg_Get<tfp_Indices>(State.m_ParamList))...);
#endif
		if (PromiseThreadLocal.m_pOnResultSet)
		{
			DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
			Future.f_OnResultSet(fg_Move(fOnResultSet));
		}

		return Promise.f_MoveFuture();
	}

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass *_pClassPtr, tf_CReturn (tf_CClass::*_pPtr) (tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
			 	NFunction::TCMemberFunctionBoundFunctor<tf_CReturn (tf_CClass::*) (tfp_CFuncPtrParams ...), tf_CClass *>(_pPtr, _pClassPtr)
			 	, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass &_ClassRef, tf_CReturn (tf_CClass::*_pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
			 	NFunction::TCMemberFunctionBoundFunctor<tf_CReturn (tf_CClass::*) (tfp_CFuncPtrParams ...), tf_CClass *>(_pPtr, &_ClassRef)
			 	, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CReturn (* _pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
			 	_pPtr
			 	, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename t_CFunction, typename... tp_COptions, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(NFunction::TCFunction<t_CFunction, tp_COptions...> const &_fFunction, tfp_CParams &&...p_Params)
	{
		using CCallType = typename NFunction::TCFunctionInfo<NFunction::TCFunction<t_CFunction, tp_COptions...>>::template TCCallType<0>;
		return fg_CallSafeImpl<typename NTraits::TCFunctionTraits<CCallType>::CReturn>
			(
			 	_fFunction
			 	, typename NMeta::TCMakeConsecutiveIndices<NTraits::TCFunctionTraits<CCallType>::mc_Arity>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename t_CFunction, typename... tp_COptions, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(NFunction::TCFunction<t_CFunction, tp_COptions...> &_fFunction, tfp_CParams &&...p_Params)
	{
		using CCallType = typename NFunction::TCFunctionInfo<NFunction::TCFunction<t_CFunction, tp_COptions...>>::template TCCallType<0>;
		return fg_CallSafeImpl<typename NTraits::TCFunctionTraits<CCallType>::CReturn>
			(
			 	_fFunction
			 	, typename NMeta::TCMakeConsecutiveIndices<NTraits::TCFunctionTraits<CCallType>::mc_Arity>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename t_CFunction, typename... tp_COptions, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(NFunction::TCFunction<t_CFunction, tp_COptions...> &&_fFunction, tfp_CParams &&...p_Params)
	{
		using CCallType = typename NFunction::TCFunctionInfo<NFunction::TCFunction<t_CFunction, tp_COptions...>>::template TCCallType<0>;
		return fg_CallSafeImpl<typename NTraits::TCFunctionTraits<CCallType>::CReturn>
			(
			 	fg_Move(_fFunction)
			 	, typename NMeta::TCMakeConsecutiveIndices<NTraits::TCFunctionTraits<CCallType>::mc_Arity>::CType()
			 	, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}
}
