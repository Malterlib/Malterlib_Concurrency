// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Storage/Optional>
#include <Mib/Concurrency/ActorFunctorWeak>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCActorSequencerActor : public CActor
	{
		TCActorSequencerActor(NStr::CStr const &_Name, mint _MaxConcurrency = 1);
		~TCActorSequencerActor();

		TCActorSequencerActor(TCActorSequencerActor &&_Other) = default;
		TCActorSequencerActor& operator = (TCActorSequencerActor &&_Other) = default;

		TCActorSequencerActor(TCActorSequencerActor const &_Other) = delete;
		TCActorSequencerActor& operator = (TCActorSequencerActor const &_Other) = delete;

		TCFuture<t_CReturnType> f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence);
		TCFuture<CActorSubscription> f_Sequence();
		TCFuture<CActorSubscription> f_TrySequence(bool _bCanSkip);
		mint f_NumWaiting() const;

	private:
		struct CToSequenceEntry
		{
			TCPromise<t_CReturnType> m_Promise;
			NFunction::TCFunctionMovable<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> m_fToSequence;
		};

		void fp_ProcessSequence();
		TCFuture<void> fp_Destroy() override;

		NContainer::TCLinkedList<CToSequenceEntry> m_ToSequence;
		NStorage::TCOptional<TCPromise<void>> m_AbortPromise;
		NStr::CStr m_Name;
		mint m_MaxConcurrency = 1;
		mint m_nRunning = 0;
	};

	template <typename t_CReturnType>
	struct TCSequencer
	{
		TCSequencer(NStr::CStr const &_Name, mint _MaxConcurrency = 1);

		TCFuture<void> f_Destroy() &&;
		TCFuture<t_CReturnType> f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence);
		TCFuture<CActorSubscription> f_Sequence();
		TCFuture<CActorSubscription> f_TrySequence(bool _bCanSkip = true);
		TCFuture<mint> f_NumWaiting() const;

	private:
		TCActor<TCActorSequencerActor<t_CReturnType>> mp_Actor;
		NStr::CStr mp_Name;
	};

	using CSequencer = TCSequencer<void>;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorSequencerActor.hpp"
