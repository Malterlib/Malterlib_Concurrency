// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	template <typename t_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::TCConcurrentRunQueueEntryHolder(t_CBaseEntry *_pEntry)
		: mp_pEntry(_pEntry)
	{
	}

	template <typename t_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::TCConcurrentRunQueueEntryHolder(TCConcurrentRunQueueEntryHolder &&_Other)
		: mp_pEntry(fg_Exchange(_Other.mp_pEntry, nullptr))
	{
	}

	template <typename t_CBaseEntry>
	auto TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::operator = (TCConcurrentRunQueueEntryHolder &&_Other) -> TCConcurrentRunQueueEntryHolder &
	{
		mp_pEntry = fg_Exchange(_Other.mp_pEntry, nullptr);

		return *this;
	}

	template <typename t_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::~TCConcurrentRunQueueEntryHolder()
	{
		if (mp_pEntry)
			mp_pEntry->f_Cleanup();
	}

	template <typename t_CBaseEntry>
	bool TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::f_IsEmpty() const
	{
		return !mp_pEntry;
	}

	template <typename t_CBaseEntry>
	t_CBaseEntry *TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::operator -> ()
	{
		return mp_pEntry;
	}

	template <typename t_CBaseEntry>
	t_CBaseEntry *TCConcurrentRunQueueEntryHolder<t_CBaseEntry>::f_Detach()
	{
		return fg_Exchange(mp_pEntry, nullptr);
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::~TCConcurrentRunQueue()
	{
		auto *pEntry = mp_pFirstQueued.f_Exchange(nullptr, NAtomic::gc_MemoryOrder_Acquire);
		if (!pEntry)
			return;

		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			pNextEntry->f_Cleanup();
			pEntry = pNextEntry;
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::TCConcurrentRunQueue()
	{
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::CLocalQueueData::~CLocalQueueData()
	{
		while (auto *pEntry = m_LocalQueue.f_Pop())
		{
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<t_CEntry *>(pEntry));
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	auto TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::CLocalQueueData::f_PopFirst() -> t_CBaseEntry *
	{
		auto *pEntry = m_LocalQueue.f_GetFirst();
		if (!pEntry)
			return nullptr;

		pEntry->m_Link.f_UnsafeUnlink();
		--m_nEntries;

		return pEntry;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddChainToQueue(t_CBaseEntry *_pFirstEntry, t_CBaseEntry *_pLastEntry)
	{
		while (true)
		{
			auto *pFirstQueued = mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			_pLastEntry->m_pNextQueued.f_Store(pFirstQueued, NAtomic::gc_MemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstQueued, _pFirstEntry, NAtomic::gc_MemoryOrder_Release, NAtomic::gc_MemoryOrder_Relaxed))
				break;
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	auto TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::fs_QueueEntry(FQueueDispatch &&_fFunctor) -> NStorage::TCUniquePointer<t_CEntry>
	{
		return fg_Construct(fg_Move(_fFunctor));
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueIfEmpty(NStorage::TCUniquePointer<t_CEntry> &&_pEntry)
	{
		t_CEntry *pNewEntry = _pEntry.f_Get();
		pNewEntry->m_pNextQueued.f_Store(nullptr, NAtomic::gc_MemoryOrder_Relaxed);

		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			if (pFirstEntry)
				return false;

			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::gc_MemoryOrder_Release, NAtomic::gc_MemoryOrder_Relaxed))
			{
				_pEntry.f_Detach();
				return true;
			}
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueue(NStorage::TCUniquePointer<t_CEntry> &&_pEntry)
	{
		t_CEntry *pNewEntry = _pEntry.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::gc_MemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::gc_MemoryOrder_Release, NAtomic::gc_MemoryOrder_Relaxed))
				break;
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocal(NStorage::TCUniquePointer<t_CEntry> &&_pEntry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _pEntry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pEntry);
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocalFirst(NStorage::TCUniquePointer<t_CEntry> &&_pEntry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _pEntry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_InsertFirst(pEntry);
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueue(TCConcurrentRunQueueEntryHolder<t_CBaseEntry> &&_Entry)
	{
		auto *pNewEntry = _Entry.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::gc_MemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::gc_MemoryOrder_Release, NAtomic::gc_MemoryOrder_Relaxed))
				break;
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocal(TCConcurrentRunQueueEntryHolder<t_CBaseEntry> &&_Entry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _Entry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pEntry);
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocalFirst(TCConcurrentRunQueueEntryHolder<t_CBaseEntry> &&_Entry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _Entry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_InsertFirst(pEntry);
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueue(FQueueDispatch &&_fFunctor)
	{
		NStorage::TCUniquePointer<t_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_fFunctor));
		t_CEntry *pNewEntry = pNewEntryUnique.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::gc_MemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::gc_MemoryOrder_Release, NAtomic::gc_MemoryOrder_Relaxed))
				break;
		}
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocal(FQueueDispatch &&_fFunctor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<t_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_fFunctor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pNewEntryUnique.f_Detach());
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_AddToQueueLocalFirst(FQueueDispatch &&_fFunctor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<t_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_fFunctor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_InsertFirst(pNewEntryUnique.f_Detach());
		++_LocalQueue.m_nEntries;
		return bWasEmpty;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_RemoveAllExceptFirst(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (auto *pEntry = _LocalQueue.m_LocalQueue.f_GetLast())
		{
			if (pEntry->m_Link.f_IsAloneInList())
				break;
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<t_CEntry *>(pEntry));
		}

		_LocalQueue.m_nEntries = _LocalQueue.m_LocalQueue.f_IsEmpty() ? 0 : 1;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	void TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_RemoveAll(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (auto *pEntry = _LocalQueue.m_LocalQueue.f_Pop())
		{
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<t_CEntry *>(pEntry));
		}

		_LocalQueue.m_nEntries = 0;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_OneOrLessInQueue(CLocalQueueData &_LocalQueue)
	{
		return (_LocalQueue.m_LocalQueue.f_IsEmpty() || _LocalQueue.m_LocalQueue.f_GetFirst()->m_Link.f_IsAloneInList())
			&& mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed) == nullptr
		;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	bool TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_IsEmpty(CLocalQueueData &_LocalQueue)
	{
		return _LocalQueue.m_LocalQueue.f_IsEmpty() && mp_pFirstQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed) == nullptr;
	}

	template <typename t_CBaseEntry, typename t_CEntry>
	umint TCConcurrentRunQueue<t_CBaseEntry, t_CEntry>::f_TransferThreadSafeQueue(CLocalQueueData &_LocalQueue)
	{
		auto *pEntry = mp_pFirstQueued.f_Exchange(nullptr, NAtomic::gc_MemoryOrder_Acquire);
		if (!pEntry)
			return 0;

		umint nTransferred = 0;
		decltype(_LocalQueue.m_LocalQueue) NewEntries;
		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::gc_MemoryOrder_Relaxed);

			NewEntries.f_UnsafeInsertFirst(pEntry);
			++nTransferred;

			pEntry = pNextEntry;
		}

		_LocalQueue.m_LocalQueue.f_InsertLast(NewEntries);
		_LocalQueue.m_nEntries += nTransferred;

		return nTransferred;
	}
}
