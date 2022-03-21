// Copyright © 2017 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCActorSequencerAsync
	{
		TCActorSequencerAsync(mint _MaxConcurrency = 1);
		~TCActorSequencerAsync();

		TCActorSequencerAsync(TCActorSequencerAsync &&_Other) = default;
		TCActorSequencerAsync& operator = (TCActorSequencerAsync &&_Other) = default;

		TCActorSequencerAsync(TCActorSequencerAsync const &_Other) = delete;
		TCActorSequencerAsync& operator = (TCActorSequencerAsync const &_Other) = delete;

		template <typename tf_FToSequence>
		TCFuture<t_CReturnType> operator / (tf_FToSequence &&_fToSequence);
		TCFuture<CActorSubscription> f_Sequence()
			requires (NTraits::TCIsSame<t_CReturnType, void>::mc_Value)
		;
		TCFuture<void> f_Abort();
		mint f_NumWaiting() const;
		
	private:
		struct CToSequenceEntry
		{
			TCPromise<t_CReturnType> m_Promise;
			NFunction::TCFunctionMovable<TCFuture<t_CReturnType> (CActorSubscription &&_DoneSubscription)> m_fToSequence;
		};
		
		struct CState : public NStorage::TCSharedPointerIntrusiveBase<>
		{
			void f_ProcessSequence();

			NContainer::TCLinkedList<CToSequenceEntry> m_ToSequence;
			NStorage::TCOptional<TCPromise<void>> m_AbortPromise;
			mint m_MaxConcurrency = 1;
			mint m_nRunning = 0;
			bool m_bDestroyed = false;
		};

		NStorage::TCSharedPointer<CState> mp_pState;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorSequencerAsync.hpp"
