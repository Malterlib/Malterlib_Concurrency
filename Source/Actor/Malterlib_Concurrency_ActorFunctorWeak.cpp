// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper gc_ActorFunctorWeakInit{}; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = gc_ActorFunctorWeakInit; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object

	constexpr TCActorFunctorWeakHelper<true> gc_ActorFunctorWeakCoalescedInit{}; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	// NOLINTNEXTLINE(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	TCActorFunctorWeakHelper<true> const &g_ActorFunctorWeakCoalesced = gc_ActorFunctorWeakCoalescedInit;
}
