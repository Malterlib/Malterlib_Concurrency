// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifndef DMibConfig_Concurrency_DebugActorCallstacks
#	ifdef DMibDebug
#		define DMibConfig_Concurrency_DebugActorCallstacks 1
#	else
#		define DMibConfig_Concurrency_DebugActorCallstacks 0
#	endif
#endif

#ifndef DMibConfig_Concurrency_DebugBlockDestroy
#	if defined(DMibDebug)
#		define DMibConfig_Concurrency_DebugBlockDestroy 1
#	else
#		define DMibConfig_Concurrency_DebugBlockDestroy 0
#	endif
#endif

#if DMibConfig_Concurrency_DebugActorCallstacks
#include <Mib/Container/LinkedList>
#endif

namespace NMib::NConcurrency
{
#if DMibConfig_Concurrency_DebugActorCallstacks
	using CAsyncCallstacks = NContainer::TCLinkedList<NException::CCallstack>;
	//using CAsyncCallstacks = NContainer::TCVector<NException::CCallstack>;
#endif
}
