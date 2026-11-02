// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Annotations that the malterlib-* clang-tidy checks read and verify. They expand to nothing for other compilers.
//
// The phases of the life of an actor. Once an actor is in a phase it stays in it, because the flags that tell are only ever set:
//   Destroy: the actor is destroyed. fp_DestroyInternal sets the flag f_IsDestroyed() returns, then runs fp_Destroy.
//   StopApp: the app stops. f_StopApp sets m_bStoppingApp, then runs fp_StopApp.
//
// DMibActorEntries(Phases...): the entries of a collection are only removed in the phases, and with Owner by the call that added
//   them with a fresh key. A coroutine may then keep a reference to an entry across a suspension, with a guard of the phases alive.
// DMibActorPhase(Phases...): a function that only runs in one of the phases. Overrides inherit it.
// DMibActorPhaseEntry(Phase): sets the flag of the phase, then runs it.
// DMibActorPhaseFlag(Phase): the flag of the phase, or what returns it.
// DMibActorPhaseGuard(Phases...): ends the coroutine on resume once the actor is in one of the phases, while its result lives.
//
// DMibRunsToCompletion: a function, or the member functions of a class that isn't an actor, that only ends by finishing, never by
//   being aborted, because it only suspends on concurrent actors. Its callers join it, so it may refer to their data.
// DMibRunsToCompletionEntry: where running to completion begins, by switching to a concurrent actor before it first suspends.

#if defined(__clang__)
#	define DMibActorEntries(...) [[clang::annotate("malterlib-actor-entries", #__VA_ARGS__)]]
#	define DMibActorPhase(...) [[clang::annotate("malterlib-actor-phase", #__VA_ARGS__)]]
#	define DMibActorPhaseEntry(...) [[clang::annotate("malterlib-actor-phase-entry", #__VA_ARGS__)]]
#	define DMibActorPhaseFlag(...) [[clang::annotate("malterlib-actor-phase-flag", #__VA_ARGS__)]]
#	define DMibActorPhaseGuard(...) [[clang::annotate("malterlib-actor-phase-guard", #__VA_ARGS__)]]
#	define DMibRunsToCompletion [[clang::annotate("malterlib-runs-to-completion")]]
#	define DMibRunsToCompletionEntry [[clang::annotate("malterlib-runs-to-completion-entry")]]
#else
#	define DMibActorEntries(...)
#	define DMibActorPhase(...)
#	define DMibActorPhaseEntry(...)
#	define DMibActorPhaseFlag(...)
#	define DMibActorPhaseGuard(...)
#	define DMibRunsToCompletion
#	define DMibRunsToCompletionEntry
#endif
