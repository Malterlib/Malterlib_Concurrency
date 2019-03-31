// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::TCActorCallOnce
		(
			TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMovable<TCFuture<t_CResult> (tp_CParams...)> &&_fFunction
			, bool _bSupportRetry
			, NStr::CStr const &_ErrorOnRunning
		)
		: m_CallState(fg_ConstructActor<CCallState>(fg_Construct(_Actor), _Actor, fg_Move(_fFunction), _ErrorOnRunning, _bSupportRetry))
	{
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::~TCActorCallOnce()
	{
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::operator()(tp_CParams const &...p_Params)
	{
		return m_CallState.f_CallByValue(&CCallState::f_Call, p_Params...);
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::CCallState
		(
			TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMovable<TCFuture<t_CResult> (tp_CParams...)> &&_fToPerform
			, NStr::CStr const &_ErrorOnRunning
			, bool _bSupportRetry
		)
		: m_Actor(_Actor)
		, m_fToPerform(fg_Move(_fToPerform))
		, m_ErrorOnRunning(_ErrorOnRunning)
		, m_bSupportRetry(_bSupportRetry)
	{
	}

	template <typename t_CResult, typename ...tp_CParams>
	TCFuture<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::f_Call(tp_CParams const &...p_Params)
	{
		if (m_Result.f_IsSet())
			return m_Result;

		if (m_bRunning)
		{
			if (m_ErrorOnRunning.f_IsEmpty())
				return m_Promises.f_Insert();
			else
				return DMibErrorInstance(m_ErrorOnRunning);
		}

		m_bRunning = true;

		TCPromise<t_CResult> Promise;

		auto Actor = m_Actor.f_Lock();
		if (!Actor)
			return DMibErrorInstance("Actor destroyed");

		CCurrentActorScope CurrentActor(Actor);

		m_fToPerform(p_Params...) > [this, Promise](TCAsyncResult<t_CResult> const &_Result)
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
