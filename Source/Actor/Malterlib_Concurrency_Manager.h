// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Function/Function>
#include <Mib/Meta/Meta>
#include <Mib/Storage/Tuple>

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
	struct CFutureCoroutineContext;

	/// \brief Manages scheduling of running actors in a thread pool
	class CConcurrencyManager
	{
	public:
		CConcurrencyManager(EExecutionPriority _ExecutionPriority[EPriority_Max]);
		~CConcurrencyManager();
		void f_Init();
		void f_Stop();

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
			requires (!NTraits::TCIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>::mc_Value)
		;

		template <typename tf_CActor>
		consteval static bool fs_HasOverridenDestroy()
			requires (NTraits::TCIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>::mc_Value)
		;

		template <typename tf_CActor>
		consteval static bool fs_HasOverridenDestroy();

		void f_BlockOnDestroy();
		TCActor<CConcurrentActor> const &f_GetConcurrentActor();
		TCActor<CConcurrentActor> const &f_GetConcurrentActor(TCWeakActor<CActor> const &_Actor);
		TCActor<CConcurrentActor> const &f_GetConcurrentActorLowPrio();
		TCActor<CConcurrentActor> const &f_GetConcurrentActorForThisThread(EPriority _Priority);
		TCActor<CConcurrentActor> const &f_GetConcurrentActorForOtherThread(EPriority _Priority);
		TCActor<CTimerActor> const &f_GetTimerActor();
		TCActor<CDirectCallActor> const &f_GetDirectCallActor();
		TCActor<CThisConcurrentActor> const &f_GetThisConcurrentActor();
		TCActor<CThisConcurrentActorLowPrio> const &f_GetThisConcurrentActorLowPrio();
		TCActor<COtherConcurrentActor> const &f_GetOtherConcurrentActor();
		TCActor<COtherConcurrentActorLowPrio> const &f_GetOtherConcurrentActorLowPrio();
		TCActor<NPrivate::CDirectResultActor> const &f_GetDirectResultActor();
		TCActor<CDynamicConcurrentActor> const &f_GetDynamicConcurrentActor();
		TCActor<CDynamicConcurrentActorLowPrio> const &f_GetDynamicConcurrentActorLowPrio();

		void f_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatch &&_ToQueue);
		void f_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatch &&_ToQueue);

		void f_SetExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority);
		EExecutionPriority f_GetExecutionPriority(EPriority _Priority);

		bool f_DestroyingAlwaysAliveActors() const;

		mint f_GetConcurrency() const;
		mint f_GetQueue() const;

#if DMibConfig_Concurrency_DebugFutures
		NThread::CMutual m_FutureListLock;
		DMibListLinkDS_List(NPrivate::CPromiseDataBase, m_Link) m_Futures;
		DMibListLinkDS_List(NPrivate::CPromiseDataBase, m_LinkCoro) m_Coroutines;
#endif
	private:
		template <typename t_CActor>
		friend class TCActorInternal;

		friend class CActorHolder;
		friend struct CActor;
		friend class CDefaultActorHolder;
		friend struct CCurrentActorScope;
		friend struct CCurrentlyProcessingActorScope;
		friend struct CConcurrencyThreadLocal;
#if DMibConfig_Concurrency_DebugFutures
		friend struct NPrivate::CPromiseDataBase;
#endif
		struct CQueue
		{
			align_cacheline CConcurrentRunQueue m_JobQueue;
			NAtomic::TCAtomic<mint> m_Working;
			align_cacheline CConcurrentRunQueue::CLocalQueueData m_JobQueueLocal;
			mint m_iQueue;
			EPriority m_Priority;
			NThread::CEventAutoReset m_Event;
			NStorage::TCUniquePointer<NThread::CThreadObjectNonTracked, NMemory::CAllocator_NonTrackedHeap> m_pThread;
			NAtomic::TCAtomic<bool> m_bThreadCreated;
			CQueue(CQueue &&_Other);
			CQueue();
			void f_Signal(CConcurrencyManager *_pThis);
			void fp_CreateThread(CConcurrencyManager *_pThis);
		};

		void fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread);
		void fp_QueueJob(EPriority _Priority, mint _iFixedCore, FActorQueueDispatch &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CQueue &_Queue, FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		inline_never mint fp_InitConcurrentActors();
		void fp_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatch &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		void fp_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatch &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);

		void fp_AddedActor();
		void fp_RemovedActor();
		mint fp_NumActors();

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
		mint m_ActorsOtherMask;

#if DMibConfig_Concurrency_DebugBlockDestroy
		NThread::CMutual m_ActorListLock;
		DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
#endif

		mint m_nThreads = 0;
		NContainer::TCVector<CQueue> m_Queues[EPriority_Max];

		bool m_bDestroyed = false;
		bool m_bStopped = false;

		align_cacheline NAtomic::TCAtomic<mint> m_nConcurrentActors;
		NThread::CMutual m_pConcurrentActorLock;

		NContainer::TCVector<TCActor<CConcurrentActorImpl>> m_ConcurrentActors[EPriority_Max];
		NContainer::TCVector<TCActor<CConcurrentActor>> m_ConcurrentActorsRef[EPriority_Max];

		NAtomic::TCAtomic<bool> m_bTimerActorInit;
		NThread::CMutual m_TimerActorLock;
		TCActor<CTimerActor> m_pTimerActor;

		NThread::CMutual m_ThreadCreateLock;

		TCActor<CDirectCallActorImpl> m_DirectCallActor;
		TCActor<CThisConcurrentActorImpl> m_ThisConcurrentActor;
		TCActor<CThisConcurrentActorLowPrioImpl> m_ThisConcurrentActorLowPrio;
		TCActor<COtherConcurrentActorImpl> m_OtherConcurrentActor;
		TCActor<COtherConcurrentActorLowPrioImpl> m_OtherConcurrentActorLowPrio;
		TCActor<CDynamicConcurrentActorImpl> m_DynamicConcurrentActor;
		TCActor<CDynamicConcurrentActorLowPrioImpl> m_DynamicConcurrentActorLowPrio;
		TCActor<NPrivate::CDirectResultActorImpl> m_DirectResultActor;

		TCActor<CDirectCallActor> m_DirectCallActorRef;
		TCActor<CThisConcurrentActor> m_ThisConcurrentActorRef;
		TCActor<CThisConcurrentActorLowPrio> m_ThisConcurrentActorLowPrioRef;
		TCActor<COtherConcurrentActor> m_OtherConcurrentActorRef;
		TCActor<COtherConcurrentActorLowPrio> m_OtherConcurrentActorLowPrioRef;
		TCActor<CDynamicConcurrentActor> m_DynamicConcurrentActorRef;
		TCActor<CDynamicConcurrentActorLowPrio> m_DynamicConcurrentActorLowPrioRef;
		TCActor<NPrivate::CDirectResultActor> m_DirectResultActorRef;

		NAtomic::TCAtomic<bool> m_bDestroyingAlwaysAliveActors = false;
		EExecutionPriority m_ExecutionPriority[EPriority_Max] = {EExecutionPriority_Lowest, EExecutionPriority_Normal};
	};

	struct CCaptureExceptionSettings
	{
		NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> m_fTransformer;
	};

	struct CConcurrencyThreadLocal final
	{
		CConcurrencyThreadLocal();
		~CConcurrencyThreadLocal();

		CActor *m_pCurrentActor = nullptr;
		CActorHolder *m_pCurrentlyProcessingActorHolder = nullptr;
		CActorHolder *m_pCurrentlyDestructingActorHolder = nullptr;
		CConcurrencyManager::CQueue *m_pThisQueue = nullptr;
		mint m_iConcurrentActor[EPriority_Max];
		mint m_JobQueueIndex[EPriority_Max];
		mint m_nActorsIndex = NMisc::fg_GetRandomUnsigned();
		bool m_bCurrentlyProcessingInActorHolder = false;
		bool m_bForceNonLocal = false;

		NException::CExceptionPointer m_pNoResultException;

		NContainer::TCVector<CCaptureExceptionSettings> m_PendingCaptureExceptions;
		NContainer::TCVector<TCFuture<void>> m_AsyncDestructors;

		bool m_bCaptureAsyncDestructors = false;

#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks *m_pCallstacks = nullptr;
#endif

#if DMibEnableSafeCheck > 0
		CActor *m_pCurrentlyConstructingActor = nullptr;
		CActor *m_pCurrentlyDestructingActor = nullptr;
		CActorHolder *m_pCurrentlyOverridenProcessingActorHolder = nullptr;
		mint m_AllowWrongThreadDestroySequence = 0;
#endif
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
		mint m_nWaits = 0;
		bool m_bForceWakeUp = false;
#endif
	};

	struct CAllowWrongThreadDestroy
	{
		~CAllowWrongThreadDestroy();
	};

	extern CAllowWrongThreadDestroy g_AllowWrongThreadDestroy;

	CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal();

	bool fg_ActorRunning(TCActor<> &_Actor);
	bool fg_CurrentActorProcessing();
#if DMibEnableSafeCheck > 0
	bool fg_CurrentActorProcessingOrOverridden();
#endif

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params);

	template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params);

	auto fg_DiscardResult() -> decltype(NConcurrency::TCActor<NConcurrency::CDirectCallActor>() / NPrivate::CDiscardResultFunctor());
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
