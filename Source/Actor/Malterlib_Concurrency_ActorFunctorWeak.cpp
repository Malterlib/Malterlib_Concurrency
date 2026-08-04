// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper g_ActorFunctorWeakInit{};
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = g_ActorFunctorWeakInit;

	constexpr TCActorFunctorWeakHelper<true> g_ActorFunctorWeakCoalescedInit{};
	TCActorFunctorWeakHelper<true> const &g_ActorFunctorWeakCoalesced = g_ActorFunctorWeakCoalescedInit;
}
