// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	constexpr static const mint gc_ActorQueueDispatchFunctionMemory = fg_AlignUpConstExpr(sizeof(void*)*4*2, mint(DMibPMemoryCacheLineSize));
	constexpr static const uint32 gc_InvalidQueue = TCLimitsInt<uint16>::mc_Max;

	/*
	void * // VTable
	void * // PrevPtr
	void * // NextPtr
	void * // TCFunction VTable
	*/
	constexpr static const mint gc_ActorQueueDispatchQueueEntryOverhead = sizeof(void *) * 4;

	using FActorQueueDispatch = NFunction::TCFunction
		<
			void (NFunction::CThisTag &, CConcurrencyThreadLocal &_ThreadLocal)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, gc_ActorQueueDispatchFunctionMemory - gc_ActorQueueDispatchQueueEntryOverhead
				, sizeof(void *)
				, false
			>
		>
	;

	using FActorQueueDispatchEmpty = NFunction::TCFunction
		<
			void (NFunction::CThisTag &, CConcurrencyThreadLocal &_ThreadLocal)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, 0
				, sizeof(void *)
				, false
			>
		>
	;

	struct CConcurrentRunQueueEntry
	{
		CConcurrentRunQueueEntry();
		~CConcurrentRunQueueEntry();
		
		virtual void f_Call(CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void f_Cleanup() = 0;

		union
		{
			NAtomic::TCAtomic<CConcurrentRunQueueEntry *> m_pNextQueued;
			DMibListLinkDSA_LinkType m_Link;
		};
		DMibListLinkD_Trans(CConcurrentRunQueueEntry, m_Link);
	};

	struct align_cacheline CConcurrentRunQueueEntry_Functor final : public CConcurrentRunQueueEntry
	{
		using FQueueDispatch = FActorQueueDispatch;

		CConcurrentRunQueueEntry_Functor(FActorQueueDispatch &&_fToCall);
		void f_Call(CConcurrencyThreadLocal &_ThreadLocal) override;
		void f_Cleanup() override;

		FActorQueueDispatch m_fToCall;
	};

	using FActorQueueDispatchNoAlloc = NFunction::TCFunction
		<
			void (NFunction::CThisTag &, CConcurrencyThreadLocal &_ThreadLocal)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
#if DMibConfig_RefCountDebugging
				true
#else
				false
#endif
#ifdef DPlatformFamily_Windows
				, sizeof(void *) * 6
#else
				, sizeof(void *) * 5
#endif
				, sizeof(void *)
				, false
			>
		>
	;
	
	struct CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc
	{
		using FQueueDispatch = FActorQueueDispatchNoAlloc;

		CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc();
		~CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc();

		CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc(FActorQueueDispatchNoAlloc &&_fToCall);

		void f_Call(CConcurrencyThreadLocal &_ThreadLocal);
		void f_Cleanup();

		union
		{
			NAtomic::TCAtomic<CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc *> m_pNextQueued;
			DMibListLinkDSA_LinkType m_Link;
		};
		DMibListLinkD_Trans(CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc, m_Link);

		FActorQueueDispatchNoAlloc m_fToCall;
	};
	
	template <typename tf_CBaseEntry>
	struct TCConcurrentRunQueueEntryHolder
	{
		TCConcurrentRunQueueEntryHolder(tf_CBaseEntry *_pEntry);
		TCConcurrentRunQueueEntryHolder(TCConcurrentRunQueueEntryHolder &&_Other);
		TCConcurrentRunQueueEntryHolder &operator = (TCConcurrentRunQueueEntryHolder &&_Other);
		~TCConcurrentRunQueueEntryHolder();

		bool f_IsEmpty() const;
		tf_CBaseEntry *operator -> ();
		tf_CBaseEntry *f_Detach();

	private:
		tf_CBaseEntry *mp_pEntry;
	};

	using CConcurrentRunQueueEntryHolder = TCConcurrentRunQueueEntryHolder<CConcurrentRunQueueEntry>;

	template <typename tf_CBaseEntry, typename tf_CEntry>
	struct TCConcurrentRunQueue
	{
		using FQueueDispatch = typename tf_CEntry::FQueueDispatch;

		struct CLocalQueueData
		{
			~CLocalQueueData();

			DMibListLinkDS_List_FromTemplate(tf_CBaseEntry, m_Link) m_LocalQueue;
		};

		TCConcurrentRunQueue();
		~TCConcurrentRunQueue();
		
		static NStorage::TCUniquePointer<tf_CEntry> fs_QueueEntry(FQueueDispatch &&_Functor);
		bool f_AddToQueueIfEmpty(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry);
		void f_AddToQueue(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry);
		bool f_AddToQueueLocal(NStorage::TCUniquePointer<tf_CEntry> &&_pEntry, CLocalQueueData &_LocalQueue);

		void f_AddToQueue(FQueueDispatch &&_Functor);
		bool f_AddToQueueLocal(FQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue);
		bool f_AddToQueueLocalFirst(FQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue);

		void f_AddToQueue(TCConcurrentRunQueueEntryHolder<tf_CBaseEntry> &&_Entry);
		bool f_AddToQueueLocal(TCConcurrentRunQueueEntryHolder<tf_CBaseEntry> &&_Entry, CLocalQueueData &_LocalQueue);

		bool f_TransferThreadSafeQueue(CLocalQueueData &_LocalQueue);
		bool f_IsEmpty(CLocalQueueData &_LocalQueue);
		bool f_OneOrLessInQueue(CLocalQueueData &_LocalQueue);
		void f_RemoveAll(CLocalQueueData &_LocalQueue);
		void f_RemoveAllExceptFirst(CLocalQueueData &_LocalQueue);

	private:
		NAtomic::TCAtomic<tf_CBaseEntry *> mp_pFirstQueued;
	};

	using CConcurrentRunQueue = TCConcurrentRunQueue<CConcurrentRunQueueEntry, CConcurrentRunQueueEntry_Functor>;
	using CConcurrentRunQueueNonVirtualNoAlloc = TCConcurrentRunQueue<CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc, CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc>;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_RunQueue.hpp"
