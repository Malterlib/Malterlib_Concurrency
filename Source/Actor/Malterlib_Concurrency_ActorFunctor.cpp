 // Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctor.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorHelper g_ActorFunctorInit{};
	CActorFunctorHelper const &g_ActorFunctor = g_ActorFunctorInit;
}
