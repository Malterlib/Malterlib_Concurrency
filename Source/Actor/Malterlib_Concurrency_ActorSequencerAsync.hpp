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
	mint TCActorSequencerAsync<t_CReturnType>::f_NumWaiting() const
	{
		auto &State = *mp_pState;
		return State.m_ToSequence.f_GetLen();
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCActorSequencerAsync<t_CReturnType>::f_Sequence()
		requires (NTraits::TCIsSame<t_CReturnType, void>::mc_Value)
	{
		TCPromise<CActorSubscription> Promise;

		auto fQueueSequence = [pState = mp_pState, Promise]() mutable
			{
				auto &State = *pState;
				if (State.m_bDestroyed)
				{
					if (!Promise.f_IsSet())
						Promise.f_SetException(DMibErrorInstance("Sequencer has been aborted"));
					return;
				}

				auto &ToSequence = State.m_ToSequence.f_Insert(CToSequenceEntry{});
				ToSequence.m_fToSequence = [Promise](CActorSubscription &&_DoneSubscription) -> TCFuture<void>
					{
						TCPromise<void> NewPromise;

						if (!Promise.f_IsSet())
							Promise.f_SetResult(fg_Move(_DoneSubscription));

						return NewPromise <<= g_Void;
					}
				;

				ToSequence.m_Promise.f_Future() > [Promise](TCAsyncResult<void> &&_Result)
					{
						if (!Promise.f_IsSet() && !_Result)
							Promise.f_SetException(fg_Move(_Result));
					}
				;

				State.f_ProcessSequence();
			}
		;

		if (fg_CurrentActorProcessing())
			fQueueSequence();

		g_Dispatch / fg_Move(fQueueSequence) > fg_DiscardResult();

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnType>
	template <typename tf_FToSequence>
	TCFuture<t_CReturnType> TCActorSequencerAsync<t_CReturnType>::operator / (tf_FToSequence &&_fToSequence)
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
	TCActorSequencerAsync<t_CReturnType>::~TCActorSequencerAsync()
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
	TCFuture<void> TCActorSequencerAsync<t_CReturnType>::f_Abort()
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
	void TCActorSequencerAsync<t_CReturnType>::CState::f_ProcessSequence()
	{
		if (m_ToSequence.f_IsEmpty() || m_nRunning >= m_MaxConcurrency)
			return;
		
		++m_nRunning;
		
		auto ToSequence = m_ToSequence.f_Pop();
		g_Dispatch / [pState = NStorage::TCSharedPointer<CState>(fg_Explicit(this)), fToSequence = fg_Move(ToSequence.m_fToSequence)]() mutable -> TCFuture<t_CReturnType>
			{
				return fg_CallSafe
					(
						fg_Move(fToSequence)
					 	, g_ActorSubscription / [pState]
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

							State.f_ProcessSequence();
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
