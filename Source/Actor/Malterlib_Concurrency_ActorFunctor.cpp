// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctor.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorHelper gc_ActorFunctorInit{}; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	CActorFunctorHelper const &g_ActorFunctor = gc_ActorFunctorInit; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object

	constexpr TCActorFunctorHelper<true> gc_ActorFunctorCoalescedInit{}; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	// NOLINTNEXTLINE(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	TCActorFunctorHelper<true> const &g_ActorFunctorCoalesced = gc_ActorFunctorCoalescedInit;

	namespace NPrivate
	{
		// Returns false when a delivery is already queued; that delivery covers this call
		bool CActorFunctorCoalescedGate::f_Arm()
		{
			return !m_bArmed.f_Exchange(true);
		}

		void CActorFunctorCoalescedGate::f_Disarm()
		{
			m_Generation.f_FetchAdd(1);
			m_bArmed.f_Exchange(false);
		}

		uint32 CActorFunctorCoalescedGate::f_GetGeneration() const
		{
			return m_Generation.f_Load();
		}

		// A newer delivery has stepped the generation, so its gate is left alone
		void CActorFunctorCoalescedGate::f_ReopenOnFailure(uint32 _Generation)
		{
			if (m_Generation.f_Load() == _Generation)
				m_bArmed.f_Exchange(false);
		}
	}
}
