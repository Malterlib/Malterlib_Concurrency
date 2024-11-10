// Copyright © 2024 Favro Holding AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	template <typename tf_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::TCConcurrentRunQueueEntryHolder(tf_CBaseEntry *_pEntry)
		: mp_pEntry(_pEntry)
	{
	}

	template <typename tf_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::TCConcurrentRunQueueEntryHolder(TCConcurrentRunQueueEntryHolder &&_Other)
		: mp_pEntry(fg_Exchange(_Other.mp_pEntry, nullptr))
	{
	}

	template <typename tf_CBaseEntry>
	auto TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::operator = (TCConcurrentRunQueueEntryHolder &&_Other) -> TCConcurrentRunQueueEntryHolder &
	{
		mp_pEntry = fg_Exchange(_Other.mp_pEntry, nullptr);

		return *this;
	}

	template <typename tf_CBaseEntry>
	TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::~TCConcurrentRunQueueEntryHolder()
	{
		if (mp_pEntry)
			mp_pEntry->f_Cleanup();
	}

	template <typename tf_CBaseEntry>
	bool TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::f_IsEmpty() const
	{
		return !mp_pEntry;
	}

	template <typename tf_CBaseEntry>
	tf_CBaseEntry *TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::operator -> ()
	{
		return mp_pEntry;
	}

	template <typename tf_CBaseEntry>
	tf_CBaseEntry *TCConcurrentRunQueueEntryHolder<tf_CBaseEntry>::f_Detach()
	{
		return fg_Exchange(mp_pEntry, nullptr);
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::~TCConcurrentRunQueue()
	{
		auto *pEntry = mp_pFirstQueued.f_Exchange(nullptr, NAtomic::EMemoryOrder_Acquire);
		if (!pEntry)
			return;

		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			pNextEntry->f_Cleanup();
			pEntry = pNextEntry;
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::TCConcurrentRunQueue()
	{
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::CLocalQueueData::~CLocalQueueData()
	{
		while (auto *pEntry = m_LocalQueue.f_Pop())
		{
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<tf_CEntry *>(pEntry));
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	auto TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::fs_QueueEntry(FQueueDispatch &&_Functor) -> NStorage::TCUniquePointer<tf_CEntry>
	{
		return fg_Construct(fg_Move(_Functor));
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueueIfEmpty(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry)
	{
		tf_CEntry *pNewEntry = _pEntry.f_Get();
		pNewEntry->m_pNextQueued.f_Store(nullptr, NAtomic::EMemoryOrder_Relaxed);

		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (pFirstEntry)
				return false;

			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
			{
				_pEntry.f_Detach();
				return true;
			}
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueue(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry)
	{
		tf_CEntry *pNewEntry = _pEntry.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::EMemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
				break;
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueueLocal(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _pEntry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pEntry);
		return bWasEmpty;
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueue(TCConcurrentRunQueueEntryHolder<tf_CBaseEntry> &&_Entry)
	{
		auto *pNewEntry = _Entry.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::EMemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
				break;
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueueLocal(TCConcurrentRunQueueEntryHolder<tf_CBaseEntry> &&_Entry, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		auto pEntry = _Entry.f_Detach();
		pEntry->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pEntry);
		return bWasEmpty;
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueue(FQueueDispatch &&_Functor)
	{
		NStorage::TCUniquePointer<tf_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		tf_CEntry *pNewEntry = pNewEntryUnique.f_Detach();
		while (true)
		{
			auto *pFirstEntry = mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			pNewEntry->m_pNextQueued.f_Store(pFirstEntry, NAtomic::EMemoryOrder_Relaxed);
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
				break;
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueueLocal(FQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<tf_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pNewEntryUnique.f_Detach());
		return bWasEmpty;
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_AddToQueueLocalFirst(FQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<tf_CEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_InsertFirst(pNewEntryUnique.f_Detach());
		return bWasEmpty;
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_RemoveAllExceptFirst(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (auto *pEntry = _LocalQueue.m_LocalQueue.f_GetLast())
		{
			if (pEntry->m_Link.f_IsAloneInList())
				break;
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<tf_CEntry *>(pEntry));
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_RemoveAll(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (auto *pEntry = _LocalQueue.m_LocalQueue.f_Pop())
		{
			pEntry->m_Link.f_UnsafeUnlink();
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), static_cast<tf_CEntry *>(pEntry));
		}
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	void TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_DeleteQueueEntry(CConcurrentRunQueueEntry *_pEntry)
	{
		_pEntry->f_Cleanup();
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_OneOrLessInQueue(CLocalQueueData &_LocalQueue)
	{
		return mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed) == nullptr && (_LocalQueue.m_LocalQueue.f_IsEmpty() || _LocalQueue.m_LocalQueue.f_GetFirst()->m_Link.f_IsAloneInList());
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_IsEmpty(CLocalQueueData &_LocalQueue)
	{
		return mp_pFirstQueued.f_Load(NAtomic::EMemoryOrder_Relaxed) == nullptr && _LocalQueue.m_LocalQueue.f_IsEmpty();
	}

	template <typename tf_CBaseEntry, typename tf_CEntry>
	bool TCConcurrentRunQueue<tf_CBaseEntry, tf_CEntry>::f_TransferThreadSafeQueue(CLocalQueueData &_LocalQueue)
	{
		auto *pEntry = mp_pFirstQueued.f_Exchange(nullptr, NAtomic::EMemoryOrder_Acquire);
		if (!pEntry)
			return false;

		decltype(_LocalQueue.m_LocalQueue) NewEntries;
		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);

			NewEntries.f_UnsafeInsertFirst(pEntry);

			pEntry = pNextEntry;
		}

		_LocalQueue.m_LocalQueue.f_InsertLast(NewEntries);
		
		return true;
	}
}
