// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CType>
	struct TCFutureQueue
	{
		TCFutureQueue(mint _nMaxQueued);

		TCFuture<t_CType> f_Insert(TCFuture<t_CType> &&_Future);

		bool f_IsEmpty() const;
		operator bool () const;

		TCFuture<t_CType> f_PopFirst();

	private:
		NContainer::TCLinkedList<TCFuture<t_CType>> mp_QueuedWrites;
		mint mp_nQueuedWrites = 0;
		mint mp_nMaxQueued = 0;
	};
}

#include "Malterlib_Concurrency_FutureQueue.hpp"
