// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// These defaults must not be reachable from a precompiled header: the MalterlibMemoryManagerFlavor
// build property overrides them with per-file defines, which conflict with macros baked into a PCH

// Eager owner-side arena garbage collection: pending cross-thread frees are collected at every
// processed entry boundary (the has-pending guard makes the idle case a cheap check) and at pool
// and separate-thread wake/park boundaries. With the SubSlabBitmaps free store this measures
// ~17% faster and far more consistent on UiStress (272-274 fps vs 227-250 on a 13700K), but
// ~22% slower on M-series macOS (124 fps vs 159 with the lazy heuristics), so the default is
// per platform. Off = the lazy heuristics (collect every 64th in-drain entry plus an empty-runs
// backstop), which also measure slightly better for the ArenaBlockLists and MiMalloc memory
// managers. Pair with DMibConfig_Memory_EagerOwnerGC, which tunes the matching owner
// full-release cadence.
// 2 = timed: collect at entry boundaries, but at most once per
// DMibConfig_Concurrency_ArenaGCIntervalCycles per thread — globally uniform in time regardless
// of drain shapes and entry rates, and remote-free reap batches amortize over a full interval
// The MalterlibMemoryManagerFlavor build property emits this define as 0 for the BlockLists
// flavor and for the non-Malterlib memory managers; the per-platform defaults below apply to the
// Bitmap and SubSlabLists flavors.
#ifndef DMibConfig_Concurrency_EagerArenaGC
#	if defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#		define DMibConfig_Concurrency_EagerArenaGC 3
#	else
#		define DMibConfig_Concurrency_EagerArenaGC 1
#	endif
#endif

// Minimum cycles between per-thread arena collects in timed mode (~1.3 ms at 3.2 GHz; measured
// best on M-series where it matches the lazy heuristics at 157 vs 159 fps — on a 13700K timed
// loses the eager win at every tested interval, so it does not replace mode 1)
#ifndef DMibConfig_Concurrency_ArenaGCIntervalCycles
#	define DMibConfig_Concurrency_ArenaGCIntervalCycles (umint(1) << 22)
#endif
