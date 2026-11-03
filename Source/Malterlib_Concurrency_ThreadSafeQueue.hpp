// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NContainer
{
#ifdef DMibContainer_UseBoostLockFreeQueue
	template <typename t_CType>
	template <typename tf_CParam0>
	TCThreadSafeQueueListEntry<t_CType>::TCThreadSafeQueueListEntry(tf_CParam0 &&_Param0)
		: mp_Data(fg_Forward<tf_CParam0>(_Param0))
	{
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
		: mp_pEntry(_Other.mp_pEntry)
	{
		_Other.mp_pEntry = nullptr;
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator = (TCThreadSafeQueueEntry &&_Other) -> TCThreadSafeQueueEntry &
	{
		if (mp_pEntry)
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);

		mp_pEntry = _Other.mp_pEntry;
		_Other.mp_pEntry = nullptr;

		return *this;
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::~TCThreadSafeQueueEntry()
	{
		if (mp_pEntry)
		{
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);
			mp_pEntry = nullptr;
		}
	}

	template <typename t_CType, typename t_CAllocator>
	bool TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_IsValid() const
	{
		return mp_pEntry != nullptr;
	}

	template <typename t_CType, typename t_CAllocator>
	t_CType &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_Get()
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator * ()
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType *TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator -> ()
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	t_CType const &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_Get() const
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType const &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator * () const
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType const *TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator -> () const
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	inline_small TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator bool () const
	{
		return f_IsValid();
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
		: mp_pEntry(_pEntry)
	{
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::TCThreadSafeQueue()
	{
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::~TCThreadSafeQueue()
	{
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_Clear()
	{
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueue<t_CType, t_CAllocator>::f_Pop() -> CReturnEntry
	{
		CListEntry *pEntry;
		if (mp_Queue.pop(pEntry))
			return CReturnEntry(pEntry);
		return CReturnEntry(nullptr);
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueue<t_CType, t_CAllocator>::f_PopLIFO() -> CReturnEntry
	{
		DMibPDebugBreak; // Not yet supported
		return CReturnEntry();
	}

	template <typename t_CType, typename t_CAllocator>
	template <typename tf_CParam0>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_Push(tf_CParam0 &&_Param0)
	{
		CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<tf_CParam0>(_Param0));
		mp_Queue.push(pEntry);
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items)
	{
		CListEntry *pEntry = nullptr;
		while (mp_Queue.pop(pEntry))
		{
			o_Items.f_Insert(fg_Move(pEntry->mp_Data));
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
		}
	}

	template <typename t_CType, typename t_CAllocator>
	bool TCThreadSafeQueue<t_CType, t_CAllocator>::f_IsEmpty() const
	{
		return fg_RemoveQualifiers(mp_Queue).empty();
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::fp_Clear()
	{
		CListEntry *pEntry;
		while (mp_Queue.pop(pEntry))
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
	}
#else
	template <typename t_CType>
	template <typename ...tfp_CParams>
	TCThreadSafeQueueListEntry<t_CType>::TCThreadSafeQueueListEntry(tfp_CParams && ...p_Params)
		: mp_Data(fg_Forward<tfp_CParams>(p_Params)...)
	{
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
		: mp_pEntry(_Other.mp_pEntry)
	{
		_Other.mp_pEntry = nullptr;
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator = (TCThreadSafeQueueEntry &&_Other) -> TCThreadSafeQueueEntry &
	{
		if (mp_pEntry)
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);

		mp_pEntry = _Other.mp_pEntry;
		_Other.mp_pEntry = nullptr;

		return *this;
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::~TCThreadSafeQueueEntry()
	{
		if (mp_pEntry)
		{
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);
			mp_pEntry = nullptr;
		}
	}

	template <typename t_CType, typename t_CAllocator>
	bool TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_IsValid() const
	{
		return mp_pEntry != nullptr;
	}

	template <typename t_CType, typename t_CAllocator>
	t_CType &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_Get()
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator * ()
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType *TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator -> ()
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	t_CType const &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::f_Get() const
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType const &TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator * () const
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	mark_nodebug t_CType const *TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator -> () const
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator>
	inline_small TCThreadSafeQueueEntry<t_CType, t_CAllocator>::operator bool () const
	{
		return f_IsValid();
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueueEntry<t_CType, t_CAllocator>::TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
		: mp_pEntry(_pEntry)
	{
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::fp_Clear()
	{
		CListEntry *pEntry = mp_Queue.f_Pop();
		while (pEntry)
		{
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
			pEntry = mp_Queue.f_Pop();
		}
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::TCThreadSafeQueue()
	{
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::TCThreadSafeQueue(TCThreadSafeQueue const &_Other)
	{
		DMibLock(_Other.mp_Lock);
		auto Iter = _Other.mp_Queue.f_GetIterator();
		while (Iter)
		{
			mp_Queue.f_InsertLast(fg_ConstructObject<CListEntry>(t_CAllocator(), Iter->mp_Data));
			++Iter;
		}
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::TCThreadSafeQueue(TCThreadSafeQueue &&_Other)
	{
		DMibLock(_Other.mp_Lock);
		mp_Queue = fg_Move(_Other.mp_Queue);
	}

	template <typename t_CType, typename t_CAllocator>
	TCThreadSafeQueue<t_CType, t_CAllocator>::~TCThreadSafeQueue()
	{
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_Clear()
	{
		DMibLock(mp_Lock);
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueue<t_CType, t_CAllocator>::f_Pop() -> CReturnEntry
	{
		CListEntry *pEntry;
		{
			DMibLock(mp_Lock);
			pEntry = mp_Queue.f_Pop();
		}

		return CReturnEntry(pEntry);
	}

	template <typename t_CType, typename t_CAllocator>
	auto TCThreadSafeQueue<t_CType, t_CAllocator>::f_PopLIFO() -> CReturnEntry
	{
		CListEntry *pEntry;
		{
			DMibLock(mp_Lock);
			pEntry = mp_Queue.f_GetLast();
			if (pEntry)
				pEntry->mp_Link.f_Unlink();
		}

		return CReturnEntry(pEntry);
	}

	template <typename t_CType, typename t_CAllocator>
	template <typename ...tfp_CParams>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_Push(tfp_CParams && ...p_Params)
	{
		CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<tfp_CParams>(p_Params)...);
		{
			DMibLock(mp_Lock);
			mp_Queue.f_InsertLast(pEntry);
		}
	}

	template <typename t_CType, typename t_CAllocator>
	void TCThreadSafeQueue<t_CType, t_CAllocator>::f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items)
	{
		DMibLock(mp_Lock);

		CListEntry *pEntry = mp_Queue.f_Pop();
		while (pEntry)
		{
			o_Items.f_Insert(fg_Move(pEntry->mp_Data));
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
			pEntry = mp_Queue.f_Pop();
		}
	}

	template <typename t_CType, typename t_CAllocator>
	bool TCThreadSafeQueue<t_CType, t_CAllocator>::f_IsEmpty() const
	{
		DMibLock(mp_Lock);
		return mp_Queue.f_IsEmpty();
	}
#endif
}
