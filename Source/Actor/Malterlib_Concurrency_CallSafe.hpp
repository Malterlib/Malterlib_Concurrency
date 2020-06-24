// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	template <typename tf_CReturn, typename tf_CFunction, typename ...tfp_CParams, mint... tfp_Indices>
	auto fg_CallSafeImpl(tf_CFunction &&_Function, NMeta::TCIndices<tfp_Indices...> const &_Indices, tfp_CParams &&...p_Params)
	{
		static_assert(NPrivate::TCIsFuture<tf_CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value);

		using CReturn = typename TCChooseType
			<
				NPrivate::TCIsFuture<tf_CReturn>::mc_Value
				, typename NPrivate::TCIsFuture<tf_CReturn>::CType
				, tf_CReturn
			>::CType
		;

		struct CState
		{
			tf_CFunction m_Function;
			NStorage::TCTuple<typename NTraits::TCRemoveQualifiers<typename NTraits::TCDecay<tfp_CParams>::CType>::CType...> m_ParamList;
			no_unique_address_workaround typename TCChooseType<NPrivate::TCIsFuture<tf_CReturn>::mc_Value, TCPromise<CReturn>, CEmpty>::CType m_Promise;
		};

		NStorage::TCUniquePointer<CState> pState = NStorage::TCUniquePointer<CState>(new CState{fg_Forward<tf_CFunction>(_Function), NStorage::fg_Tuple(fg_Forward<tfp_CParams>(p_Params)...)});

		auto &State = *pState;

		NFunction::TCFunctionMovable<void (TCAsyncResult<CReturn> &&_AsyncResult)> fOnResultSet
			= [pState = fg_Move(pState)](NConcurrency::TCAsyncResult<CReturn> &&_Result) mutable
			{
				if constexpr (NPrivate::TCIsFuture<tf_CReturn>::mc_Value)
					pState->m_Promise.f_SetResult(fg_Move(_Result));
			}
		;

		auto Future = [&]
			{
				if constexpr (NPrivate::TCIsFuture<tf_CReturn>::mc_Value)
					return State.m_Promise.f_Future();
				else
					return CEmpty();
			}
			()
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

		auto Return = State.m_Function(fg_Move(fg_Get<tfp_Indices>(State.m_ParamList))...);

		DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Return.f_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
		DMibFastCheck
			(
				ThreadLocal.m_bExpectCoroutineCall
				|| !PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy
				|| Return.f_HasData(PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy)
			)
		;

 		ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
		Cleanup.f_Clear();
#else
		auto Return = State.m_Function(fg_Move(fg_Get<tfp_Indices>(State.m_ParamList))...);
#endif
		if constexpr (NPrivate::TCIsFuture<tf_CReturn>::mc_Value)
		{
			if (PromiseThreadLocal.m_pOnResultSet)
			{
				DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
				Return.f_OnResultSet(fg_Move(fOnResultSet));
			}

			return Future;
		}
		else
		{
			DMibFastCheck(!PromiseThreadLocal.m_pOnResultSet || PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);

			return Return;
		}
	}

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass *_pClassPtr, tf_CReturn (tf_CClass::*_pPtr) (tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params)
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
	mark_artificial inline_always auto fg_CallSafe(tf_CClass const *_pClassPtr, tf_CReturn (tf_CClass::*_pPtr) (tfp_CFuncPtrParams ...) const, tfp_CParams &&...p_Params)
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
				NFunction::TCMemberFunctionBoundFunctor<tf_CReturn (tf_CClass::*) (tfp_CFuncPtrParams ...) const, tf_CClass const *>(_pPtr, _pClassPtr)
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass &_ClassRef, tf_CReturn (tf_CClass::*_pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params)
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
				NFunction::TCMemberFunctionBoundFunctor<tf_CReturn (tf_CClass::*) (tfp_CFuncPtrParams ...), tf_CClass *>(_pPtr, &_ClassRef)
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass const &_ClassRef, tf_CReturn (tf_CClass::*_pPtr)(tfp_CFuncPtrParams ...) const, tfp_CParams &&...p_Params)
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
				NFunction::TCMemberFunctionBoundFunctor<tf_CReturn (tf_CClass::*) (tfp_CFuncPtrParams ...) const, tf_CClass const *>(_pPtr, &_ClassRef)
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CReturn (* _pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params)
	{
		return fg_CallSafeImpl<tf_CReturn>
			(
				_pPtr
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CFuncPtrParams)>::CType()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CFunction, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CFunction &&_fFunction, tfp_CParams &&...p_Params)
	{
		static_assert(NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_CFunction>::CType, void (tfp_CParams...)>::mc_Value);
		using CReturn = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_CFunction>::CType, void (tfp_CParams...)>::CReturnType;
		return fg_CallSafeImpl<CReturn>
			(
				fg_Forward<tf_CFunction>(_fFunction)
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CParams)>::CType()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}
}
