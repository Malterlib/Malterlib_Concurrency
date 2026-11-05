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

		template <typename tf_CParam0>
		TCThreadSafeQueueListEntry(tf_CParam0 &&_Param0);

		t_CType mp_Data;
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other);
		~TCThreadSafeQueueEntry();

		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry &&_Other);

		bool f_IsValid() const;

		t_CType &f_Get();
		mark_nodebug t_CType &operator * ();
		mark_nodebug t_CType *operator -> ();

		t_CType const &f_Get() const;
		mark_nodebug t_CType const &operator * () const;
		mark_nodebug t_CType const *operator -> () const;

		inline_small explicit operator bool () const;

	private:
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueue<t_CType, t_CAllocator>;

		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry);

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry const &_Other);

		TCThreadSafeQueueListEntry<t_CType> *mp_pEntry;
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueue
	{
	private:
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

	public: // NOLINT(malterlib-member-order): the public section uses CReturnEntry
		TCThreadSafeQueue();
		~TCThreadSafeQueue();

		void f_Clear();

		CReturnEntry f_Pop();
		CReturnEntry f_PopLIFO();

		template <typename tf_CParam0>
		void f_Push(tf_CParam0 &&_Param0);

		void f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items);

		bool f_IsEmpty() const;

	private:
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CAllocator = t_CAllocator;

		TCThreadSafeQueue(TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue(TCThreadSafeQueue &&_Other);

		void fp_Clear();

		TCThreadSafeQueue &operator = (TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue &operator = (TCThreadSafeQueue &&_Other);

		boost::lockfree::queue<CListEntry *> mp_Queue;
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

		template <typename ...tfp_CParams>
		TCThreadSafeQueueListEntry(tfp_CParams && ...p_Params);

		DMibListLinkDS_Link(TCThreadSafeQueueListEntry, mp_Link);
		t_CType mp_Data;
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueueEntry
	{
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry &&_Other);
		~TCThreadSafeQueueEntry();

		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry &&_Other);

		bool f_IsValid() const;

		t_CType &f_Get();
		mark_nodebug t_CType &operator * ();
		mark_nodebug t_CType *operator -> ();

		t_CType const &f_Get() const;
		mark_nodebug t_CType const &operator * () const;
		mark_nodebug t_CType const *operator -> () const;

		inline_small explicit operator bool () const;

	private:
		// template <typename t_CType, typename t_CAllocator>
		friend struct TCThreadSafeQueue<t_CType, t_CAllocator>;

		TCThreadSafeQueueEntry(TCThreadSafeQueueListEntry<t_CType> *_pEntry);

		// Disable copy
		TCThreadSafeQueueEntry(TCThreadSafeQueueEntry const &_Other);
		TCThreadSafeQueueEntry &operator = (TCThreadSafeQueueEntry const &_Other);

		TCThreadSafeQueueListEntry<t_CType> *mp_pEntry;
	};

	template <typename t_CType, typename t_CAllocator = NMemory::CAllocator_Heap>
	struct TCThreadSafeQueue
	{
	private:
		using CReturnEntry = TCThreadSafeQueueEntry<t_CType, t_CAllocator>;

	public: // NOLINT(malterlib-member-order): the public section uses CReturnEntry
		TCThreadSafeQueue();
		TCThreadSafeQueue(TCThreadSafeQueue const &_Other);
		TCThreadSafeQueue(TCThreadSafeQueue &&_Other);
		~TCThreadSafeQueue();

		void f_Clear();

		CReturnEntry f_Pop();
		CReturnEntry f_PopLIFO();

		template <typename ...tfp_CParams>
		void f_Push(tfp_CParams && ...p_Params);

		void f_PopAllToVector(NContainer::TCVector<t_CType> &o_Items);

		bool f_IsEmpty() const;

	private:
		using CListEntry = TCThreadSafeQueueListEntry<t_CType>;
		using CAllocator = t_CAllocator;

		void fp_Clear();

		mutable NThread::CLowLevelLock mp_Lock;
		DMibListLinkDS_List_FromTemplate(CListEntry, mp_Link) mp_Queue;
	};
#endif
}

#include "Malterlib_Concurrency_ThreadSafeQueue.hpp"
