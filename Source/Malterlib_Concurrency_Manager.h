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
			
			struct CLocalSemaphore : public NThread::CSemaphore
			{
				CLocalSemaphore();
			};
			
			NThread::CMutual m_ActorListLock;
			DMibListLinkDS_List(CActorHolder, m_ActorLink) m_Actors;
			NContainer::TCThreadSafeQueue<NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag>> m_JobQueue[EPriority_Max];
			CLocalSemaphore m_JobSemaphore[EPriority_Max];

			NContainer::TCVector<NPtr::TCUniquePointer<NThread::CThreadObjectNonTracked, NMem::CAllocator_NonTrackedHeap>, NMem::CAllocator_NonTrackedHeap> m_Threads[EPriority_Max];

			NThread::CMutual m_pConcurrentActorLock;
			TCActor<CConcurrentActor> m_pConcurrentActor;
			TCActor<CConcurrentActorLowPrio> m_pConcurrentActorLowPrio;
			NThread::CMutual m_pTimerActorLock;
			TCActor<CTimerActor> m_pTimerActor;

			void fp_RunThread(EPriority _Priority, NThread::CThreadObjectNonTracked *_pThread);
			void fp_QueueJob(EPriority _Priority, NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_ToQueue)
			{
				m_JobQueue[_Priority].f_Push(fg_Move(_ToQueue));
				m_JobSemaphore[_Priority].f_Signal();
			}

		public:
			struct CThreadLocal
			{
				CActor *m_pCurrentActor = nullptr;
#if DMibConcurrencyDebugActorCallstacks
				CAsyncCallstacks *m_pCallstacks = nullptr;
#endif
			};
			
			mutable NThread::TCThreadLocal<CThreadLocal, NMem::CAllocator_Heap, NThread::EThreadLocalFlag_AlwaysCreated> m_ThreadLocal;

		public:
			CConcurrencyManager();
			~CConcurrencyManager();
			void f_Stop();

			template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
			TCActor<tf_CType> f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params);

			void f_BlockOnDestroy();
			TCActor<CConcurrentActor> const &f_GetConcurrentActor();
			TCActor<CConcurrentActorLowPrio> const &f_GetConcurrentActorLowPrio();
			TCActor<CTimerActor> const &f_GetTimerActor();

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
