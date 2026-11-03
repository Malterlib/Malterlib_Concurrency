// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Function/Function>

#include <boost/lockfree/queue.hpp>

#define DDefaultThreadSafeQueueSize 32

namespace NMib::NContainer
{
	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	struct TCThreadSafeQueueAtomic;
	template <typename t_CType, typename t_CAllocator, int t_DefaultSize>
	struct TCThreadSafeQueueAtomicEntry;

	template <typename t_CType>
	struct TCThreadSafeQueueAtomicListEntry
	{
	private:
		template <typename tf_CObjectType, typename tf_CAllocator, typename ...tfp_CParams>
		friend tf_CObjectType *NMib::fg_ConstructObject(tf_CAllocator &&_Allocator, tfp_CParams && ...p_Params);

		template <typename t_CType0, typename t_CAllocator0, int t_DefaultSize0>
		friend struct TCThreadSafeQueueAtomic;

		template <typename t_CType0, typename t_CAllocator0, int t_DefaultSize0>
		friend struct TCThreadSafeQueueAtomicEntry;

		t_CType m_Data;

		template <typename tf_Param0>
		TCThreadSafeQueueAtomicListEntry(tf_Param0 &&_Param0)
			: m_Data(fg_Forward<tf_Param0>(_Param0))
		{
		}
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap, int t_DefaultSize = DDefaultThreadSafeQueueSize>
	struct TCThreadSafeQueueAtomicEntry
	{
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicEntry &&_Other)
			: m_pEntry(_Other.m_pEntry)
		{
			_Other.m_pEntry = nullptr;
		}

		TCThreadSafeQueueAtomicEntry &operator = (TCThreadSafeQueueAtomicEntry &&_Other)
		{
			if (m_pEntry)
				fg_DeleteObjectDefiniteType(t_CAllocator(), m_pEntry);

			m_pEntry = _Other.m_pEntry;
			_Other.m_pEntry = nullptr;

			return *this;
		}

		~TCThreadSafeQueueAtomicEntry()
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
		TCThreadSafeQueueAtomicListEntry<t_CType> *m_pEntry;
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>;
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicListEntry<t_CType> *_pEntry)
			: m_pEntry(_pEntry)
		{
		}

		// Disable copy
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicEntry const &_Other);
		TCThreadSafeQueueAtomicEntry &operator = (TCThreadSafeQueueAtomicEntry const &_Other);
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap, int t_DefaultSize = DDefaultThreadSafeQueueSize>
	struct TCThreadSafeQueueAtomic
	{
		TCThreadSafeQueueAtomic()
		{
		}

		~TCThreadSafeQueueAtomic()
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
		using CListEntry = TCThreadSafeQueueAtomicListEntry<t_CType>;
		using CReturnEntry = TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator>;

		template <typename T>
		struct CStdAllocator : public std::allocator<T>
		{
			T *allocate(size_t n)
			{
				return (T *)t_CAllocator::f_Alloc(n * sizeof(T));
			}

			void deallocate(T *p, size_t n)
			{
				t_CAllocator::f_Free(p, sizeof(T) * n);
			}

			template <typename t_CType2>
			struct rebind
			{
				using other = CStdAllocator<t_CType2>;
			};
		};

		using allocator_type = typename boost::lockfree::allocator<CStdAllocator<void>>;

		boost::lockfree::queue<CListEntry *, allocator_type> m_Queue {t_DefaultSize};

		using CAllocator = t_CAllocator;

		void fp_Clear()
		{
			CListEntry *pEntry;
			while (m_Queue.pop(pEntry))
				fg_DeleteObjectDefiniteType(t_CAllocator(), pEntry);
		}

		TCThreadSafeQueueAtomic(TCThreadSafeQueueAtomic const &_Other);
		TCThreadSafeQueueAtomic(TCThreadSafeQueueAtomic &&_Other);
		TCThreadSafeQueueAtomic &operator = (TCThreadSafeQueueAtomic const &_Other);
		TCThreadSafeQueueAtomic &operator = (TCThreadSafeQueueAtomic &&_Other);
	};
}
