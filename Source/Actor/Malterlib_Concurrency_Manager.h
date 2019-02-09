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
		CConcurrencyManager();
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

		void f_BlockOnDestroy();
		TCActor<CConcurrentActor> const &f_GetConcurrentActor();
		TCActor<CConcurrentActor> const &f_GetConcurrentActorLowPrio();
		TCActor<CConcurrentActor> const &f_GetConcurrentActorForThisThread(EPriority _Priority);
		TCActor<CTimerActor> const &f_GetTimerActor();
		TCActor<CDirectCallActor> const &f_GetDirectCallActor();

		void f_DispatchFirstOnCurrentThread(EPriority _Priority, FActorQueueDispatch &&_ToQueue);
		void f_DispatchOnNextThread(EPriority _Priority, FActorQueueDispatch &&_ToQueue);

	private:
		template <typename t_CActor>
		friend class TCActorInternal;

		friend class CActorHolder;
		friend class CActor;
		friend class CDefaultActorHolder;
		friend struct CCurrentActorScope;
		friend struct CConcurrencyThreadLocal;

		struct CQueue
		{
			align_cacheline NAtomic::TCAtomic<mint> m_Working;
			CConcurrentRunQueue m_JobQueue;
			mint m_iQueue;
			EPriority m_Priority;
			NThread::CEventAutoReset m_Event;
			NStorage::TCUniquePointer<NThread::CThreadObjectNonTracked, NMemory::CAllocator_NonTrackedHeap> m_pThread;
			CQueue(CQueue &&_Other);
			CQueue();
			void f_Signal(CConcurrencyManager *_pThis);
			void fp_CreateThread(CConcurrencyManager *_pThis);
		};

		void fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread);
		void fp_QueueJob(EPriority _Priority, mint _iFixedCore, FActorQueueDispatch &&_ToQueue);
		bool fp_AddToQueue(CQueue &_Queue, FActorQueueDispatch &&_Functor);
		inline_never mint fp_InitConcurrentActors();

		NAtomic::TCAtomic<mint> m_nActors;
#if DMibConfig_Concurrency_DebugBlockDestroy
		NThread::CMutual m_ActorListLock;
		DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
#endif

		mint m_nThreads = 0;
		NContainer::TCVector<CQueue> m_Queues[EPriority_Max];

		bool m_bDestroyed = false;

		align_cacheline NAtomic::TCAtomic<mint> m_nConcurrentActors;
		NThread::CMutual m_pConcurrentActorLock;
		NContainer::TCVector<TCActor<CConcurrentActor>> m_ConcurrentActors[EPriority_Max];
		NThread::CMutual m_pTimerActorLock;
		TCActor<CTimerActor> m_pTimerActor;
		NThread::CMutual m_ThreadCreateLock;
		TCActor<CDirectCallActor> m_DirectCallActor;
	};

	struct CConcurrencyThreadLocal
	{
		CConcurrencyThreadLocal();
		~CConcurrencyThreadLocal();

		CActor *m_pCurrentActor = nullptr;
		CActorHolder *m_pCurrentlyProcessingActorHolder = nullptr;
#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks *m_pCallstacks = nullptr;
#endif
		CConcurrencyManager::CQueue *m_pThisQueue = nullptr;
		mint m_iConcurrentActor[EPriority_Max];
		mint m_JobQueueIndex[EPriority_Max];
#if defined DMibContractConfigure_CheckEnabled
		CActor *m_pCurrentlyConstructingActor = nullptr;
		mint m_AllowWrongThreadDestroySequence = 0;
#endif
	};

	struct CAllowWrongThreadDestroy
	{
		~CAllowWrongThreadDestroy();
	};

	extern CAllowWrongThreadDestroy g_AllowWrongThreadDestroy;

	CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal();

	bool fg_CurrentActorRunning();

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params);

	template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params);

	auto fg_DiscardResult() -> decltype(fg_ConcurrentActor() / NPrivate::CDiscardResultFunctor());

	COnScopeExitShared fg_OnScopeExitActor(TCActor<> const &_Actor, NFunction::TCFunctionMovable<void ()> &&_fOnExitFunctor);
}

#include "Malterlib_Concurrency_ActorHelpers.h"
#include "Malterlib_Concurrency_ActorCallHelpers.h"
#include "Malterlib_Concurrency_Utils.h"


#include "Malterlib_Concurrency_Actor.hpp"
#include "Malterlib_Concurrency_ActorHolder.hpp"
#include "Malterlib_Concurrency_ActorInternal.hpp"
#include "Malterlib_Concurrency_Manager.hpp"
#include "Malterlib_Concurrency_Promise_Coroutine.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
