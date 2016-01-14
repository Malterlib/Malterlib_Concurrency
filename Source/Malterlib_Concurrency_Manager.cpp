// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		
		namespace
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
			return g_SubSystem_Concurrency->f_GetConcurrencyManager();
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
				m_pConcurrentActor->m_ActorLink.f_Unlink(); // Disregard concurrent actor from destroy
				m_pConcurrentActorLowPrio->m_ActorLink.f_Unlink(); // Disregard concurrent actor from destroy
				m_pTimerActor->m_ActorLink.f_Unlink(); // Disregard concurrent actor from destroy
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
				m_Actors.f_Insert(*m_pConcurrentActor.m_pInternalActor);
				m_Actors.f_Insert(*m_pConcurrentActorLowPrio.m_pInternalActor);
				{
					DMibUnlock(m_ActorListLock);
					DMibLock(m_pConcurrentActorLock);
					m_pConcurrentActor->fp_Terminate();
					m_pConcurrentActor = nullptr;
					m_pConcurrentActorLowPrio->fp_Terminate();
					m_pConcurrentActorLowPrio= nullptr;
				}
				while (!m_Actors.f_IsEmpty())
				{
					DMibUnlock(m_ActorListLock);
					NSys::fg_Thread_SmallestSleep();
				}
			}
		}

		TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor()
		{
			{
				if (!m_pConcurrentActor)
				{
					DMibLock(m_pConcurrentActorLock);
					if (!m_pConcurrentActor)
						m_pConcurrentActor = fg_ConstructActor<CConcurrentActor>();
				}
				return m_pConcurrentActor;
			}
		}

		TCActor<CConcurrentActorLowPrio> const &CConcurrencyManager::f_GetConcurrentActorLowPrio()
		{
			{
				if (!m_pConcurrentActorLowPrio)
				{
					DMibLock(m_pConcurrentActorLock);
					if (!m_pConcurrentActorLowPrio)
						m_pConcurrentActorLowPrio = fg_ConstructActor<CConcurrentActorLowPrio>();
				}
				return m_pConcurrentActorLowPrio;
			}
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

		auto fg_DiscardResult() -> decltype(fg_ConcurrentActor() / NPrivate::CDiscardResultFunctor())
		{
			// For now just call on concurrent actor, but should be optimized not to call any actor
			return fg_ConcurrentActor() / NPrivate::CDiscardResultFunctor();
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

