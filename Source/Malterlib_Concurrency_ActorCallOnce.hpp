// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CResult>
		TCActorCallOnce<tf_CResult>::TCActorCallOnce(TCWeakActor<CActor> const &_Actor, NFunction::TCFunction<TCContinuation<tf_CResult> (NFunction::CThisTag &)> &&_fFunction)
			: m_CallState(fg_ConstructActor<CCallState>(_Actor, fg_Move(_fFunction))) 
		{
		}
		
		template <typename tf_CResult>
		TCActorCallOnce<tf_CResult>::~TCActorCallOnce()
		{
		}
		
		template <typename tf_CResult>
		auto TCActorCallOnce<tf_CResult>::operator()()
		{
			return m_CallState(&CCallState::f_Call);
		}
	
		template <typename tf_CResult>
		TCActorCallOnce<tf_CResult>::CCallState::CCallState(TCWeakActor<CActor> const &_Actor, NFunction::TCFunction<TCContinuation<tf_CResult> (NFunction::CThisTag &)> &&_fToPerform)
			: m_Actor(_Actor)
			, m_fToPerform(fg_Move(_fToPerform))
		{
		}
		
		template <typename tf_CResult>
		TCContinuation<tf_CResult> TCActorCallOnce<tf_CResult>::CCallState::f_Call()
		{
			TCContinuation<tf_CResult> Continuation;

			if (m_Result.f_IsSet())
			{
				Continuation.f_SetResult(m_Result);
				return Continuation;
			}
			
			if (m_bRunning)
			{
				m_Continuations.f_Insert(Continuation);
				return Continuation;
			}
			
			auto Actor = m_Actor.f_Lock();
			if (!Actor)
				return DMibErrorInstance("Actor for call once call has been deleted");

			m_bRunning = true;
			
			Actor
				(
					&CActor::f_DispatchWithReturn<TCContinuation<tf_CResult>>
					, [this]() mutable
					{
						return m_fToPerform();
					}
				)
				> [this, Continuation](TCAsyncResult<tf_CResult> const &_Result)
				{
					Continuation.f_SetResult(_Result);
					
					for (auto &Continuation : m_Continuations)
						Continuation.f_SetResult(_Result);
					
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
