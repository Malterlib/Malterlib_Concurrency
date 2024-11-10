// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::TCActorCallOnce
		(
			TCActorFunctor<TCFuture<t_CResult> (tp_CParams...)> &&_fFunction
			, bool _bSupportRetry
			, NStr::CStr const &_ErrorOnRunning
		)
		: m_CallState(fg_ConstructActor<CCallState>(fg_Construct(_fFunction.f_GetActor()), fg_Move(_fFunction), _ErrorOnRunning, _bSupportRetry))
	{
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::~TCActorCallOnce()
	{
		if (m_CallState)
			fg_Move(m_CallState).f_Destroy().f_DiscardResult();
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<void> TCActorCallOnce<t_CResult, tp_CParams...>::f_Destroy()
	{
		TCPromise<void> Promise{CPromiseConstructNoConsume()};

		if (m_CallState)
			return Promise <<= fg_Move(m_CallState).f_Destroy();

		return Promise <<= g_Void;
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::operator()(tp_CParams ...p_Params)
	{
		return m_CallState.template f_Bind<&CCallState::f_Call, EVirtualCall::mc_NotVirtual>(fg_Forward<tp_CParams>(p_Params)...).f_Call();
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::CCallState
		(
			TCActorFunctor<TCFuture<t_CResult> (tp_CParams...)> &&_fToPerform
			, NStr::CStr const &_ErrorOnRunning
			, bool _bSupportRetry
		)
		: m_fToPerform(fg_Move(_fToPerform))
		, m_ErrorOnRunning(_ErrorOnRunning)
		, m_bSupportRetry(_bSupportRetry)
	{
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::f_Call(NTraits::TCDecayType<tp_CParams> ...p_Params)
	{
		if (m_Result.f_IsSet())
			co_return m_Result;

		if (m_bRunning)
		{
			if (m_ErrorOnRunning.f_IsEmpty())
				co_return co_await m_Promises.f_Insert().f_Future();
			else
				co_return DMibErrorInstance(m_ErrorOnRunning);
		}

		m_bRunning = true;

		auto Result = co_await m_fToPerform(fg_Forward<tp_CParams>(p_Params)...).f_Wrap();

		fg_Dispatch
			(
				[Promises = fg_Move(m_Promises), Result]() mutable
				{
					for (auto &DeferredPromise : Promises)
						DeferredPromise.f_SetResult(Result);
				}
			)
			.f_DiscardResult()
		;

		if (Result || !m_bSupportRetry)
		{
			m_Result = Result;
			m_fToPerform.f_Clear();
		}

		m_bRunning = false;

		co_return fg_Move(Result);
	}
}
