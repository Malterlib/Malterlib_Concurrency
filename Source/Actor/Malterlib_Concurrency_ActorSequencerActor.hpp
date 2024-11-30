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

		if (f_IsDestroyed())
			co_return DMibErrorInstance("Sequencer '{}' has been aborted"_f << m_Name);

		TCPromiseFuturePair<CActorSubscription> Promise;

		auto &ToSequence = m_ToSequence.f_Insert(CToSequenceEntry{});
		ToSequence.m_fToSequence = [Promise = Promise.m_Promise](CActorSubscription _DoneSubscription) -> TCFuture<t_CReturnType>
			{
				if (!Promise.f_IsSet())
					Promise.f_SetResult(fg_Move(_DoneSubscription));

				if constexpr (NTraits::TCIsSame<t_CReturnType, void>::mc_Value)
					co_return {};
				else if constexpr (NTraits::TCHasTrivialDefaultConstructor<t_CReturnType>::mc_Value)
					co_return t_CReturnType{};
				else
					co_return DMibErrorInstance("Value");
			}
		;

		ToSequence.m_Promise.f_Future() > [Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<t_CReturnType> &&_Result)
			{
				if (!Promise.f_IsSet() && !_Result)
					Promise.f_SetException(fg_Move(_Result));
			}
		;

		fp_ProcessSequence();

		co_return co_await fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCActorSequencerActor<t_CReturnType>::f_TrySequence(bool _bCanSkip)
	{
		if (_bCanSkip && m_nRunning >= m_MaxConcurrency)
			co_return {};

		co_return co_await f_Sequence();
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCActorSequencerActor<t_CReturnType>::f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription _DoneSubscription)> _fToSequence)
	{
		using namespace NStr;

		TCPromiseFuturePair<t_CReturnType> Promise;

		if (f_IsDestroyed())
			co_return DMibErrorInstance("Sequencer '{}' has been aborted"_f << m_Name);

		auto &ToSequence = m_ToSequence.f_Insert(CToSequenceEntry{fg_Move(Promise.m_Promise)});
		ToSequence.m_fToSequence = [fToSequence = fg_Move(_fToSequence)](CActorSubscription &&_DoneSubscription) mutable -> TCFuture<t_CReturnType>
			{
				return fg_Move(fToSequence)(fg_Move(_DoneSubscription));
			}
		;

		fp_ProcessSequence();

		co_return co_await fg_Move(Promise.m_Future);
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

		for (auto &ToSequence : m_ToSequence)
			ToSequence.m_Promise.f_SetException(DMibErrorInstance("Actor sequencer '{}' aborted"_f << m_Name));

		m_ToSequence.f_Clear();

		if (m_nRunning)
		{
			TCPromiseFuturePair<void> AbortPromise;
			m_AbortPromise = fg_Move(AbortPromise.m_Promise);
			return fg_Move(AbortPromise.m_Future);
		}
		else
			return g_Void;
	}
	
	template <typename t_CReturnType>
	void TCActorSequencerActor<t_CReturnType>::fp_ProcessSequence()
	{
		if (m_ToSequence.f_IsEmpty() || m_nRunning >= m_MaxConcurrency)
			return;
		
		++m_nRunning;
		
		auto ToSequence = m_ToSequence.f_Pop();
		auto Promise = ToSequence.m_Promise;
		ToSequence.m_fToSequence
			(
				g_ActorSubscription / [this]
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
			> [Promise = fg_Move(Promise)](TCAsyncResult<t_CReturnType> &&_Result)
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

		return DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCSequencer<t_CReturnType>::f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription _DoneSubscription)> _fToSequence)
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_RunSequenced, fg_Move(_fToSequence));

		return DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCSequencer<t_CReturnType>::f_Sequence()
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_Sequence);

		return DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<CActorSubscription> TCSequencer<t_CReturnType>::f_TrySequence(bool _bCanSkip)
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_TrySequence, _bCanSkip);

		return DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}

	template <typename t_CReturnType>
	TCFuture<mint> TCSequencer<t_CReturnType>::f_NumWaiting() const
	{
		using namespace NStr;

		if (mp_Actor)
			return mp_Actor(&TCActorSequencerActor<t_CReturnType>::f_NumWaiting);

		return DMibErrorInstance("Sequencer '{}' has been aborted"_f << mp_Name);
	}
}
