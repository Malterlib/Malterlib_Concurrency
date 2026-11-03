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

		t_CType mp_Data;

		template <typename tf_CParam0>
		TCThreadSafeQueueAtomicListEntry(tf_CParam0 &&_Param0);
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap, int t_DefaultSize = DDefaultThreadSafeQueueSize>
	struct TCThreadSafeQueueAtomicEntry
	{
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicEntry &&_Other);
		TCThreadSafeQueueAtomicEntry &operator = (TCThreadSafeQueueAtomicEntry &&_Other);
		~TCThreadSafeQueueAtomicEntry();

		bool f_IsValid() const;

		t_CType &f_Get();
		mark_nodebug t_CType &operator * ();
		mark_nodebug t_CType *operator -> ();

		t_CType const &f_Get() const;
		mark_nodebug t_CType const &operator * () const;
		mark_nodebug t_CType const *operator -> () const;

		inline_small explicit operator bool () const;

	private:
		TCThreadSafeQueueAtomicListEntry<t_CType> *mp_pEntry;
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueueAtomic<t_CType, t_CAllocator, t_DefaultSize>;
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicListEntry<t_CType> *_pEntry);

		// Disable copy
		TCThreadSafeQueueAtomicEntry(TCThreadSafeQueueAtomicEntry const &_Other);
		TCThreadSafeQueueAtomicEntry &operator = (TCThreadSafeQueueAtomicEntry const &_Other);
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap, int t_DefaultSize = DDefaultThreadSafeQueueSize>
	struct TCThreadSafeQueueAtomic
	{
	private:
		// Declared before the public section, which uses it.
		using CReturnEntry = TCThreadSafeQueueAtomicEntry<t_CType, t_CAllocator>;

	public:
		TCThreadSafeQueueAtomic();
		~TCThreadSafeQueueAtomic();

		void f_Clear();

		CReturnEntry f_Pop();
		CReturnEntry f_PopLIFO();

		template <typename tf_CParam0>
		void f_Push(tf_CParam0 &&_Param0);

		void f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items);

		bool f_IsEmpty() const;

	private:
		using CListEntry = TCThreadSafeQueueAtomicListEntry<t_CType>;

		template <typename t_CValue>
		struct TCStdAllocator : public std::allocator<t_CValue>
		{
			t_CValue *allocate(size_t _nItems);
			void deallocate(t_CValue *_pMemory, size_t _nItems);

			template <typename t_CType2>
			struct rebind
			{
				using other = TCStdAllocator<t_CType2>;
			};
		};

		using allocator_type = typename boost::lockfree::allocator<TCStdAllocator<void>>;

		boost::lockfree::queue<CListEntry *, allocator_type> mp_Queue {t_DefaultSize};

		using CAllocator = t_CAllocator;

		void fp_Clear();

		TCThreadSafeQueueAtomic(TCThreadSafeQueueAtomic const &_Other);
		TCThreadSafeQueueAtomic(TCThreadSafeQueueAtomic &&_Other);
		TCThreadSafeQueueAtomic &operator = (TCThreadSafeQueueAtomic const &_Other);
		TCThreadSafeQueueAtomic &operator = (TCThreadSafeQueueAtomic &&_Other);
	};
}

#include "Malterlib_Concurrency_ThreadSafeQueueAtomic.hpp"
