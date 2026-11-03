// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper gc_ActorFunctorWeakInit{};
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = gc_ActorFunctorWeakInit;

	constexpr TCActorFunctorWeakHelper<true> gc_ActorFunctorWeakCoalescedInit{};
	TCActorFunctorWeakHelper<true> const &g_ActorFunctorWeakCoalesced = gc_ActorFunctorWeakCoalescedInit;
}
