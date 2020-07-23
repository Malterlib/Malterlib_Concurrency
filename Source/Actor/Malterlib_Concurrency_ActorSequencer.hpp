// Copyright © 2017 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCActorSequencer<t_CReturnType>::TCActorSequencer(mint _MaxConcurrency)
		: mp_pState(fg_Construct())
	{
		mp_pState->m_MaxConcurrency = _MaxConcurrency;
	}
	
	template <typename t_CReturnType>
	template <typename tf_FToSequence>
	TCFuture<t_CReturnType> TCActorSequencer<t_CReturnType>::operator / (tf_FToSequence &&_fToSequence)
	{
		auto fQueueSequence = [pState = mp_pState, fToSequence = fg_Move(_fToSequence)]() mutable -> TCFuture<t_CReturnType>
			{
				TCPromise<t_CReturnType> Promise;

				auto &State = *pState;
				if (State.m_bDestroyed)
					return Promise <<= DMibErrorInstance("Sequencer has been aborted");

				auto &ToSequence = State.m_ToSequence.f_Insert(CToSequenceEntry{fg_Move(Promise)});
				ToSequence.m_fToSequence = fg_Move(fToSequence);

				auto Future = ToSequence.m_Promise.f_Future();

				State.f_ProcessSequence();

				return fg_Move(Future);
			}
		;
		if (fg_CurrentActorProcessing())
			return fQueueSequence();
		return g_Future <<= g_Dispatch / fg_Move(fQueueSequence);
	}
	
	template <typename t_CReturnType>
	TCActorSequencer<t_CReturnType>::~TCActorSequencer()
	{
		if (!mp_pState)
			return;
		
		auto &State = *mp_pState;
		
		for (auto &ToSequence : State.m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer went out of scope"));
		
		State.m_ToSequence.f_Clear();
		State.m_bDestroyed = true;
	}
	
	template <typename t_CReturnType>
	TCFuture<void> TCActorSequencer<t_CReturnType>::f_Abort()
	{
		auto fDoAbort = [pState = mp_pState]() -> TCFuture<void>
			{
				TCPromise<void> AbortPromise;
				auto &State = *pState;

				for (auto &ToSequence : State.m_ToSequence)
					ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer aborted"));

				State.m_ToSequence.f_Clear();
				State.m_bDestroyed = true;

				if (State.m_nRunning)
					return (*(State.m_AbortPromise = fg_Move(AbortPromise))).f_Future();
				else
					return AbortPromise <<= g_Void;
			}
		;
		if (fg_CurrentActorProcessing())
			return fDoAbort();
		return g_Future <<= g_Dispatch / fg_Move(fDoAbort);
	}
	
	template <typename t_CReturnType>
	void TCActorSequencer<t_CReturnType>::CState::f_ProcessSequence()
	{
		if (m_ToSequence.f_IsEmpty() || m_nRunning >= m_MaxConcurrency)
			return;
		
		++m_nRunning;
		
		auto ToSequence = m_ToSequence.f_Pop();
		g_Dispatch / fg_Move(ToSequence.m_fToSequence)
			> [pState = NStorage::TCSharedPointer<CState>(fg_Explicit(this)), Promise = ToSequence.m_Promise](TCAsyncResult<t_CReturnType> &&_Result)
			{
				Promise.f_SetResult(fg_Move(_Result));
				
				auto &State = *pState;
				
				--State.m_nRunning;
				
				if (State.m_bDestroyed)
				{
					if (State.m_nRunning == 0)
					{
						if (State.m_AbortPromise && !(*State.m_AbortPromise).f_IsSet())
							(*State.m_AbortPromise).f_SetResult();
					}
					return;
				}
				
				State.f_ProcessSequence();
			}
		;
	}
}
