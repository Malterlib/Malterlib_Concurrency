// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ActorSubscription>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCActorSequencerActor<t_CReturnType>::TCActorSequencerActor(NStr::CStr const &_Name, mint _MaxConcurrency)
		: m_MaxConcurrency(_MaxConcurrency)
		, m_Name(_Name)
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
		using namespace NStr;

		TCPromise<CActorSubscription> Promise;

		if (f_IsDestroyed())
		{
			Promise.f_SetException(DMibErrorInstance("Sequencer '{}' has been aborted"_f << m_Name));
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
	TCFuture<CActorSubscription> TCActorSequencerActor<t_CReturnType>::f_TrySequence(bool _bCanSkip)
	{
		if (_bCanSkip && m_nRunning >= m_MaxConcurrency)
			return {};

		return f_Sequence();
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCActorSequencerActor<t_CReturnType>::f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence)
	{
		using namespace NStr;

		TCPromise<t_CReturnType> Promise;

		if (f_IsDestroyed())
			return Promise <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << m_Name);

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
		using namespace NStr;

		for (auto &ToSequence : m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer '{}' went out of scope"_f << m_Name));
		m_ToSequence.f_Clear();
	}
	
	template <typename t_CReturnType>
	TCFuture<void> TCActorSequencerActor<t_CReturnType>::fp_Destroy()
	{
		using namespace NStr;

		TCPromise<void> AbortPromise;

		for (auto &ToSequence : m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer '{}' aborted"_f << m_Name));

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

	template <typename t_CReturnType>
	TCSequencer<t_CReturnType>::TCSequencer(NStr::CStr const &_Name, mint _MaxConcurrency)
		: mp_Actor(fg_Construct(_Name, _MaxConcurrency))
		, mp_Name(_Name)
	{
	}

	template <typename t_CReturnType>
	TCFuture<void> TCSequencer<t_CReturnType>::f_Destroy() &&
	{
		using namespace NStr;

		if (mp_Actor)
			return fg_Move(mp_Actor).f_Destroy();

		return TCPromise<void>() <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCSequencer<t_CReturnType>::f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence)
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_RunSequenced, fg_Move(_fToSequence)).f_Future();

		return TCPromise<t_CReturnType>() <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCSequencer<t_CReturnType>::f_Sequence()
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_Sequence).f_Future();

		return TCPromise<CActorSubscription>() <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCSequencer<t_CReturnType>::f_TrySequence(bool _bCanSkip)
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_TrySequence, _bCanSkip).f_Future();

		return TCPromise<CActorSubscription>() <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<mint> TCSequencer<t_CReturnType>::f_NumWaiting() const
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_NumWaiting).f_Future();

		return TCPromise<mint>() <<= DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}
}
