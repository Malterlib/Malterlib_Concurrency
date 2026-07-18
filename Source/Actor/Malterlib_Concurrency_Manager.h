// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Function/Function>
#include <Mib/Meta/Meta>
#include <Mib/Storage/Tuple>
#include <Mib/Time/Stopwatch>

#include "Malterlib_Concurrency_Coroutine.h"

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Helpers.h"
#include "Malterlib_Concurrency_Promise.h"
#include "Malterlib_Concurrency_Actor.h"
#include "Malterlib_Concurrency_DefaultActors.h"
#include "Malterlib_Concurrency_ActorHolder.h"
#include "Malterlib_Concurrency_InternalCallWithAsyncResult.h"
#include "Malterlib_Concurrency_ActorInternal.h"

namespace NMib::NConcurrency
{
	struct CConcurrencyThreadLocal;
	CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal();

	struct CFutureCoroutineContext;

	struct CBlockingActorStorage
	{
		TCActor<CBlockingActor> m_Actor;
		DMibListLinkDS_Link(CBlockingActorStorage, m_Link);
#if DMibConfig_Concurrency_DebugBlockDestroy
		NThread::CLowLevelLock m_CheckoutCallstackLock;
		NException::CCallstack m_CheckoutCallstack;
#endif
	};

	/// \brief Manages scheduling of running actors in a thread pool
	class CConcurrencyManager
	{
	public:
		CConcurrencyManager(EExecutionPriority _ExecutionPriority[EPriority_Max]);
		~CConcurrencyManager();
		void f_Init();
		void f_Stop();
		void f_EnableShutdownLogging(bool _bEnabled);

		template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
		TCActor<tf_CType> f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params);

		template <typename tf_CType, typename... tfp_CParams>
		TCActor<tf_CType> f_ConstructFromInternalActor
			(
				TCActorHolderSharedPointer<TCActorInternal<tf_CType>> &&_pInternalActor
				, TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams
			)
		;

		template <typename tf_CActor>
		consteval static bool fs_HasOverridenDestroy()
			requires (!NTraits::cIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>)
		;

		template <typename tf_CActor>
		consteval static bool fs_HasOverridenDestroy()
			requires (NTraits::cIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>)
		;

		template <typename tf_CActor>
		consteval static bool fs_HasOverridenDestroy();

		void f_BlockOnDestroy();
		TCActor<CConcurrentActor> const &f_GetConcurrentActor();
		TCActor<CConcurrentActor> const &f_GetConcurrentActor(TCWeakActor<CActor> const &_Actor);
		TCActor<CConcurrentActor> const &f_GetConcurrentActorLowPrio();
		TCActor<CConcurrentActor> const &f_GetConcurrentActorHighCPU();
		TCActor<CConcurrentActor> const &f_GetConcurrentActorForThisThread(EPriority _Priority);
		TCActor<CConcurrentActor> const &f_GetConcurrentActorForOtherThread(EPriority _Priority);
		CBlockingActorCheckout f_GetBlockingActor();
		TCActor<CTimerActor> const &f_GetTimerActor();
		TCActor<CDirectCallActor> const &f_GetDirectCallActor();
		TCActor<CThisConcurrentActor> const &f_GetThisConcurrentActor();
		TCActor<CThisConcurrentActorLowPrio> const &f_GetThisConcurrentActorLowPrio();
		TCActor<CThisConcurrentActorHighCPU> const &f_GetThisConcurrentActorHighCPU();
		TCActor<COtherConcurrentActor> const &f_GetOtherConcurrentActor();
		TCActor<COtherConcurrentActorLowPrio> const &f_GetOtherConcurrentActorLowPrio();
		TCActor<COtherConcurrentActorHighCPU> const &f_GetOtherConcurrentActorHighCPU();
		TCActor<CDynamicConcurrentActor> const &f_GetDynamicConcurrentActor();
		TCActor<CDynamicConcurrentActorLowPrio> const &f_GetDynamicConcurrentActorLowPrio();
		TCActor<CDynamicConcurrentActorHighCPU> const &f_GetDynamicConcurrentActorHighCPU();

		void f_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue);
		void f_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue);

		void f_SetExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority);
		EExecutionPriority f_GetExecutionPriority(EPriority _Priority);

		bool f_DestroyingAlwaysAliveActors() const;

		umint f_GetConcurrency() const;
		umint f_GetQueue() const;

#if DMibConfig_Concurrency_SchedulerStats
		/// \brief Snapshot of aggregated scheduling statistics
		struct CSchedulerStats
		{
			umint m_nSignals = 0; ///< Semaphore signals posted to pool threads
			umint m_nSleeps = 0; ///< Times pool threads went to sleep
			umint m_nLocalEnqueues = 0; ///< Jobs enqueued to the producing thread's own local queue
			umint m_nAtomicEnqueues = 0; ///< Jobs enqueued through the atomic queue
			umint m_nCurrentThreadSelections = 0; ///< Jobs targeted at the producing pool thread's queue
			umint m_nFixedSelections = 0; ///< Jobs targeted by fixed-queue pinning
			umint m_nForcedNonLocalSelections = 0; ///< Jobs redistributed after a yield
			umint m_nLastQueueSelections = 0; ///< Jobs targeted by last-queue affinity
			umint m_nRoundRobinSelections = 0; ///< Jobs targeted by round-robin selection
			umint m_nHandoffs = 0; ///< Jobs handed off to idle threads
			umint m_nIdleClaims = 0; ///< Successful claims of idle threads
			umint m_nRunProcess = 0; ///< Actor drain runs executed on pool threads
			umint m_nEntriesDrained = 0; ///< Actor messages processed by drain runs on pool threads
			umint m_nLongDrains = 0; ///< Drain runs that processed 16 or more messages
			umint m_nGarbageCollectCalls = 0; ///< Owner-side arena garbage collect attempts from drain runs
			umint m_nGarbageCollects = 0; ///< Owner-side arena garbage collects that found pending messages
		};

		CSchedulerStats f_GetSchedulerStats(EPriority _Priority);
		CSchedulerStats f_GetSchedulerStats();
		void f_ResetSchedulerStats();
#endif

#if DMibConfig_Concurrency_DebugFutures
		NThread::CLowLevelLock m_FutureListLock;
		DMibListLinkDS_List(NPrivate::CPromiseDataBase, m_Link) m_Futures;
		DMibListLinkDS_List(NPrivate::CPromiseDataBase, m_LinkCoro) m_Coroutines;
#endif
	private:
		template <typename t_CActor>
		friend class TCActorInternal;

		friend class CActorHolder;
		friend struct CActorCommon;
		friend struct CActor;
		friend class CDefaultActorHolder;
		friend struct CCurrentActorScope;
		friend struct CCurrentlyProcessingActorScope;
		friend struct CConcurrencyThreadLocal;
#if DMibConfig_Concurrency_DebugFutures
		friend struct NPrivate::CPromiseDataBase;
#endif
		friend struct CBlockingActorCheckout;

#if DMibConfig_Concurrency_SchedulerStats
		/// \brief Scheduling statistics for one pool queue
		struct align_cacheline CSchedulerQueueStats
		{
			NAtomic::TCAtomic<umint> m_nSignals = 0; ///< Semaphore signals posted to this queue
			NAtomic::TCAtomic<umint> m_nSleeps = 0; ///< Times this queue's thread went to sleep
			NAtomic::TCAtomic<umint> m_nLocalEnqueues = 0; ///< Jobs enqueued from this queue's own thread
			NAtomic::TCAtomic<umint> m_nAtomicEnqueues = 0; ///< Jobs enqueued through the atomic queue
			NAtomic::TCAtomic<umint> m_nCurrentThreadSelections = 0; ///< Jobs targeted at this queue because the producer runs on it
			NAtomic::TCAtomic<umint> m_nFixedSelections = 0; ///< Jobs targeted at this queue by fixed-queue pinning
			NAtomic::TCAtomic<umint> m_nForcedNonLocalSelections = 0; ///< Jobs targeted at this queue after a yield
			NAtomic::TCAtomic<umint> m_nLastQueueSelections = 0; ///< Jobs targeted at this queue by last-queue affinity
			NAtomic::TCAtomic<umint> m_nRoundRobinSelections = 0; ///< Jobs targeted at this queue by round-robin selection
			NAtomic::TCAtomic<umint> m_nHandoffs = 0; ///< Jobs handed off from this queue to an idle queue
			NAtomic::TCAtomic<umint> m_nIdleClaims = 0; ///< Successful idle claims of this queue by producers
			NAtomic::TCAtomic<umint> m_nRunProcess = 0; ///< Actor drain runs executed on this queue's thread
			NAtomic::TCAtomic<umint> m_nEntriesDrained = 0; ///< Actor messages processed by drain runs on this queue's thread
			NAtomic::TCAtomic<umint> m_nLongDrains = 0; ///< Drain runs on this queue's thread that processed 16 or more messages
			NAtomic::TCAtomic<umint> m_nGarbageCollectCalls = 0; ///< Owner-side arena garbage collect attempts from drain runs
			NAtomic::TCAtomic<umint> m_nGarbageCollects = 0; ///< Owner-side arena garbage collects that found pending messages
		};
#endif

		struct CQueue
		{
			align_cacheline CConcurrentRunQueueNonVirtualNoAlloc m_JobQueue;
			NAtomic::TCAtomic<umint> m_Working;
			align_cacheline CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData m_JobQueueLocal;
			umint m_iQueue;
			EPriority m_Priority;
			NThread::CEventAutoReset m_Event;
			NStorage::TCUniquePointer<NThread::CThreadObjectNonTracked, NMemory::CAllocator_NonTrackedHeap> m_pThread;
			NAtomic::TCAtomic<bool> m_bThreadCreated;
#if DMibConfig_Concurrency_LocalFirstScheduler && DMibConfig_Concurrency_LocalFirstDistribution
#endif
#if DMibConfig_Concurrency_SchedulerStats
			CSchedulerQueueStats m_SchedulerStats;
#endif
			CQueue(CQueue &&_Other);
			CQueue();
			void f_Signal(CConcurrencyManager *_pThis);
			void fp_CreateThread(CConcurrencyManager *_pThis);
		};

		void fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread);
		void fp_QueueJob(EPriority _Priority, umint _iFixedCore, umint _iLastCore, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CQueue &_Queue, FActorQueueDispatchNoAlloc &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CQueue &_Queue, NStorage::TCUniquePointer<CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc> &&_pQueueEntry, CConcurrencyThreadLocal &_ThreadLocal);

#if DMibConfig_Concurrency_LocalFirstScheduler
		umint fp_ClaimIdleQueue(EPriority _Priority, umint _iExclude);
		void fp_SetQueueIdle(CQueue &_Queue);
		void fp_ClearQueueIdle(CQueue &_Queue);
#if DMibConfig_Concurrency_LocalFirstDistribution
		void fp_OfferExcessWork(CQueue &_Queue, bool _bTransfer, umint _TargetSize = DMibConfig_Concurrency_LocalQueueTargetSize);
#endif
#endif

		inline_never umint fp_InitConcurrentActors();
		void fp_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		void fp_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);

		void fp_AddedActor();
		void fp_RemovedActor();
		umint fp_NumActors();

#if DMibConfig_Concurrency_SchedulerStats
		inline_never void fp_DumpSchedulerStats();
#endif

		struct align_cacheline CNumActorsPerQueue
		{
			NAtomic::TCAtomic<smint> m_nActors = 0;
		};

		struct align_cacheline CNumActorsOther
		{
			NAtomic::TCAtomic<smint> m_nActors = 0;
		};

		NContainer::TCVector<CNumActorsPerQueue> m_nActorsPerQueue[EPriority_Max];
		CNumActorsPerQueue *m_nActorsPerQueueArray[EPriority_Max];
		NContainer::TCVector<CNumActorsOther> m_nActorsOther;
		umint m_ActorsOtherMask;

#if DMibConfig_Concurrency_DebugBlockDestroy
		NThread::CLowLevelLock m_ActorListLock;
		DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
#endif

		umint m_nThreads = 0;
		NContainer::TCVector<CQueue> m_Queues[EPriority_Max];

#if DMibConfig_Concurrency_LocalFirstScheduler
		/// \brief One chunk of the advisory idle-queue bitmask; a set bit marks a queue whose thread
		/// is asleep (or not yet created) and may be claimed as a handoff target
		struct align_cacheline CIdleQueueMask
		{
			NAtomic::TCAtomic<umint> m_Mask = 0;
		};

		NContainer::TCVector<CIdleQueueMask> m_IdleQueueMasks[EPriority_Max];
#endif

		bool m_bDestroyed = false;
		bool m_bFinishedDestroying = false;
		bool m_bStopped = false;
		bool m_bShutdownLogging = false;

		align_cacheline NAtomic::TCAtomic<umint> m_nConcurrentActors;
		NThread::CLowLevelLock m_pConcurrentActorLock;

		NContainer::TCVector<TCActor<CConcurrentActorImpl>> m_ConcurrentActors[EPriority_Max];
		NContainer::TCVector<TCActor<CConcurrentActor>> m_ConcurrentActorsRef[EPriority_Max];

		NThread::CLowLevelLock m_BlockingActorsLock;
		NContainer::TCVector<NStorage::TCSharedPointer<CBlockingActorStorage>> m_BlockingActors;
		DMibListLinkDS_List(CBlockingActorStorage, m_Link) m_FreeBlockingActors;
		umint m_nBlockingActors = 0;

		NAtomic::TCAtomic<bool> m_bTimerActorInit;
		NThread::CLowLevelLock m_TimerActorLock;
		TCActor<CTimerActor> m_pTimerActor;

		NThread::CLowLevelLock m_ThreadCreateLock;

		TCActor<CDirectCallActorImpl> m_DirectCallActor;
		TCActor<CThisConcurrentActorImpl> m_ThisConcurrentActor;
		TCActor<CThisConcurrentActorLowPrioImpl> m_ThisConcurrentActorLowPrio;
		TCActor<CThisConcurrentActorHighCPUImpl> m_ThisConcurrentActorHighCPU;
		TCActor<COtherConcurrentActorImpl> m_OtherConcurrentActor;
		TCActor<COtherConcurrentActorLowPrioImpl> m_OtherConcurrentActorLowPrio;
		TCActor<COtherConcurrentActorHighCPUImpl> m_OtherConcurrentActorHighCPU;
		TCActor<CDynamicConcurrentActorImpl> m_DynamicConcurrentActor;
		TCActor<CDynamicConcurrentActorLowPrioImpl> m_DynamicConcurrentActorLowPrio;
		TCActor<CDynamicConcurrentActorHighCPUImpl> m_DynamicConcurrentActorHighCPU;

		TCActor<CDirectCallActor> m_DirectCallActorRef;
		TCActor<CThisConcurrentActor> m_ThisConcurrentActorRef;
		TCActor<CThisConcurrentActorLowPrio> m_ThisConcurrentActorLowPrioRef;
		TCActor<CThisConcurrentActorHighCPU> m_ThisConcurrentActorHighCPURef;
		TCActor<COtherConcurrentActor> m_OtherConcurrentActorRef;
		TCActor<COtherConcurrentActorLowPrio> m_OtherConcurrentActorLowPrioRef;
		TCActor<COtherConcurrentActorHighCPU> m_OtherConcurrentActorHighCPURef;
		TCActor<CDynamicConcurrentActor> m_DynamicConcurrentActorRef;
		TCActor<CDynamicConcurrentActorLowPrio> m_DynamicConcurrentActorLowPrioRef;
		TCActor<CDynamicConcurrentActorHighCPU> m_DynamicConcurrentActorHighCPURef;

		NAtomic::TCAtomic<bool> m_bDestroyingAlwaysAliveActors = false;
		EExecutionPriority m_ExecutionPriority[EPriority_Max] = {EExecutionPriority_Lowest, EExecutionPriority_Normal, EExecutionPriority_Normal};
	};

	struct CCaptureExceptionSettings
	{
		FExceptionTransformer m_fTransformer;
	};

	struct CConcurrencyThreadLocal final
	{
		CConcurrencyThreadLocal();
		~CConcurrencyThreadLocal();

		CActorHolder *m_pCurrentlyProcessingActorHolder = nullptr;
		CActorHolder *m_pCurrentlyDestructingActorHolder = nullptr;
		CConcurrencyManager::CQueue *m_pThisQueue = nullptr;
		CSystemThreadLocal &m_SystemThreadLocal;
		umint m_iConcurrentActor[EPriority_Max];
		umint m_JobQueueIndex[EPriority_Max];
		umint m_nActorsIndex = NMisc::fg_GetRandomUnsigned();
		umint m_nProcessedEntries = 0;
		umint m_nRunsSinceGarbageCollect = 0;
		int64 m_LastArenaCollectCycles = 0;

		NException::CExceptionPointer m_pNoResultException;
		NException::CExceptionPointer m_pResultWasNotSetException;
		NException::CExceptionPointer m_pActorDeletedException;
		NException::CExceptionPointer m_pActorCalledDeletedException;
		NException::CExceptionPointer m_pActorAlreadyDestroyedException;

		NContainer::TCVector<CCaptureExceptionSettings> m_PendingCaptureExceptions;
		NContainer::TCVector<TCFuture<void>> m_AsyncDestructors;

		bool m_bCurrentlyProcessingInActorHolder = false;
		bool m_bForceNonLocal = false;
		bool m_bCaptureAsyncDestructors = false;

#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks *m_pCallstacks = nullptr;
#endif

#if DMibEnableSafeCheck > 0
		CActor *m_pCurrentlyConstructingActor = nullptr;
		CActor *m_pCurrentlyDestructingActor = nullptr;
		umint m_AllowWrongThreadDestroySequence = 0;
#endif
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
		umint m_nWaits = 0;
		bool m_bForceWakeUp = false;
		bool m_bForceBusyWait = false;
#endif
	};

	struct CAllowWrongThreadDestroy
	{
		~CAllowWrongThreadDestroy();
	};

	extern CAllowWrongThreadDestroy g_AllowWrongThreadDestroy;

	bool fg_ActorRunning(TCActor<> &_Actor);

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params);

	template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params);
}

#include "Malterlib_Concurrency_ActorHelpers.h"
#include "Malterlib_Concurrency_ActorCallHelpers.h"
#include "Malterlib_Concurrency_Utils.h"

#include "Malterlib_Concurrency_ReceiveAnyFunctor.hpp"
#include "Malterlib_Concurrency_Actor.hpp"
#include "Malterlib_Concurrency_ActorHolder.hpp"
#include "Malterlib_Concurrency_ActorInternal.hpp"
#include "Malterlib_Concurrency_Promise.hpp"
#include "Malterlib_Concurrency_Manager.hpp"
#include "Malterlib_Concurrency_Promise_Coroutine.hpp"
#include "Malterlib_Concurrency_WeakActor.hpp"
#include "Malterlib_Concurrency_Instantiate.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
