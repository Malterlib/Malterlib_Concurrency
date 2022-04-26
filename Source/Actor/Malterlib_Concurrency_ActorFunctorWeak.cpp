// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper g_ActorFunctorWeakInit{};
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = g_ActorFunctorWeakInit;
}
