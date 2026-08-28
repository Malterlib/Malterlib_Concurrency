// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/Coroutine>

namespace NMib::NConcurrency
{
	// Counts completion transfers the kernel may still reference a consumer's buffers through.
	// Holds are taken on the actor and released from whichever thread destroys the functor that
	// carries them; the drain fence happens on the actor, so this count is the cross-thread truth
	// destruction has to wait on
	struct CIoCompletionOpTracker
	{
		// Set once the owner started draining; the release that empties the count resolves
		static constexpr uint32 mc_DrainFlag = 0x80000000u;

		NStorage::TCOptionalClearOnMove<TCPromise<void>> m_DrainPromise;
		NAtomic::TCAtomic<uint32> m_State{0};

	private:
		friend struct CIoCompletionOpHold;

		void fp_Acquire();
		void fp_Release();
	};

	// One hold on the tracker, taken at construction and released when the hold is destroyed.
	// Captured by a completion functor, so a functor that never runs — a refused submission, a
	// throw on the way to the loop, teardown dropping it — releases exactly like one that did
	struct CIoCompletionOpHold
	{
		CIoCompletionOpHold() = default;
		explicit CIoCompletionOpHold(NStorage::TCSharedPointer<CIoCompletionOpTracker> const &_pTracker);
		CIoCompletionOpHold(CIoCompletionOpHold &&_Other);
		CIoCompletionOpHold(CIoCompletionOpHold const &) = delete;
		~CIoCompletionOpHold();

		CIoCompletionOpHold &operator = (CIoCompletionOpHold &&_Other);
		CIoCompletionOpHold &operator = (CIoCompletionOpHold const &) = delete;

	private:
		void fp_Release();

		NStorage::TCSharedPointer<CIoCompletionOpTracker> mp_pTracker;
	};
}

#include "Malterlib_Concurrency_IoCompletionOpTracker.hpp"
