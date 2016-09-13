// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Function/Function>
#include <Mib/Meta/Meta>
#include <Mib/Storage/Tuple>

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Helpers.h"
#include "Malterlib_Concurrency_Continuation.h"
#include "Malterlib_Concurrency_Actor.h"
#include "Malterlib_Concurrency_DefaultActors.h"
#include "Malterlib_Concurrency_ActorHolder.h"
#include "Malterlib_Concurrency_InternalCallWithAsyncResult.h"
#include "Malterlib_Concurrency_ActorInternal.h"

namespace NMib
{
	namespace NConcurrency
	{
		class CConcurrencyManager
		{
			template <typename t_CActor>
			friend class TCActorInternal;

			friend class CActorHolder;
			friend class CActor;
			friend class CDefaultActorHolder;

			NAtomic::TCAtomic<mint> m_nActors;
#ifdef DMibDebug
			NThread::CMutual m_ActorListLock;
			DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
#endif
			
			struct CQueue
			{
				align_cacheline NAtomic::TCAtomic<mint> m_Working;
				CConcurrentRunQueue m_JobQueue;
				NThread::CEventAutoReset m_Event;
				NPtr::TCUniquePointer<NThread::CThreadObjectNonTracked, NMem::CAllocator_NonTrackedHeap> m_pThread;
				CQueue(CQueue &&_Other);
				CQueue();
			};
			
			mint m_nThreads = 0;
			NContainer::TCVector<CQueue> m_Queues[EPriority_Max];
			
			bool m_bDestroyed = false;

			align_cacheline NAtomic::TCAtomic<mint> m_nConcurrentActors;
			NThread::CMutual m_pConcurrentActorLock;
			NContainer::TCVector<TCActor<CConcurrentActor>> m_ConcurrentActors[EPriority_Max];
			NThread::CMutual m_pTimerActorLock;
			TCActor<CTimerActor> m_pTimerActor;

			void fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread);
			void fp_QueueJob(EPriority _Priority, mint _iFixedCore, FActorQueueDispatch &&_ToQueue);
			bool fp_AddToQueue(CQueue &_Queue, FActorQueueDispatch &&_Functor);

		public:
			struct CThreadLocal
			{
				CThreadLocal()
				{
					for (mint iQueue = 0; iQueue < EPriority_Max; ++iQueue)
					{
						NMisc::CRandomShiftRNG RandomGenerator
							{
								uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
								, uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
								, uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
							}
						;
						mint nCores = NSys::fg_Thread_GetVirtualCores();
						m_JobQueueIndex[iQueue] = RandomGenerator.f_GetValue<uint32>(0, nCores);
						m_iConcurrentActor[iQueue] = RandomGenerator.f_GetValue<uint32>(0, nCores);
					}
				}
				
				CActor *m_pCurrentActor = nullptr;
				CActorHolder *m_pCurrentlyProcessingActorHolder = nullptr;
#if DMibConcurrencyDebugActorCallstacks
				CAsyncCallstacks *m_pCallstacks = nullptr;
#endif
				CQueue *m_pThisQueue = nullptr;
				mint m_iConcurrentActor[EPriority_Max];
				mint m_JobQueueIndex[EPriority_Max];
			};
			
			mutable NThread::TCThreadLocal<CThreadLocal, NMem::CAllocator_Heap, NThread::EThreadLocalFlag_AlwaysCreated> m_ThreadLocal;

			inline_never mint fp_InitConcurrentActors();
			
		public:
			CConcurrencyManager();
			~CConcurrencyManager();
			void f_Stop();

			template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
			TCActor<tf_CType> f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params);

			template <typename tf_CType, typename... tfp_CParams>
			TCActor<tf_CType> f_ConstructFromInternalActor
				(
					NPtr::TCSharedPointer<TCActorInternal<tf_CType>, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pInternalActor
					, TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams
				)
			;

			void f_BlockOnDestroy();
			TCActor<CConcurrentActor> const &f_GetConcurrentActor();
			TCActor<CConcurrentActor> const &f_GetConcurrentActorLowPrio();
			TCActor<CConcurrentActor> const &f_GetConcurrentActorForThisThread(EPriority _Priority);
			TCActor<CTimerActor> const &f_GetTimerActor();
			TCActor<CActor> f_CurrentActor();
			
			void f_DispatchFirstOnCurrentThread(EPriority _Priority, FActorQueueDispatch &&_ToQueue);
			void f_DispatchOnNextThread(EPriority _Priority, FActorQueueDispatch &&_ToQueue);
		};
		
	
		template <typename tf_CActor, typename... tfp_CParams>
		TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params);
		
		template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
		TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params);
		
		auto fg_DiscardResult() -> decltype(fg_ConcurrentActor() / NPrivate::CDiscardResultFunctor());
	}
}

#include "Malterlib_Concurrency_ActorHelpers.h"
#include "Malterlib_Concurrency_ActorCallHelpers.h"
#include "Malterlib_Concurrency_Utils.h"

		
#include "Malterlib_Concurrency_Actor.hpp"
#include "Malterlib_Concurrency_ActorHolder.hpp"
#include "Malterlib_Concurrency_ActorInternal.hpp"
#include "Malterlib_Concurrency_Manager.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
