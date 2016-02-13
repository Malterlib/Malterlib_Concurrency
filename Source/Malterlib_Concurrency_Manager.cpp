// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		
		namespace NPrivate
		{
			struct CSubSystem_Concurrency : public CSubSystem
			{
				NThread::CMutual m_ConcurrencyManagerLock;
				NPtr::TCUniquePointer<NConcurrency::CConcurrencyManager, NMem::CAllocator_NonTrackedHeap> m_pConcurrencyManager;

				void f_DestroyThreadSpecific() override
				{
					DMibLock(m_ConcurrencyManagerLock);
					if (m_pConcurrencyManager)
					{
						{
							DMibUnlock(m_ConcurrencyManagerLock);
							m_pConcurrencyManager->f_Stop();
						}
						m_pConcurrencyManager.f_Clear();
					}
				}
				
				CConcurrencyManager &f_GetConcurrencyManager()
				{
					if (!m_pConcurrencyManager)
					{
						DMibLock(m_ConcurrencyManagerLock);
						if (!m_pConcurrencyManager)
							m_pConcurrencyManager = fg_Construct();

						return *m_pConcurrencyManager;
					}
					return *m_pConcurrencyManager;
				}

			};
			
			TCSubSystem<CSubSystem_Concurrency, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Concurrency = {DAggregateInit};
		}
		
		NConcurrency::CConcurrencyManager &fg_ConcurrencyManager()
		{
			return NPrivate::g_SubSystem_Concurrency->f_GetConcurrencyManager();
		}

		NConcurrency::TCActor<NConcurrency::CConcurrentActor> fg_ConcurrentActor()
		{
			return fg_ConcurrencyManager().f_GetConcurrentActor();
		}
		
		NConcurrency::TCActor<NConcurrency::CConcurrentActorLowPrio> fg_ConcurrentActorLowPrio()
		{
			return fg_ConcurrencyManager().f_GetConcurrentActorLowPrio();
		}
		
		///
		/// CConcurrencyManager
		/// ===================
//#define DMibNoConcurrency

		CConcurrencyManager::CLocalSemaphore::CLocalSemaphore()
#ifdef DMibNoConcurrency
			: NThread::CSemaphore(0, 1)
#else
			: NThread::CSemaphore(0, NSys::fg_Thread_GetVirtualCores())
#endif
		{
		}
		
		CConcurrencyManager::CConcurrencyManager()
		{
#ifdef DMibNoConcurrency
			mint nThreads = 1;
#else
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
#endif
			for (EPriority Priority = EPriority_Low; Priority < EPriority_Max; Priority = static_cast<EPriority>(Priority + 1))
			{
				NStr::CStrNonTracked Name;
				EThreadPriority ThreadPrio = EThreadPriority_Normal;
				switch (Priority)
				{
				case EPriority_Low:
					Name = "Low";
					ThreadPrio = EThreadPriority_Lowest;
					break;
				case EPriority_Normal:
					Name = "Normal";
					break;
				}
				for (mint i = 0; i < nThreads; ++i)
				{
					m_Threads[Priority].f_Insert
						(
							NThread::CThreadObjectNonTracked::fs_StartThread
							(
								[this, Priority](NThread::CThreadObjectNonTracked *_pThread) -> aint
								{
									fp_RunThread(Priority, _pThread);
									return 0;
								}
								, NStr::CStrNonTracked::CFormat("MalterlibThreadPool({}) {}") << Name << i
								, ThreadPrio
							)
						)
					;
				}
			}
		}

		void CConcurrencyManager::f_Stop()
		{
			f_BlockOnDestroy();
			// Signal thread quit
			for (mint Prio = 0; Prio < EPriority_Max; ++Prio)
			{
				for (auto Iter = m_Threads[Prio].f_GetIterator(); Iter; ++Iter)
				{
					(*Iter)->f_Stop(false);
				}
				m_JobSemaphore[Prio].f_Signal(m_Threads[Prio].f_GetLen());
				m_Threads[Prio].f_Clear();
				while (auto pJob = m_JobQueue[Prio].f_Pop())
				{
					(*pJob)();
				}
			}
		}

		CConcurrencyManager::~CConcurrencyManager()
		{
			for (mint i = 0; i < EPriority_Max; ++i)
			{
				DMibCheck(m_Threads[i].f_IsEmpty());
				DMibCheck(m_JobQueue[i].f_IsEmpty());
			}
			DMibCheck(m_Actors.f_IsEmpty());
		}

		void CConcurrencyManager::fp_RunThread(EPriority _Priority, NThread::CThreadObjectNonTracked *_pThread)
		{
			while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
			{
				while (auto pJob = m_JobQueue[_Priority].f_Pop())
				{
					(*pJob)();
				}
				m_JobSemaphore[_Priority].f_Wait();
			}
		}


		void CConcurrencyManager::f_BlockOnDestroy()
		{
			f_GetConcurrentActor(); // Make sure concurrent actor is created
			f_GetConcurrentActorLowPrio(); // Make sure concurrent actor is created
			f_GetTimerActor(); // Make sure timer actor is created
			DMibLock(m_ActorListLock);
			{
				// Disregard concurrent actor from destroy
				for (auto &Actor : m_ConcurrentActors)
					Actor->m_ActorLink.f_Unlink();
				for (auto &Actor : m_ConcurrentActorsLowPrio)
					Actor->m_ActorLink.f_Unlink();
				m_pTimerActor->m_ActorLink.f_Unlink(); // Disregard timer actor from destroy
				while (!m_Actors.f_IsEmpty())
				{
					DMibUnlock(m_ActorListLock);
					NSys::fg_Thread_SmallestSleep();
				}
			}

			// Delete the timer actor
			{
				m_Actors.f_Insert(*m_pTimerActor.m_pInternalActor);
				{
					DMibUnlock(m_ActorListLock);
					DMibLock(m_pTimerActorLock);
					if (m_pTimerActor)
					{
						m_pTimerActor->f_Destroy();
						m_pTimerActor = nullptr;
					}
				}
				while (!m_Actors.f_IsEmpty())
				{
					DMibUnlock(m_ActorListLock);
					NSys::fg_Thread_SmallestSleep();
				}
			}
			
			// Finally delete the concurrent actor
			{
				for (auto &Actor : m_ConcurrentActors)
					m_Actors.f_Insert(*Actor.m_pInternalActor);
				for (auto &Actor : m_ConcurrentActorsLowPrio)
					m_Actors.f_Insert(*Actor.m_pInternalActor);
				{
					DMibUnlock(m_ActorListLock);
					DMibLock(m_pConcurrentActorLock);
					for (auto &Actor : m_ConcurrentActors)
						Actor->fp_Terminate();
					for (auto &Actor : m_ConcurrentActorsLowPrio)
						Actor->fp_Terminate();
					
					m_ConcurrentActors.f_Clear();
					m_ConcurrentActorsLowPrio.f_Clear();
				}
				while (!m_Actors.f_IsEmpty())
				{
					DMibUnlock(m_ActorListLock);
					NSys::fg_Thread_SmallestSleep();
				}
			}
		}

		inline_never mint CConcurrencyManager::fp_InitConcurrentActors()
		{
			DMibLock(m_pConcurrentActorLock);
			mint nActors = m_nConcurrentActors.f_Load();
			if (!nActors)
			{
				nActors = NSys::fg_Thread_GetVirtualCores();
				
				m_ConcurrentActors.f_SetLen(nActors);
				m_ConcurrentActorsLowPrio.f_SetLen(nActors);
				
				for (auto &Actor : m_ConcurrentActors)
					Actor = fg_ConstructActor<CConcurrentActor>();
				for (auto &Actor : m_ConcurrentActorsLowPrio)
					Actor = fg_ConstructActor<CConcurrentActorLowPrio>();
				
				m_nConcurrentActors = nActors;
			}
			return nActors;
		}
		
		TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor()
		{
			mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (!nActors)
				nActors = fp_InitConcurrentActors();
			
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_iConcurrentActor >= nActors)
				ThreadLocal.m_iConcurrentActor = 0;
			auto &ToReturn = m_ConcurrentActors.f_GetArray()[ThreadLocal.m_iConcurrentActor];
			++ThreadLocal.m_iConcurrentActor;
			return ToReturn;
		}

		TCActor<CConcurrentActorLowPrio> const &CConcurrencyManager::f_GetConcurrentActorLowPrio()
		{
			mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (!nActors)
				nActors = fp_InitConcurrentActors();
			
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_iConcurrentActorLowPrio >= nActors)
				ThreadLocal.m_iConcurrentActorLowPrio = 0;
			auto &ToReturn = m_ConcurrentActorsLowPrio.f_GetArray()[ThreadLocal.m_iConcurrentActorLowPrio];
			++ThreadLocal.m_iConcurrentActorLowPrio;
			return ToReturn;
		}

		TCActor<CTimerActor> const &CConcurrencyManager::f_GetTimerActor()
		{
			{
				if (!m_pTimerActor)
				{
					DMibLock(m_pTimerActorLock);
					if (!m_pTimerActor)
						m_pTimerActor = fg_ConstructActor<CTimerActor>();
				}
				return m_pTimerActor;
			}
		}

		auto fg_DiscardResult() -> decltype(NConcurrency::TCActor<NConcurrency::CConcurrentActor>() / NPrivate::CDiscardResultFunctor())
		{
			return NConcurrency::TCActor<NConcurrency::CConcurrentActor>() / NPrivate::CDiscardResultFunctor();
		}
		
#if DMibConcurrencyDebugActorCallstacks
		namespace NPrivate
		{
			CAsyncCallstacks *fg_SetConcurrentCallstacks(CAsyncCallstacks *_pCallstacks)
			{
				auto pOld = fg_ConcurrencyManager().m_ThreadLocal->m_pCallstacks;
				fg_ConcurrencyManager().m_ThreadLocal->m_pCallstacks = _pCallstacks;
				return pOld;
			}
		}
#endif
	}
}

