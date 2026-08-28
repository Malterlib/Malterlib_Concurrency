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

#ifndef DMibConfig_Concurrency_SchedulerStats
#	if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
#		define DMibConfig_Concurrency_SchedulerStats 1
#	else
#		define DMibConfig_Concurrency_SchedulerStats 0
#	endif
#endif

#ifndef DMibConfig_Concurrency_FairScheduling
#	define DMibConfig_Concurrency_FairScheduling 0
#endif

#ifndef DMibConfig_Concurrency_LocalFirstScheduler
#	define DMibConfig_Concurrency_LocalFirstScheduler 1
#endif

// Cooperative distribution of local backlogs to idle cores: excess jobs above the target queue
// size are shipped in target-sized chunks to claimed idle cores, so every wakeup carries a batch
// of runnable work. Disable to keep all locally scheduled work on the scheduling thread.
#ifndef DMibConfig_Concurrency_LocalFirstDistribution
#	define DMibConfig_Concurrency_LocalFirstDistribution 1
#endif

// Target number of jobs a pool thread keeps on its local queue when distributing; excess beyond
// this is shipped to claimed idle cores in chunks of at most this size, splitting large backlogs
// across several cores
#ifndef DMibConfig_Concurrency_LocalQueueTargetSize
#	define DMibConfig_Concurrency_LocalQueueTargetSize 32
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
