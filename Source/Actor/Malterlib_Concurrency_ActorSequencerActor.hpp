// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ActorSubscription>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCActorSequencerActor<t_CReturnType>::TCActorSequencerActor(mint _MaxConcurrency)
		: m_MaxConcurrency(_MaxConcurrency)
	{
	}

	template <typename t_CReturnType>
	mint TCActorSequencerActor<t_CReturnType>::f_NumWaiting() const
	{
		return m_ToSequence.f_GetLen();
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCActorSequencerActor<t_CReturnType>::f_Sequence()
	{
		TCPromise<CActorSubscription> Promise;

		if (f_IsDestroyed())
		{
			Promise.f_SetException(DMibErrorInstance("Sequencer has been aborted"));
			return Promise.f_MoveFuture();
		}

		auto &ToSequence = m_ToSequence.f_Insert(CToSequenceEntry{});
		ToSequence.m_fToSequence = [Promise](CActorSubscription &&_DoneSubscription) -> TCFuture<t_CReturnType>
			{
				TCPromise<t_CReturnType> NewPromise;

				if (!Promise.f_IsSet())
					Promise.f_SetResult(fg_Move(_DoneSubscription));

				if constexpr (NTraits::TCIsSame<t_CReturnType, void>::mc_Value)
					return NewPromise <<= g_Void;
				else if constexpr (NTraits::TCHasTrivialDefaultConstructor<t_CReturnType>::mc_Value)
					return NewPromise <<= t_CReturnType{};
				else
					return NewPromise <<= DMibErrorInstance("Value");
			}
		;

		ToSequence.m_Promise.f_Future() > [Promise](TCAsyncResult<t_CReturnType> &&_Result)
			{
				if (!Promise.f_IsSet() && !_Result)
					Promise.f_SetException(fg_Move(_Result));
			}
		;

		fp_ProcessSequence();

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCActorSequencerActor<t_CReturnType>::f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence)
	{
		TCPromise<t_CReturnType> Promise;

		if (f_IsDestroyed())
			return Promise <<= DMibErrorInstance("Sequencer has been aborted");

		auto &ToSequence = m_ToSequence.f_Insert(CToSequenceEntry{fg_Move(Promise)});
		ToSequence.m_fToSequence = [fToSequence = fg_Move(_fToSequence)](CActorSubscription &&_DoneSubscription) mutable -> TCFuture<t_CReturnType>
			{
				return fg_Move(fToSequence)(fg_Move(_DoneSubscription)).f_Future();
			}
		;

		auto Future = ToSequence.m_Promise.f_Future();

		fp_ProcessSequence();

		return fg_Move(Future);
	}
	
	template <typename t_CReturnType>
	TCActorSequencerActor<t_CReturnType>::~TCActorSequencerActor()
	{
		for (auto &ToSequence : m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer went out of scope"));
		m_ToSequence.f_Clear();
	}
	
	template <typename t_CReturnType>
	TCFuture<void> TCActorSequencerActor<t_CReturnType>::fp_Destroy()
	{
		TCPromise<void> AbortPromise;

		for (auto &ToSequence : m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer aborted"));

		m_ToSequence.f_Clear();

		if (m_nRunning)
			return (*(m_AbortPromise = fg_Move(AbortPromise))).f_Future();
		else
			return AbortPromise <<= g_Void;
	}
	
	template <typename t_CReturnType>
	void TCActorSequencerActor<t_CReturnType>::fp_ProcessSequence()
	{
		if (m_ToSequence.f_IsEmpty() || m_nRunning >= m_MaxConcurrency)
			return;
		
		++m_nRunning;
		
		auto ToSequence = m_ToSequence.f_Pop();
		g_Dispatch / [this, fToSequence = fg_Move(ToSequence.m_fToSequence)]() mutable -> TCFuture<t_CReturnType>
			{
				return fg_CallSafe
					(
						fg_Move(fToSequence)
						, g_ActorSubscription / [this]
						{
							--m_nRunning;

							if (f_IsDestroyed())
							{
								if (m_nRunning == 0)
								{
									if (m_AbortPromise && !(*m_AbortPromise).f_IsSet())
										(*m_AbortPromise).f_SetResult();
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
