// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper g_ActorFunctorWeakInit{};
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = g_ActorFunctorWeakInit;
}
