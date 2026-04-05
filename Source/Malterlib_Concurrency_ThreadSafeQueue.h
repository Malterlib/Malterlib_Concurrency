// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Function/Function>

//#define DMibContainer_UseBoostLockFreeQueue

#ifdef DMibContainer_UseBoostLockFreeQueue
#include <boost/lockfree/queue.hpp>
#endif

namespace NMib::NContainer
{
#ifdef DMibContainer_UseBoostLockFreeQueue
	template <typename t_CType, typename t_CAllocator>
	class TCThreadSafeQueue;
	template <typename t_CType, typename t_CAllocator>
	class TCThreadSafeQueueEntry;

	template <typename t_CType>
	class TCThreadSafeQueueListEntry
	{
		template <typename tf_ObjectType, typename tf_CAllocator, typename... tfp_CParams>
		friend tf_ObjectType *NMib::fg_ConstructObject(tf_CAllocator &&_Allocator, tfp_CParams&&... p_Params);

		template <typename t_CType0, typename t_CAllocator0>
		friend class TCThreadSafeQueue;

		template <typename t_CType0, typename t_CAllocator0>
		friend class TCThreadSafeQueueEntry;

		t_CType m_Data;

		template <typename tf_Param0>
		TCThreadSafeQueueListEntry(tf_Param0 &&_Param0)
			: m_Data(fg_Forward<tf_Param0>(_Param0))
		{
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	class TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueListEntry<t_CType> *m_pEntry;
//			template <typename t_CType, typename t_CAllocator>
		friend class TCThreadSafeQueue<t_CType, t_CAllocator>;
		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
			: m_pEntry(_pEntry)
		{

		}

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator =(TCThreadSafeQueueEntry const &_Other);
	public:

		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
			: m_pEntry(_Other.m_pEntry)
		{
			_Other.m_pEntry = nullptr;
		}

		TCThreadSafeQueueEntry &operator =(TCThreadSafeQueueEntry &&_Other)
		{
			if (m_pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), m_pEntry);
			}

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

		mark_nodebug t_CType &operator *()
		{
			return f_Get();
		}

		mark_nodebug t_CType *operator ->()
		{
			return &f_Get();
		}

		t_CType const &f_Get() const
		{
			DMibRequire(m_pEntry);
			return m_pEntry->m_Data;
		}

		mark_nodebug t_CType const &operator *() const
		{
			return f_Get();
		}

		mark_nodebug t_CType const *operator ->() const
		{
			return &f_Get();
		}

		inline_small explicit operator bool() const
		{
			return f_IsValid();
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	class TCThreadSafeQueue
	{
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

		boost::lockfree::queue<CListEntry *> m_Queue;

		using CAllocator = t_CAllocator;

		void fp_Clear()
		{
			CListEntry *pEntry;
			while (m_Queue.pop(pEntry))
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
			}
		}
		TCThreadSafeQueue(TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue(TCThreadSafeQueue &&_Other);
		TCThreadSafeQueue& operator =(TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue& operator =(TCThreadSafeQueue &&_Other);
	public:

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

		void f_PopAllToVector(NContainer::TCVector<t_CType>& _olItems)
		{
			CListEntry *pEntry = nullptr;
			while (m_Queue.pop(pEntry))
			{
				_olItems.f_Insert(fg_Move(pEntry->m_Data));
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
			}
		}

		bool f_IsEmpty() const
		{
			return fg_RemoveQualifiers(m_Queue).empty();
		}

	};
#else
	template <typename t_CType, typename t_CAllocator>
	class TCThreadSafeQueue;
	template <typename t_CType, typename t_CAllocator>
	class TCThreadSafeQueueEntry;

	template <typename t_CType>
	class TCThreadSafeQueueListEntry
	{
		template <typename tf_ObjectType, typename tf_CAllocator, typename... tfp_CParams>
		friend tf_ObjectType *NMib::fg_ConstructObject(tf_CAllocator &&_Allocator, tfp_CParams&&... p_Params);

		template <typename t_CType0, typename t_CAllocator0>
		friend class TCThreadSafeQueue;

		template <typename t_CType0, typename t_CAllocator0>
		friend class TCThreadSafeQueueEntry;

		DMibListLinkDS_Link(TCThreadSafeQueueListEntry, m_Link);
		t_CType m_Data;

		template <typename ...tfp_CParams>
		TCThreadSafeQueueListEntry(tfp_CParams && ...p_Params)
			: m_Data(fg_Forward<tfp_CParams>(p_Params)...)
		{
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	class TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueListEntry<t_CType> *m_pEntry;
//			template <typename t_CType, typename t_CAllocator>
		friend class TCThreadSafeQueue<t_CType, t_CAllocator>;
		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry)
			: m_pEntry(_pEntry)
		{

		}

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator =(TCThreadSafeQueueEntry const &_Other);
	public:

		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other)
			: m_pEntry(_Other.m_pEntry)
		{
			_Other.m_pEntry = nullptr;
		}

		TCThreadSafeQueueEntry &operator =(TCThreadSafeQueueEntry &&_Other)
		{
			if (m_pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), m_pEntry);
			}

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

		mark_nodebug t_CType &operator *()
		{
			return f_Get();
		}

		mark_nodebug t_CType *operator ->()
		{
			return &f_Get();
		}

		t_CType const &f_Get() const
		{
			DMibRequire(m_pEntry);
			return m_pEntry->m_Data;
		}

		mark_nodebug t_CType const &operator *() const
		{
			return f_Get();
		}

		mark_nodebug t_CType const *operator ->() const
		{
			return &f_Get();
		}

		inline_small explicit operator bool() const
		{
			return f_IsValid();
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	class TCThreadSafeQueue
	{
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

		mutable NThread::CLowLevelLock m_Lock;
		DMibListLinkDS_List_FromTemplate(CListEntry, m_Link) m_Queue;

		using CAllocator = t_CAllocator;

		void fp_Clear()
		{
			CListEntry *pEntry = m_Queue.f_Pop();
			while (pEntry)
			{
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
				pEntry = m_Queue.f_Pop();
			}
		}
	public:

		TCThreadSafeQueue()
		{
		}

		TCThreadSafeQueue(TCThreadSafeQueue const &_Other)
		{
			DMibLock(_Other.m_Lock);
			auto Iter = _Other.m_Queue.f_GetIterator();
			while (Iter)
			{
				m_Queue.f_InsertLast(fg_ConstructObject<CListEntry>(t_CAllocator(), Iter->m_Data));
				++Iter;
			}
		}
		TCThreadSafeQueue(TCThreadSafeQueue &&_Other)
		{
			DMibLock(_Other.m_Lock);
			m_Queue = fg_Move(_Other.m_Queue);
		}

		~TCThreadSafeQueue()
		{
			fp_Clear();
		}

		void f_Clear()
		{
			DMibLock(m_Lock);
			fp_Clear();
		}

		CReturnEntry f_Pop()
		{
			CListEntry *pEntry;
			{
				DMibLock(m_Lock);
				pEntry = m_Queue.f_Pop();
			}

			return CReturnEntry(pEntry);
		}

		CReturnEntry f_PopLIFO()
		{
			CListEntry *pEntry;
			{
				DMibLock(m_Lock);
				pEntry = m_Queue.f_GetLast();
				if (pEntry)
					pEntry->m_Link.f_Unlink();
			}

			return CReturnEntry(pEntry);
		}

		template <typename ...tfp_CParams>
		void f_Push(tfp_CParams && ...p_Params)
		{
			CListEntry *pEntry = fg_ConstructObject<CListEntry>(t_CAllocator(), fg_Forward<tfp_CParams>(p_Params)...);
			{
				DMibLock(m_Lock);
				m_Queue.f_InsertLast(pEntry);
			}
		}

		void f_PopAllToVector(NContainer::TCVector<t_CType>& _olItems)
		{
			DMibLock(m_Lock);

			CListEntry *pEntry = m_Queue.f_Pop();
			while (pEntry)
			{
				_olItems.f_Insert(fg_Move(pEntry->m_Data));
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
				pEntry = m_Queue.f_Pop();
			}
		}

		bool f_IsEmpty() const
		{
			DMibLock(m_Lock);
			return m_Queue.f_IsEmpty();
		}

	};
#endif
}
