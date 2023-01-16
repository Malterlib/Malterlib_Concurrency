// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCActorSequencerActor : public CActor
	{
		TCActorSequencerActor(mint _MaxConcurrency = 1);
		~TCActorSequencerActor();

		TCActorSequencerActor(TCActorSequencerActor &&_Other) = default;
		TCActorSequencerActor& operator = (TCActorSequencerActor &&_Other) = default;

		TCActorSequencerActor(TCActorSequencerActor const &_Other) = delete;
		TCActorSequencerActor& operator = (TCActorSequencerActor const &_Other) = delete;

		TCFuture<t_CReturnType> f_RunSequenced(TCActorFunctorWeak<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> &&_fToSequence);
		TCFuture<CActorSubscription> f_Sequence();
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
		mint m_MaxConcurrency = 1;
		mint m_nRunning = 0;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorSequencerActor.hpp"
