// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctor.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorHelper g_ActorFunctorInit{};
	CActorFunctorHelper const &g_ActorFunctor = g_ActorFunctorInit;
}
