// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Function/Function>
#include <Mib/Meta/Meta>
#include <Mib/Storage/Tuple>
#include <Mib/Thread/Local>
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
	namespace NPrivate
	{
		umint fg_AllocateSingletonIndex(NAtomic::TCAtomic<umint> &_Index);

		template <typename tf_CActor>
		umint fg_GetSingletonIndex();

#if DMibConfig_Concurrency_DebugFutures
		// The futures and coroutines of the process, whichever manager runs them. Kept outside the managers, as getting the default
		// manager constructs it, which must not happen while a coroutine is being set up
		struct CDebugFutures
		{
			NThread::CLowLevelLock m_Lock;
			DMibListLinkDS_List(CPromiseDataBase, m_Link) m_Futures;
			DMibListLinkDS_List(CPromiseDataBase, m_LinkCoro) m_Coroutines;
		};

		CDebugFutures &fg_DebugFutures();
#endif
	}

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

	struct CIoLoopBinding
	{
		explicit operator bool () const;

		NSys::ICIoLoop *m_pLoop = nullptr;
		umint m_iQueue = TCLimitsInt<umint>::mc_Max;
		EPriority m_Priority = EPriority_Max;
	};

	// Thread-local binding for newly created I/O objects. A coroutine must close the scope before suspending.
	struct [[nodiscard]] CIoLoopCreateScope
	{
		CIoLoopCreateScope(CIoLoopBinding const &_Binding);
		~CIoLoopCreateScope();

		CIoLoopCreateScope(CIoLoopCreateScope const &_Other) = delete;
		CIoLoopCreateScope &operator = (CIoLoopCreateScope const &_Other) = delete;

	private:
		DMibThreadLocalScopeDebugMember;
		NSys::ICIoLoop *mp_pPreviousLoop = nullptr; // Restored on close so scopes nest.
		bool mp_bSet = false;
	};

	/// \brief Manages scheduling of running actors in a thread pool
	struct CConcurrencyManager
	{
		CConcurrencyManager(EExecutionPriority _ExecutionPriority[EPriority_Max], EPriority _PriorityClamp = EPriority_Low);
		~CConcurrencyManager();
		void f_Init();
		void f_Stop();
		void f_EnableShutdownLogging(bool _bEnabled);

		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
		TCActor<tf_CType> f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams && ...p_Params);

		template <typename tf_CType, typename ...tfp_CParams>
		TCActor<tf_CType> f_ConstructFromInternalActor(TCActorHolderSharedPointer<TCActorInternal<tf_CType>> &&_pInternalActor, TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams);

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
		TCActor<CTimerActor> const &f_GetTimerActor(TCActor<CActor> const &_Actor);
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

		void f_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_fToQueue);
		void f_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_fToQueue);

		void f_DispatchToQueue(EPriority _Priority, umint _iQueue, FActorQueueDispatchNoAlloc &&_fToQueue);

		void f_EnableQueueIoLoop(EPriority _Priority, umint _iQueue);
		NSys::ICIoLoop *f_GetQueueIoLoop(EPriority _Priority, umint _iQueue) const;
		NSys::ICIoLoop *f_GetThreadIoLoop();

		CIoLoopBinding f_PickIoLoopBinding(EPriority _Priority);

		inline_always EPriority f_ClampPriority(EPriority _Priority) const;
		EPriority f_GetQueuePriority() const;
		umint f_GetNumQueues(EPriority _Priority) const;

		void f_SetExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority);
		EExecutionPriority f_GetExecutionPriority(EPriority _Priority);

		template <typename tf_CActor, typename tf_FCreate>
		TCWrapped<TCActor<tf_CActor>> f_GetSingleton(tf_FCreate const &_fCreate);

		bool f_DestroyingAlwaysAliveActors() const;

		umint f_GetConcurrency() const;
		umint f_GetQueue() const;

#if DMibConfig_Concurrency_SchedulerStats
		/// \brief Snapshot of aggregated scheduling statistics
		struct CSchedulerStats
		{
			umint m_nSignals = 0;                  ///< Semaphore signals posted to pool threads
			umint m_nSleeps = 0;                   ///< Times pool threads went to sleep
			umint m_nLocalEnqueues = 0;            ///< Jobs enqueued to the producing thread's own local queue
			umint m_nAtomicEnqueues = 0;           ///< Jobs enqueued through the atomic queue
			umint m_nCurrentThreadSelections = 0;  ///< Jobs targeted at the producing pool thread's queue
			umint m_nFixedSelections = 0;          ///< Jobs targeted by fixed-queue pinning
			umint m_nForcedNonLocalSelections = 0; ///< Jobs redistributed after a yield
			umint m_nLastQueueSelections = 0;      ///< Jobs targeted by last-queue affinity
			umint m_nRoundRobinSelections = 0;     ///< Jobs targeted by round-robin selection
			umint m_nHandoffs = 0;                 ///< Jobs handed off to idle threads
			umint m_nIdleClaims = 0;               ///< Successful claims of idle threads
			umint m_nRunProcess = 0;               ///< Actor drain runs executed on pool threads
			umint m_nEntriesDrained = 0;           ///< Actor messages processed by drain runs on pool threads
			umint m_nLongDrains = 0;               ///< Drain runs that processed 16 or more messages
			umint m_nGarbageCollectCalls = 0;      ///< Owner-side arena garbage collect attempts from drain runs
			umint m_nGarbageCollects = 0;          ///< Owner-side arena garbage collects that found pending messages
		};

		CSchedulerStats f_GetSchedulerStats(EPriority _Priority);
		CSchedulerStats f_GetSchedulerStats();
		void f_ResetSchedulerStats();
#endif

#if DMibConfig_Concurrency_DebugFutures
		bool m_bReportFutures = false; // Set on the default manager, which reports the futures that outlive it
#endif
	private:
		template <typename t_CActor>
		friend struct TCActorInternal;

		friend struct CActorHolder;
		friend struct CActorCommon;
		friend struct CActor;
		friend struct CTimerActor;
		friend struct CTimerActorHolder;
		friend struct CDefaultActorHolder;
		friend struct CCurrentActorScope;
		friend struct CCurrentlyProcessingActorScope;
		friend struct CConcurrencyThreadLocal;
		friend struct CBlockingActorCheckout;

#if DMibConfig_Concurrency_SchedulerStats
		/// \brief Scheduling statistics for one pool queue
		struct align_cacheline CSchedulerQueueStats
		{
			NAtomic::TCAtomic<umint> m_nSignals = 0;                  ///< Semaphore signals posted to this queue
			NAtomic::TCAtomic<umint> m_nSleeps = 0;                   ///< Times this queue's thread went to sleep
			NAtomic::TCAtomic<umint> m_nLocalEnqueues = 0;            ///< Jobs enqueued from this queue's own thread
			NAtomic::TCAtomic<umint> m_nAtomicEnqueues = 0;           ///< Jobs enqueued through the atomic queue
			NAtomic::TCAtomic<umint> m_nCurrentThreadSelections = 0;  ///< Jobs targeted at this queue because the producer runs on it
			NAtomic::TCAtomic<umint> m_nFixedSelections = 0;          ///< Jobs targeted at this queue by fixed-queue pinning
			NAtomic::TCAtomic<umint> m_nForcedNonLocalSelections = 0; ///< Jobs targeted at this queue after a yield
			NAtomic::TCAtomic<umint> m_nLastQueueSelections = 0;      ///< Jobs targeted at this queue by last-queue affinity
			NAtomic::TCAtomic<umint> m_nRoundRobinSelections = 0;     ///< Jobs targeted at this queue by round-robin selection
			NAtomic::TCAtomic<umint> m_nHandoffs = 0;                 ///< Jobs handed off from this queue to an idle queue
			NAtomic::TCAtomic<umint> m_nIdleClaims = 0;               ///< Successful idle claims of this queue by producers
			NAtomic::TCAtomic<umint> m_nRunProcess = 0;               ///< Actor drain runs executed on this queue's thread
			NAtomic::TCAtomic<umint> m_nEntriesDrained = 0;           ///< Actor messages processed by drain runs on this queue's thread
			NAtomic::TCAtomic<umint> m_nLongDrains = 0;               ///< Drain runs on this queue's thread that processed 16 or more messages
			NAtomic::TCAtomic<umint> m_nGarbageCollectCalls = 0;      ///< Owner-side arena garbage collect attempts from drain runs
			NAtomic::TCAtomic<umint> m_nGarbageCollects = 0;          ///< Owner-side arena garbage collects that found pending messages
		};
#endif

		struct CSingleton
		{
			virtual ~CSingleton();

			virtual TCFuture<void> f_Destroy() = 0;
		};

		template <typename t_CActor>
		struct TCSingleton final : CSingleton
		{
			TCSingleton(TCActor<t_CActor> &&_Actor);

			TCFuture<void> f_Destroy() override;

			TCActor<t_CActor> m_Actor;
		};

		struct CQueue
		{
			align_cacheline CConcurrentRunQueueNonVirtualNoAlloc m_JobQueue;
			NAtomic::TCAtomic<umint> m_Working;

			align_cacheline CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData m_JobQueueLocal;
			umint m_iQueue;
			TCActor<CTimerActorImpl> m_TimerActor;
			TCActor<CTimerActor> m_TimerActorRef;
			bool m_bTimerWorkEnabled = true;  // Cleared by timer teardown before releasing its holder.
			fp64 m_TimerDeadline = -1.0;      // Owner-only deadline on the manager clock; negative when disarmed.
			EPriority m_Priority;
			bool m_bIoLoopParkActive = false; // Only the owner thread reads or writes this park choice.

			align_cacheline NThread::CEventAutoReset m_Event; // Co-locate immutable loop fields with the event dirtied by signallers to avoid a second cache-line transfer.
			NSys::ICIoLoop *m_pIoLoop = nullptr;
			NStorage::TCUniquePointer<NThread::CThreadObjectNonTracked, NMemory::CAllocator_NonTrackedHeap> m_pThread;
			NAtomic::TCAtomic<bool> m_bThreadCreated;
			bool m_bIoLoopParksOnEvent = false;               // The loop waits on m_Event, so signalling it needs no explicit loop wake.

#if DMibConfig_Concurrency_SchedulerStats
			CSchedulerQueueStats m_SchedulerStats;
#endif

			CQueue(CQueue &&_Other);
			CQueue();
			void f_Signal(CConcurrencyManager *_pThis);

		private:
			void fp_CreateThread(CConcurrencyManager *_pThis);
		};

		void fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread);
		void fp_QueueJob(EPriority _Priority, umint _iFixedCore, umint _iLastCore, FActorQueueDispatchNoAlloc &&_fToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CQueue &_Queue, FActorQueueDispatchNoAlloc &&_fFunctor, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CQueue &_Queue, NStorage::TCUniquePointer<CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc> &&_pQueueEntry, CConcurrencyThreadLocal &_ThreadLocal);

#if DMibConfig_Concurrency_LocalFirstScheduler
		umint fp_ClaimIdleQueue(EPriority _Priority, umint _iExclude);
		void fp_SetQueueIdle(CQueue &_Queue);
		void fp_ClearQueueIdle(CQueue &_Queue);
#	if DMibConfig_Concurrency_LocalFirstDistribution
		void fp_OfferExcessWork(CQueue &_Queue, bool _bTransfer, umint _TargetSize = DMibConfig_Concurrency_LocalQueueTargetSize);
#	endif
#endif
		void fp_DrainCheckpoint(CQueue &_Queue, umint _OfferTargetSize);
		bool fp_CheckTimers(CQueue &_Queue);

		inline_never umint fp_InitConcurrentActors();
		void fp_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_fToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		void fp_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_fToQueue, CConcurrencyThreadLocal &_ThreadLocal);

		static TCFuture<void> fsp_DestroySingletons(NContainer::TCVector<NStorage::TCUniquePointer<CSingleton>> _Singletons, NContainer::TCVector<umint> _CreationOrder);

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

		NContainer::TCVector<CNumActorsPerQueue> mp_NumActorsPerQueue[EPriority_Max];
		CNumActorsPerQueue *mp_NumActorsPerQueueArray[EPriority_Max];
		NContainer::TCVector<CNumActorsOther> mp_nActorsOther;
		umint mp_ActorsOtherMask;

#if DMibConfig_Concurrency_DebugBlockDestroy
		NThread::CLowLevelLock m_ActorListLock;
		DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
#endif

		umint mp_nThreads = 0;
		NContainer::TCVector<CQueue> mp_Queues[EPriority_Max];
		umint mp_NumIoLoopQueues[EPriority_Max] = {}; // Queues [0, count) own loops; fixed at startup.
		NAtomic::TCAtomic<umint> mp_NextIoLoopBinding[EPriority_Max] = {};
		NAtomic::TCAtomic<bool> mp_IoLoopsEnabled[EPriority_Max] = {};

#if DMibConfig_Concurrency_LocalFirstScheduler
		/// \brief One chunk of the advisory idle-queue bitmask; a set bit marks a queue whose thread
		/// is asleep (or not yet created) and may be claimed as a handoff target
		struct align_cacheline CIdleQueueMask
		{
			NAtomic::TCAtomic<umint> m_Mask = 0;
		};

		NContainer::TCVector<CIdleQueueMask> mp_IdleQueueMasks[EPriority_Max];
#endif

		NThread::CMutual mp_SingletonLock;                                         // Recursive, as creating a singleton can ask for another one
		NContainer::TCVector<NStorage::TCUniquePointer<CSingleton>> mp_Singletons; // Indexed by the singleton index of the type
		NContainer::TCVector<umint> mp_SingletonCreationOrder;                     // Singletons are released in reverse creation order
		bool mp_bSingletonsDestroyed = false;

		bool mp_bDestroyed = false;
		bool mp_bFinishedDestroying = false;
		bool mp_bStopped = false;
		bool mp_bShutdownLogging = false;

		align_cacheline NAtomic::TCAtomic<umint> mp_nConcurrentActors;
		NThread::CLowLevelLock mp_ConcurrentActorLock;

		NContainer::TCVector<TCActor<CConcurrentActorImpl>> mp_ConcurrentActors[EPriority_Max];
		NContainer::TCVector<TCActor<CConcurrentActor>> mp_ConcurrentActorsRef[EPriority_Max];

		NThread::CLowLevelLock mp_BlockingActorsLock;
		NContainer::TCVector<NStorage::TCSharedPointer<CBlockingActorStorage>> mp_BlockingActors;
		DMibListLinkDS_List(CBlockingActorStorage, m_Link) mp_FreeBlockingActors;
		umint mp_nBlockingActors = 0;

		NTime::CStopwatch mp_TimerStopwatch;
		NAtomic::TCAtomic<umint> mp_NextTimerQueue[EPriority_Max] = {};

		NThread::CLowLevelLock mp_ThreadCreateLock;

		TCActor<CDirectCallActorImpl> mp_DirectCallActor;
		TCActor<CThisConcurrentActorImpl> mp_ThisConcurrentActor;
		TCActor<CThisConcurrentActorLowPrioImpl> mp_ThisConcurrentActorLowPrio;
		TCActor<CThisConcurrentActorHighCPUImpl> mp_ThisConcurrentActorHighCPU;
		TCActor<COtherConcurrentActorImpl> mp_OtherConcurrentActor;
		TCActor<COtherConcurrentActorLowPrioImpl> mp_OtherConcurrentActorLowPrio;
		TCActor<COtherConcurrentActorHighCPUImpl> mp_OtherConcurrentActorHighCPU;
		TCActor<CDynamicConcurrentActorImpl> mp_DynamicConcurrentActor;
		TCActor<CDynamicConcurrentActorLowPrioImpl> mp_DynamicConcurrentActorLowPrio;
		TCActor<CDynamicConcurrentActorHighCPUImpl> mp_DynamicConcurrentActorHighCPU;

		TCActor<CDirectCallActor> mp_DirectCallActorRef;
		TCActor<CThisConcurrentActor> mp_ThisConcurrentActorRef;
		TCActor<CThisConcurrentActorLowPrio> mp_ThisConcurrentActorLowPrioRef;
		TCActor<CThisConcurrentActorHighCPU> mp_ThisConcurrentActorHighCPURef;
		TCActor<COtherConcurrentActor> mp_OtherConcurrentActorRef;
		TCActor<COtherConcurrentActorLowPrio> mp_OtherConcurrentActorLowPrioRef;
		TCActor<COtherConcurrentActorHighCPU> mp_OtherConcurrentActorHighCPURef;
		TCActor<CDynamicConcurrentActor> mp_DynamicConcurrentActorRef;
		TCActor<CDynamicConcurrentActorLowPrio> mp_DynamicConcurrentActorLowPrioRef;
		TCActor<CDynamicConcurrentActorHighCPU> mp_DynamicConcurrentActorHighCPURef;

		EPriority mp_PriorityClamp = EPriority_Low; // Bands below this are raised to it, so a clamped band never gets a worker thread.
		NAtomic::TCAtomic<bool> mp_bDestroyingAlwaysAliveActors = false;
		EExecutionPriority mp_ExecutionPriority[EPriority_Max] = {EExecutionPriority_Low, EExecutionPriority_Normal, EExecutionPriority_Normal};
	};

	struct CCaptureExceptionSettings
	{
		FExceptionTransformer m_fTransformer;
	};

	struct CConcurrencyThreadLocal final
	{
		static constexpr bool mc_bThreadLocalConstructForThread = true;

		CConcurrencyThreadLocal(NThread::CThreadLocalConstructionContext const &_Context);
		~CConcurrencyThreadLocal();

		CActorHolder *m_pCurrentlyProcessingActorHolder = nullptr;
		CActorHolder *m_pCurrentlyDestructingActorHolder = nullptr;
		CConcurrencyManager::CQueue *m_pThisQueue = nullptr;
		CSystemThreadLocal &m_SystemThreadLocal;
		umint m_ConcurrentActorIndex[EPriority_Max];
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

	template <typename tf_CActor, typename ...tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams && ...p_Params);

	template <typename tf_CActor, typename tf_CHolderType, typename ...tfp_CHolderParams, typename ...tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams && ...p_Params);
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
#include "Malterlib_Concurrency_InstantiatePromise.hpp"

#ifndef DMibPNoShortCuts
using namespace NMib::NConcurrency;
#endif
