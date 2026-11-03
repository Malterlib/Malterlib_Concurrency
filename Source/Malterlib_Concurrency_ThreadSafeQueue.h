// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Function/Function>

//#define DMibContainer_UseBoostLockFreeQueue

#ifdef DMibContainer_UseBoostLockFreeQueue
#	include <boost/lockfree/queue.hpp>
#endif

namespace NMib::NContainer
{
#ifdef DMibContainer_UseBoostLockFreeQueue
	template <typename t_CType, typename t_CAllocator>
	struct TCThreadSafeQueue;
	template <typename t_CType, typename t_CAllocator>
	struct TCThreadSafeQueueEntry;

	template <typename t_CType>
	struct TCThreadSafeQueueListEntry
	{
	private:
		template <typename tf_CObjectType, typename tf_CAllocator, typename ...tfp_CParams>
		friend tf_CObjectType *NMib::fg_ConstructObject(tf_CAllocator &&_Allocator, tfp_CParams && ...p_Params);

		template <typename t_CType0, typename t_CAllocator0>
		friend struct TCThreadSafeQueue;

		template <typename t_CType0, typename t_CAllocator0>
		friend struct TCThreadSafeQueueEntry;

		t_CType m_Data;

		template <typename tf_Param0>
		TCThreadSafeQueueListEntry(tf_Param0 &&_Param0)
			: m_Data(fg_Forward<tf_Param0>(_Param0))
		{
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
			: m_pEntry(_Other.m_pEntry)
		{
			_Other.m_pEntry = nullptr;
		}

		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry &&_Other)
		{
			if (m_pEntry)
				fg_DeleteObjectDefiniteType(t_CAllocator(), m_pEntry);

			m_pEntry = _Other.m_pEntry;
			_Other.m_pEntry = nullptr;

			return *this;
		}

		~TCThreadSafeQueueEntry()
		{
			if (m_pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), m_pEntry);
				m_pEntry = nullptr;
			}
		}

		bool f_IsValid() const
		{
			return m_pEntry != nullptr;
		}

		t_CType &f_Get()
		{
			DMibRequire(m_pEntry);
			return m_pEntry->m_Data;
		}

		mark_nodebug t_CType &operator * ()
		{
			return f_Get();
		}

		mark_nodebug t_CType *operator -> ()
		{
			return &f_Get();
		}

		t_CType const &f_Get() const
		{
			DMibRequire(m_pEntry);
			return m_pEntry->m_Data;
		}

		mark_nodebug t_CType const &operator * () const
		{
			return f_Get();
		}

		mark_nodebug t_CType const *operator -> () const
		{
			return &f_Get();
		}

		inline_small explicit operator bool () const
		{
			return f_IsValid();
		}

	private:
		TCThreadSafeQueueListEntry<t_CType> *m_pEntry;
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueue<t_CType, t_CAllocator>;
		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
			: m_pEntry(_pEntry)
		{
		}

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry const &_Other);
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueue
	{
		TCThreadSafeQueue()
		{
		}

		~TCThreadSafeQueue()
		{
			fp_Clear();
		}

		void f_Clear()
		{
			fp_Clear();
		}

		CReturnEntry f_Pop()
		{
			CListEntry *pEntry;
			if (m_Queue.pop(pEntry))
				return CReturnEntry(pEntry);
			return CReturnEntry(nullptr);
		}

		CReturnEntry f_PopLIFO()
		{
			DMibPDebugBreak; // Not yet supported
			return CReturnEntry();
		}

		template <typename t_CParam0>
		void f_Push(t_CParam0 &&_Param0)
		{
			CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<t_CParam0>(_Param0));
			m_Queue.push(pEntry);
		}

		void f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items)
		{
			CListEntry *pEntry = nullptr;
			while (m_Queue.pop(pEntry))
			{
				o_Items.f_Insert(fg_Move(pEntry->m_Data));
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
			}
		}

		bool f_IsEmpty() const
		{
			return fg_RemoveQualifiers(m_Queue).empty();
		}

	private:
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

		boost::lockfree::queue<CListEntry *> m_Queue;

		using CAllocator = t_CAllocator;

		void fp_Clear()
		{
			CListEntry *pEntry;
			while (m_Queue.pop(pEntry))
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
		}

		TCThreadSafeQueue(TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue(TCThreadSafeQueue &&_Other);
		TCThreadSafeQueue &operator = (TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue &operator = (TCThreadSafeQueue &&_Other);
	};
#else
	template <typename t_CType, typename t_CAllocator>
	struct TCThreadSafeQueue;
	template <typename t_CType, typename t_CAllocator>
	struct TCThreadSafeQueueEntry;

	template <typename t_CType>
	struct TCThreadSafeQueueListEntry
	{
	private:
		template <typename tf_CObjectType, typename tf_CAllocator, typename ...tfp_CParams>
		friend tf_CObjectType *NMib::fg_ConstructObject(tf_CAllocator &&_Allocator, tfp_CParams && ...p_Params);

		template <typename t_CType0, typename t_CAllocator0>
		friend struct TCThreadSafeQueue;

		template <typename t_CType0, typename t_CAllocator0>
		friend struct TCThreadSafeQueueEntry;

		DMibListLinkDS_Link(TCThreadSafeQueueListEntry, mp_Link);
		t_CType mp_Data;

		template <typename ...tfp_CParams>
		TCThreadSafeQueueListEntry(tfp_CParams && ...p_Params)
			: mp_Data(fg_Forward<tfp_CParams>(p_Params)...)
		{
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
			: mp_pEntry(_Other.mp_pEntry)
		{
			_Other.mp_pEntry = nullptr;
		}

		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry &&_Other)
		{
			if (mp_pEntry)
				fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);

			mp_pEntry = _Other.mp_pEntry;
			_Other.mp_pEntry = nullptr;

			return *this;
		}

		~TCThreadSafeQueueEntry()
		{
			if (mp_pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), mp_pEntry);
				mp_pEntry = nullptr;
			}
		}

		bool f_IsValid() const
		{
			return mp_pEntry != nullptr;
		}

		t_CType &f_Get()
		{
			DMibRequire(mp_pEntry);
			return mp_pEntry->mp_Data;
		}

		mark_nodebug t_CType &operator * ()
		{
			return f_Get();
		}

		mark_nodebug t_CType *operator -> ()
		{
			return &f_Get();
		}

		t_CType const &f_Get() const
		{
			DMibRequire(mp_pEntry);
			return mp_pEntry->mp_Data;
		}

		mark_nodebug t_CType const &operator * () const
		{
			return f_Get();
		}

		mark_nodebug t_CType const *operator -> () const
		{
			return &f_Get();
		}

		inline_small explicit operator bool () const
		{
			return f_IsValid();
		}

	private:
		TCThreadSafeQueueListEntry<t_CType> *mp_pEntry;
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueue<t_CType, t_CAllocator>;
		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
			: mp_pEntry(_pEntry)
		{
		}

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry const &_Other);
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueue
	{
	private:
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

		mutable NThread::CLowLevelLock mp_Lock;
		DMibListLinkDS_List_FromTemplate(CListEntry, mp_Link) mp_Queue;

		using CAllocator = t_CAllocator;

		void fp_Clear()
		{
			CListEntry *pEntry = mp_Queue.f_Pop();
			while (pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
				pEntry = mp_Queue.f_Pop();
			}
		}

	public:
		TCThreadSafeQueue()
		{
		}

		TCThreadSafeQueue(TCThreadSafeQueue const &_Other)
		{
			DMibLock(_Other.mp_Lock);
			auto Iter = _Other.mp_Queue.f_GetIterator();
			while (Iter)
			{
				mp_Queue.f_InsertLast(fg_ConstructObject<CListEntry>(t_CAllocator(), Iter->mp_Data));
				++Iter;
			}
		}

		TCThreadSafeQueue(TCThreadSafeQueue &&_Other)
		{
			DMibLock(_Other.mp_Lock);
			mp_Queue = fg_Move(_Other.mp_Queue);
		}

		~TCThreadSafeQueue()
		{
			fp_Clear();
		}

		void f_Clear()
		{
			DMibLock(mp_Lock);
			fp_Clear();
		}

		CReturnEntry f_Pop()
		{
			CListEntry *pEntry;
			{
				DMibLock(mp_Lock);
				pEntry = mp_Queue.f_Pop();
			}

			return CReturnEntry(pEntry);
		}

		CReturnEntry f_PopLIFO()
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

		template <typename ...tfp_CParams>
		void f_Push(tfp_CParams && ...p_Params)
		{
			CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<tfp_CParams>(p_Params)...);
			{
				DMibLock(mp_Lock);
				mp_Queue.f_InsertLast(pEntry);
			}
		}

		void f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items)
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

		bool f_IsEmpty() const
		{
			DMibLock(mp_Lock);
			return mp_Queue.f_IsEmpty();
		}
	};
#endif
}
