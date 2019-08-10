// Copyright © 2017 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ActorSubscription>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCActorSequencerAsync<t_CReturnType>::TCActorSequencerAsync(mint _MaxConcurrency)
		: mp_pState(fg_Construct())
	{
		mp_pState->m_MaxConcurrency = _MaxConcurrency;
	}
	
	template <typename t_CReturnType>
	template <typename tf_FToSequence>
	TCFuture<t_CReturnType> TCActorSequencerAsync<t_CReturnType>::operator / (tf_FToSequence &&_fToSequence)
	{
		auto fQueueSequence = [this, fToSequence = fg_Move(_fToSequence)]() mutable -> TCFuture<t_CReturnType>
			{
				TCPromise<t_CReturnType> Promise;

				auto &State = *mp_pState;
				if (State.m_bDestroyed)
					return Promise <<= DMibErrorInstance("Sequencer has been aborted");

				auto &ToSequence = State.m_ToSequence.f_Insert(CToSequenceEntry{fg_Move(Promise)});
				ToSequence.m_fToSequence = fg_Move(fToSequence);

				auto Future = ToSequence.m_Promise.f_Future();

				fp_ProcessSequence();

				return fg_Move(Future);
			}
		;
		if (fg_CurrentActorRunning())
			return fQueueSequence();
		return g_Future <<= g_Dispatch / fg_Move(fQueueSequence);
	}
	
	template <typename t_CReturnType>
	TCActorSequencerAsync<t_CReturnType>::~TCActorSequencerAsync()
	{
		auto &State = *mp_pState;
		
		for (auto &ToSequence : State.m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer went out of scope"));
		
		State.m_ToSequence.f_Clear();
		State.m_bDestroyed = true;
	}
	
	template <typename t_CReturnType>
	TCFuture<void> TCActorSequencerAsync<t_CReturnType>::f_Abort()
	{
		auto fDoAbort = [this]() -> TCFuture<void>
			{
				TCPromise<void> AbortPromise;

				auto &State = *mp_pState;

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
		if (fg_CurrentActorRunning())
			return fDoAbort();
		return g_Future <<= g_Dispatch / fg_Move(fDoAbort);
	}
	
	template <typename t_CReturnType>
	void TCActorSequencerAsync<t_CReturnType>::fp_ProcessSequence()
	{
		auto &State = *mp_pState;
		
		if (State.m_ToSequence.f_IsEmpty() || State.m_nRunning >= State.m_MaxConcurrency)
			return;
		
		++State.m_nRunning;
		
		auto ToSequence = State.m_ToSequence.f_Pop();
		g_Dispatch / [this, pState = mp_pState, fToSequence = fg_Move(ToSequence.m_fToSequence)]() mutable -> TCFuture<t_CReturnType>
			{
				return fToSequence
					(
					 	g_ActorSubscription / [this, pState = mp_pState]
						{
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

							fp_ProcessSequence();
						}
					)
				;
			}
			> [Promise = ToSequence.m_Promise](TCAsyncResult<t_CReturnType> &&_Result)
			{
				Promise.f_SetResult(fg_Move(_Result));
			}
		;
	}
}
