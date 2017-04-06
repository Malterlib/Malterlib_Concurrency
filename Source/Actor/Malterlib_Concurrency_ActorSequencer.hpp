// Copyright © 2017 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCActorSequencer<t_CReturnType>::TCActorSequencer()
		: mp_pState(fg_Construct())
	{
	}
	
	template <typename t_CReturnType>
	template <typename tf_FToSequence>
	TCContinuation<t_CReturnType> TCActorSequencer<t_CReturnType>::operator > (tf_FToSequence &&_fToSequence)
	{
		auto &State = *mp_pState;
		if (State.m_bDestroyed)
			return DMibErrorInstance("Sequencer has been aborted");
		
		auto &ToSequence = State.m_ToSequence.f_Insert();
		ToSequence.m_fToSequence = fg_Move(_fToSequence);
		
		auto Continuation = ToSequence.m_Continuation;
		
		fp_ProcessSequence();
		
		return Continuation;
	}
	
	template <typename t_CReturnType>
	TCActorSequencer<t_CReturnType>::~TCActorSequencer()
	{
		auto &State = *mp_pState;
		
		for (auto &ToSequence : State.m_ToSequence)
			ToSequence.m_Continuation.f_SetException(DMibErrorInstance("Actor sequencer went out of scope"));
		
		State.m_ToSequence.f_Clear();
		State.m_bDestroyed = true;
	}
	
	template <typename t_CReturnType>
	TCContinuation<void> TCActorSequencer<t_CReturnType>::f_Abort()
	{
		auto &State = *mp_pState;
		
		for (auto &ToSequence : State.m_ToSequence)
			ToSequence.m_Continuation.f_SetException(DMibErrorInstance("Actor sequencer aborted"));
		
		State.m_ToSequence.f_Clear();
		State.m_bDestroyed = true;
		
		if (State.m_bRunning)
			return State.m_AbortContinuation;
		else
			return fg_Explicit();
	}
	
	template <typename t_CReturnType>
	void TCActorSequencer<t_CReturnType>::fp_ProcessSequence()
	{
		auto &State = *mp_pState;
		
		if (State.m_ToSequence.f_IsEmpty() || State.m_bRunning)
			return;
		
		State.m_bRunning = true;
		
		auto ToSequence = State.m_ToSequence.f_Pop();
		g_Dispatch > fg_Move(ToSequence.m_fToSequence) > [this, pState = mp_pState, Continuation = ToSequence.m_Continuation](TCAsyncResult<void> &&_Result)
			{
				Continuation.f_SetResult(_Result);
				
				auto &State = *pState;
				
				State.m_bRunning = false;
				
				if (State.m_bDestroyed)
				{
					if (!State.m_AbortContinuation.f_IsSet())
						State.m_AbortContinuation.f_SetResult();
					return;
				}
				
				fp_ProcessSequence();
			}
		;
	}
}
