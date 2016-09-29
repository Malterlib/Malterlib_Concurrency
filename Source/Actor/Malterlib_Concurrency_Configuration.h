// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifndef DMibConcurrencyDebugActorCallstacks
#	ifdef DMibDebug
#		define DMibConcurrencyDebugActorCallstacks 1
#	else
#		define DMibConcurrencyDebugActorCallstacks 0
#	endif
#endif

#if DMibConcurrencyDebugActorCallstacks
#include <Mib/Container/LinkedList>
#endif

namespace NMib
{
	namespace NConcurrency
	{
#if DMibConcurrencyDebugActorCallstacks
		using CAsyncCallstacks = NContainer::TCLinkedList<NException::CCallstack>;
		//using CAsyncCallstacks = NContainer::TCVector<NException::CCallstack>;
#endif
	}
}
