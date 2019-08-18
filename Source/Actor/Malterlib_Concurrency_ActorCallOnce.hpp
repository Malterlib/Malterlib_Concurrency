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
			fg_Move(m_CallState).f_Destroy() > fg_DiscardResult();
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<void> TCActorCallOnce<t_CResult, tp_CParams...>::f_Destroy()
	{
		TCPromise<void> Promise;

		if (m_CallState)
			return Promise <<= fg_Move(m_CallState).f_Destroy();

		return Promise <<= g_Void;
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::operator()(tp_CParams ...p_Params)
	{
		TCPromise<t_CResult> Promise;
		return Promise <<= m_CallState.template f_CallByValue<&CCallState::f_Call>(fg_Forward<tp_CParams>(p_Params)...);
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
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::f_Call(tp_CParams ...p_Params)
	{
		TCPromise<t_CResult> Promise;

		if (m_Result.f_IsSet())
			return Promise <<= m_Result;

		if (m_bRunning)
		{
			if (m_ErrorOnRunning.f_IsEmpty())
				return m_Promises.f_Insert(fg_Move(Promise)).f_Future();
			else
				return Promise <<= DMibErrorInstance(m_ErrorOnRunning);
		}

		m_bRunning = true;

		m_fToPerform(fg_Forward<tp_CParams>(p_Params)...) > [this, Promise](TCAsyncResult<t_CResult> const &_Result)
			{
				Promise.f_SetResult(_Result);

				for (auto &DeferredPromise : m_Promises)
					DeferredPromise.f_SetResult(_Result);
				m_Promises.f_Clear();

				if (_Result || !m_bSupportRetry)
				{
					m_Result = _Result;
					m_fToPerform.f_Clear();
				}

				m_bRunning = false;
			}
		;

		return Promise.f_MoveFuture();

	}
}
