// Copyright © 2017 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCActorSequencer
	{
		TCActorSequencer(mint _MaxConcurrency = 1);
		~TCActorSequencer();
		
		template <typename tf_FToSequence>
		TCFuture<t_CReturnType> operator / (tf_FToSequence &&_fToSequence);
		
		TCFuture<void> f_Abort();
		
	private:
		struct CToSequenceEntry
		{
			TCPromise<t_CReturnType> m_Promise;
			NFunction::TCFunctionMovable<TCFuture<t_CReturnType> ()> m_fToSequence;
		};
		
		struct CState
		{
			NContainer::TCLinkedList<CToSequenceEntry> m_ToSequence;
			NStorage::TCOptional<TCPromise<void>> m_AbortPromise;
			mint m_MaxConcurrency = 1;
			mint m_nRunning = 0;
			bool m_bDestroyed = false;
		};
		
		void fp_ProcessSequence();
		
		NStorage::TCSharedPointer<CState> mp_pState;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorSequencer.hpp"
