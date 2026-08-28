// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/Coroutine>

namespace NMib::NConcurrency
{
	// Tracks kernel references to consumer buffers. Holds are acquired on the actor and may be released on any thread;
	// destruction waits for the actor's drain fence.
	struct CIoCompletionOpTracker
	{
		static constexpr uint32 mc_DrainFlag = 0x80000000u; // Once set, the release of the final hold resolves the drain fence.

		NStorage::TCOptionalClearOnMove<TCPromise<void>> m_DrainPromise;
		NAtomic::TCAtomic<uint32> m_State{0};

	private:
		friend struct CIoCompletionOpHold;

		void fp_Acquire();
		void fp_Release();
	};

	// Releases its tracker hold on destruction, including when a completion functor never runs.
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
