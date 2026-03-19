// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	template <typename t_CParams>
	struct TCSafeCallParamsToFunctionPointerImpl;

	template <typename ...tp_CParams>
	struct TCSafeCallParamsToFunctionPointerImpl<NMeta::TCTypeList<tp_CParams...>>
	{
		using CType = NTraits::TCAddPointer<void (NTraits::TCRemoveQualifiers<NTraits::TCDecay<tp_CParams>>...)>;
	};

	template <typename t_CParams>
	using TCSafeCallParamsToFunctionPointer = typename TCSafeCallParamsToFunctionPointerImpl<t_CParams>::CType;

	template <typename tf_CReturn, typename ...tfp_CStorageParams, typename tf_CFunction, typename ...tfp_CParams, umint... tfp_Indices>
	auto fg_CallSafeImpl(tf_CFunction &&_Function, NMeta::TCIndices<tfp_Indices...> const &_Indices, tfp_CParams &&...p_Params)
		requires NTraits::cIsCallableWith
		<
			TCSafeCallParamsToFunctionPointer<NMeta::TCTypeList<tfp_CStorageParams...>>
			, void (tfp_CParams...)
		>
	{
		static_assert(NPrivate::TCIsFuture<tf_CReturn>::mc_Value || NPrivate::TCIsUnsafeFuture<tf_CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value);

		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		struct CState final : public CPromiseKeepAlive
		{
			CState(tf_CFunction &&_Function)
				: CPromiseKeepAlive(sizeof(*this))
				, m_Function(fg_Forward<tf_CFunction>(_Function))
			{
			}

			NTraits::TCDecay<tf_CFunction> m_Function;
		};

		auto pState = new CState(fg_Forward<tf_CFunction>(_Function));
		auto &State = *pState;

#if DMibEnableSafeCheck > 0
		bool bPreviousExpectCoroutineCall = PromiseThreadLocal.m_bExpectCoroutineCall;
		PromiseThreadLocal.m_bExpectCoroutineCall = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
			}
		;

		auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy;
		PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = nullptr;

		auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
		PromiseThreadLocal.m_bCaptureDebugException = true;

		auto bPreviousSafeCall = PromiseThreadLocal.m_bSafeCall;
		PromiseThreadLocal.m_bSafeCall = true;

		auto CleanupConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
				PromiseThreadLocal.m_bSafeCall = bPreviousSafeCall;
			}
		;
#endif
		auto pPreviousKeepAlive = PromiseThreadLocal.m_pKeepAlive;
		PromiseThreadLocal.m_pKeepAlive = pState;

		auto CleanupConsumeKeepalive = g_OnScopeExit / [&]
			{
				if (PromiseThreadLocal.m_pKeepAlive)
				{
					DMibFastCheck(PromiseThreadLocal.m_pKeepAlive == pState);
					fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pState);
				}
				PromiseThreadLocal.m_pKeepAlive = pPreviousKeepAlive;
			}
		;

#if DMibEnableSafeCheck > 0
		auto Return = State.m_Function(fg_Forward<tfp_CParams>(p_Params)...);
		DMibFastCheck
			(
				PromiseThreadLocal.m_bExpectCoroutineCall
				|| !PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy
				|| Return.f_Debug_HasData(PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy)
			)
		;
		return Return;
#else
		return State.m_Function(fg_Forward<tfp_CParams>(p_Params)...);
#endif
	}

	template <typename tf_CFunction, typename tf_CFunctor, typename ...tfp_CStorageParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafeGenericImpl(NMeta::TCTypeList<tfp_CStorageParams...> _TypeList, tf_CFunctor &&_fFunction, tfp_CParams &&...p_Params)
		requires NTraits::cIsCallableWith
		<
			TCSafeCallParamsToFunctionPointer<NMeta::TCTypeList<tfp_CStorageParams...>>
			, void (tfp_CParams...)
		>
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_CFunction>;
		using CReturn = NTraits::TCCallableReturnTypeFor<CFunctionType, void (tfp_CParams...)>;
		return fg_CallSafeImpl<CReturn, tfp_CStorageParams...>
			(
				fg_Forward<tf_CFunctor>(_fFunction)
				, NMeta::TCConsecutiveIndices<sizeof...(tfp_CParams)>()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CFunction, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CFunction &&_fFunction, tfp_CParams &&...p_Params)
		requires NTraits::cIsCallableWith
		<
			TCSafeCallParamsToFunctionPointer<typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReference<tf_CFunction>::operator ())>::CParams>
			, void (tfp_CParams...)
		>
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_CFunction>;
		return fg_CallSafeGenericImpl<tf_CFunction>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, fg_Forward<tf_CFunction>(_fFunction)
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CFunction, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(NStorage::TCSharedPointer<tf_CFunction> const &_fFunction, tfp_CParams &&...p_Params)
		requires NTraits::cIsCallableWith
		<
			TCSafeCallParamsToFunctionPointer<typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReference<tf_CFunction>::operator ())>::CParams>
			, void (tfp_CParams...)
		>
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_CFunction>;
		return fg_CallSafeGenericImpl<tf_CFunction>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, _fFunction
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CFunction, typename ...tfp_CParams>
	auto fg_CallSafeDispatchedOn(TCActor<CActor> &&_Actor, tf_CFunction &&_fFunction, tfp_CParams &&...p_Params)
	{
		using CReturn = NTraits::TCDecay<decltype(fg_CallSafe(fg_Forward<tf_CFunction>(_fFunction), fg_Forward<tfp_CParams>(p_Params)...))>;
		using CStrippedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;
		return g_Dispatch(_Actor) / [fFunction = fg_Forward<tf_CFunction>(_fFunction), ...p_Params = fg_Forward<tfp_CParams>(p_Params)]() mutable -> TCFuture<CStrippedReturn>
			{
				if constexpr (NPrivate::TCIsFuture<CReturn>::mc_Value)
					co_return co_await fg_CallSafe(fg_Move(fFunction), fg_Move(p_Params)...);
				else
				{
					co_return fg_CallSafe(fg_Move(fFunction), fg_Move(p_Params)...);
				}
			}
		;
	}
	template <typename tf_CFunction, typename ...tfp_CParams>
	auto fg_CallSafeDispatched(tf_CFunction &&_fFunction, tfp_CParams &&...p_Params)
	{
		return fg_CallSafeDispatchedOn(fg_CurrentActor(), fg_Forward<tf_CFunction>(_fFunction), fg_Forward<tfp_CParams>(p_Params)...);
	}
}
