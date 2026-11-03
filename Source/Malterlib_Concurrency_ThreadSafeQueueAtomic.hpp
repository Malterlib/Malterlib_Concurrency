// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NContainer
{
	template <typename t_CType>
	template <typename tf_CParam0>
	TCThreadSafeQueueAtomicListEntry<t_CType>::TCThreadSafeQueueAtomicListEntry(tf_CParam0 &&_Param0)
		: mp_Data(fg_Forward<tf_CParam0>(_Param0))
	{
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicEntry &&_Other)
		: mp_pEntry(_Other.mp_pEntry)
	{
		_Other.mp_pEntry = nullptr;
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	auto TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator = (TCThreadSafeQueueAtomicEntry &&_Other) -> TCThreadSafeQueueAtomicEntry &
	{
		if (mp_pEntry)
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);

		mp_pEntry = _Other.mp_pEntry;
		_Other.mp_pEntry = nullptr;

		return *this;
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::~TCThreadSafeQueueAtomicEntry()
	{
		if (mp_pEntry)
		{
			fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);
			mp_pEntry = nullptr;
		}
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	bool TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::f_IsValid() const
	{
		return mp_pEntry != nullptr;
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	t_CType &TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::f_Get()
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	mark_nodebug t_CType &TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator * ()
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	mark_nodebug t_CType *TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator -> ()
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	t_CType const &TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::f_Get() const
	{
		DMibRequire(mp_pEntry);
		return mp_pEntry->mp_Data;
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	mark_nodebug t_CType const &TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator * () const
	{
		return f_Get();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	mark_nodebug t_CType const *TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator -> () const
	{
		return &f_Get();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	inline_small TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::operator bool () const
	{
		return f_IsValid();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator, t_DefaultSize>::TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicListEntry<t_CType> *_pEntry)
		: mp_pEntry(_pEntry)
	{
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::TCThreadSafeQueueAtomic()
	{
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::~TCThreadSafeQueueAtomic()
	{
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	void TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_Clear()
	{
		fp_Clear();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	auto TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_Pop() -> CReturnEntry
	{
		CListEntry *pEntry;
		if (mp_Queue.pop(pEntry))
			return CReturnEntry(pEntry);
		return CReturnEntry(nullptr);
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	auto TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_PopLIFO() -> CReturnEntry
	{
		DMibPDebugBreak; // Not yet supported
		return CReturnEntry();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	template <typename tf_CParam0>
	void TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_Push(tf_CParam0 &&_Param0)
	{
		CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<tf_CParam0>(_Param0));
		mp_Queue.push(pEntry);
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	void TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items)
	{
		CListEntry *pEntry = nullptr;
		while (mp_Queue.pop(pEntry))
		{
			o_Items.f_Insert(fg_Move(pEntry->mp_Data));
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
		}
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	bool TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::f_IsEmpty() const
	{
		return fg_RemoveQualifiers(mp_Queue).empty();
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	template <typename t_CValue>
	t_CValue *TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::TCStdAllocator<t_CValue>::allocate(size_t _nItems)
	{
		return (t_CValue *)t_CAllocator::f_Alloc(_nItems * sizeof(t_CValue));
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	template <typename t_CValue>
	void TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::TCStdAllocator<t_CValue>::deallocate(t_CValue *_pMemory, size_t _nItems)
	{
		t_CAllocator::f_Free(_pMemory, sizeof(t_CValue) * _nItems);
	}

	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	void TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>::fp_Clear()
	{
		CListEntry *pEntry;
		while (mp_Queue.pop(pEntry))
			fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
	}
}
