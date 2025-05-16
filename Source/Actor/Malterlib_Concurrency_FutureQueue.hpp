// Copyright © 2025 Unbroken AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CType>
	TCFutureQueue<t_CType>::TCFutureQueue(mint _nMaxQueued)
		: mp_nMaxQueued(_nMaxQueued)
	{
	}

	template <typename t_CType>
	TCFuture<t_CType> TCFutureQueue<t_CType>::f_Insert(TCFuture<t_CType> &&_Future)
	{
		mp_QueuedWrites.f_InsertLast(fg_Move(_Future));

		if (mp_nQueuedWrites >= mp_nMaxQueued)
			return mp_QueuedWrites.f_PopFirst();

		++mp_nQueuedWrites;
		
		return {};
	}

	template <typename t_CType>
	bool TCFutureQueue<t_CType>::f_IsEmpty() const
	{
		return mp_nQueuedWrites == 0;
	}

	template <typename t_CType>
	TCFutureQueue<t_CType>::operator bool () const
	{
		return mp_nQueuedWrites != 0;
	}

	template <typename t_CType>
	TCFuture<t_CType> TCFutureQueue<t_CType>::f_PopFirst()
	{
		DMibFastCheck(mp_nQueuedWrites >= 1);

		--mp_nQueuedWrites;
		return mp_QueuedWrites.f_PopFirst();
	}
}
