// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// These defaults must not be reachable from a precompiled header: the MalterlibMemoryManagerFlavor
// build property overrides them with per-file defines, which conflict with macros baked into a PCH

#ifndef DMibConfig_Concurrency_EagerArenaGC
#	if defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#		define DMibConfig_Concurrency_EagerArenaGC 3
#	else
#		define DMibConfig_Concurrency_EagerArenaGC 1
#	endif
#endif

#ifndef DMibConfig_Concurrency_ArenaGCIntervalCycles
#	define DMibConfig_Concurrency_ArenaGCIntervalCycles (umint(1) << 22)
#endif
