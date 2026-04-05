// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifndef DMibConfig_Concurrency_DebugActorCallstacks
#	define DMibConfig_Concurrency_DebugActorCallstacks 0
#endif

#ifndef DMibConfig_Concurrency_DebugBlockDestroy
#	if defined(DMibDebug)
#		define DMibConfig_Concurrency_DebugBlockDestroy 1
#	else
#		define DMibConfig_Concurrency_DebugBlockDestroy 0
#	endif
#endif

#ifndef DMibConfig_Concurrency_DebugFutures
#	define DMibConfig_Concurrency_DebugFutures 0
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
