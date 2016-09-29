// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CResult, typename ...tp_CParams>
		TCActorCallOnce<t_CResult, tp_CParams...>::TCActorCallOnce
			(
				TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<TCContinuation<t_CResult> (tp_CParams...)> &&_fFunction
				, NStr::CStr const &_ErrorOnRunning
			)
			: m_CallState(fg_ConstructActor<CCallState>(_Actor.f_Weak(), fg_Move(_fFunction), _ErrorOnRunning)) 
		{
		}
		
		template <typename t_CResult, typename ...tp_CParams>
		TCActorCallOnce<t_CResult, tp_CParams...>::~TCActorCallOnce()
		{
		}
		
		template <typename t_CResult, typename ...tp_CParams>
		auto TCActorCallOnce<t_CResult, tp_CParams...>::operator()(tp_CParams const &...p_Params)
		{
			return m_CallState.f_CallByValue(&CCallState::f_Call, p_Params...);
		}
	
		template <typename t_CResult, typename ...tp_CParams>
		TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::CCallState
			(
				TCWeakActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<TCContinuation<t_CResult> (tp_CParams...)> &&_fToPerform
				, NStr::CStr const &_ErrorOnRunning
			)
			: m_Actor(_Actor)
			, m_fToPerform(fg_Move(_fToPerform))
			, m_ErrorOnRunning(_ErrorOnRunning)
		{
		}
		
		template <typename t_CResult, typename ...tp_CParams>
		TCContinuation<t_CResult> TCActorCallOnce<t_CResult, tp_CParams...>::CCallState::f_Call(tp_CParams const &...p_Params)
		{
			if (m_Result.f_IsSet())
			{
				return m_Result;
			}
			
			if (m_bRunning)
			{
				if (m_ErrorOnRunning.f_IsEmpty())
				{
					return m_Continuations.f_Insert();
				}
				else
				{
					return DMibErrorInstance(m_ErrorOnRunning);
				}
			}
			
			auto Actor = m_Actor.f_Lock();
			if (!Actor)
				return DMibErrorInstance("Actor for call once call has been deleted");

			m_bRunning = true;

			TCContinuation<t_CResult> Continuation;

			fg_Dispatch
				(
					Actor
					, [this, p_Params...]() mutable
					{
						return m_fToPerform(p_Params...);
					}
				)
				> [this, Continuation](TCAsyncResult<t_CResult> const &_Result)
				{
					Continuation.f_SetResult(_Result);
					
					for (auto &DeferredContinuation : m_Continuations)
						DeferredContinuation.f_SetResult(_Result);
					m_Continuations.f_Clear();
					
					if (_Result)
					{
						m_Result = _Result;
						m_Actor.f_Clear();
						m_fToPerform.f_Clear();
					}
					
					m_bRunning = false;
				}
			;
			
			return Continuation;
			
		}
	}
}
