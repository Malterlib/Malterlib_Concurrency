// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CType>
	struct TCFutureQueue
	{
		TCFutureQueue(umint _nMaxQueued);

		TCFuture<t_CType> f_Insert(TCFuture<t_CType> &&_Future);

		bool f_IsEmpty() const;
		operator bool () const;

		TCFuture<t_CType> f_PopFirst();

	private:
		NContainer::TCLinkedList<TCFuture<t_CType>> mp_QueuedWrites;
		umint mp_nQueuedWrites = 0;
		umint mp_nMaxQueued = 0;
	};
}

#include "Malterlib_Concurrency_FutureQueue.hpp"
