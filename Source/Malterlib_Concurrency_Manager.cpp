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

		NConcurrency::TCActor<NConcurrency::CConcurrentActor> const &fg_ConcurrentActor()
		{
			return fg_ConcurrencyManager().f_GetConcurrentActor();
		}
		
		NConcurrency::TCActor<NConcurrency::CConcurrentActor> const &fg_ConcurrentActorLowPrio()
		{
			return fg_ConcurrencyManager().f_GetConcurrentActorLowPrio();
		}
		
		///
		/// CConcurrencyManager
		/// ===================
//#define DMibNoConcurrency

		CConcurrencyManager::CConcurrencyManager()
		{
#ifdef DMibNoConcurrency
			mint nThreads = 1;
#else
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
#endif
			m_nThreads = nThreads;
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
				auto &Queues = m_Queues[Priority];
				Queues.f_SetLen(nThreads);
				for (mint i = 0; i < nThreads; ++i)
				{
					auto *pQueue = &Queues[i];
					Queues[i].m_pThread =
						(
							NThread::CThreadObjectNonTracked::fs_StartThread
							(
								[this, pQueue](NThread::CThreadObjectNonTracked *_pThread) -> aint
								{
									fp_RunThread(*pQueue, _pThread);
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
				auto &Queue = m_Queues[Prio];
				for (auto &Queue : Queue)
				{
					if (Queue.m_pThread)
					{
						Queue.m_pThread->f_Stop(false);
						Queue.m_Event.f_Signal();
					}
				}

				for (auto &Queue : Queue)
					Queue.m_pThread.f_Clear();

				for (auto &Queue : Queue)
				{
					while (true)
					{
						Queue.m_JobQueue.f_TransferThreadSafeQueue();
						if (!Queue.m_JobQueue.f_FirstQueueEntry())
							break;
						
						while (auto pEntry = Queue.m_JobQueue.f_FirstQueueEntry())
						{
							(*pEntry)();
							
							Queue.m_JobQueue.f_PopQueueEntry(pEntry);
						}
					}
				}
			}
		}

		CConcurrencyManager::~CConcurrencyManager()
		{
			f_Stop();
			for (mint i = 0; i < EPriority_Max; ++i)
			{
				for (auto &Queue : m_Queues[i])
				{
					(void)Queue;
					DMibCheck(Queue.m_pThread.f_IsEmpty());
					DMibCheck(Queue.m_JobQueue.f_IsEmpty());
				}
			}
#ifdef DMibDebug
			DMibCheck(m_Actors.f_IsEmpty());
#endif
		}

		CConcurrencyManager::CQueue::CQueue(CQueue &&_Other)
		{
			DMibFastCheck(false);
		}
		
		CConcurrencyManager::CQueue::CQueue()
		{
		}
		
		void CConcurrencyManager::f_DispatchFirstOnCurrentThread(EPriority _Priority, NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_ToQueue)
		{
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_pThisQueue)
			{
				ThreadLocal.m_pThisQueue->m_JobQueue.f_AddToQueueLocal(fg_Move(_ToQueue));
				return;
			}

			auto &Queue = m_Queues[_Priority][0]; // Otherwise dipsatch on first queue, rely on work stealing to distribute
			if (fp_AddToQueue(Queue, fg_Move(_ToQueue)))
				Queue.m_Event.f_Signal();
		}
		
		void CConcurrencyManager::f_DispatchOnNextThread(EPriority _Priority, NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_ToQueue)
		{
			auto &ThreadLocal = *m_ThreadLocal;
			mint iQueue = 0;
			if (ThreadLocal.m_pThisQueue)
			{
				DMibCheck(ThreadLocal.m_pThisQueue >= &m_Queues[_Priority].f_GetFirst() && ThreadLocal.m_pThisQueue <= &m_Queues[_Priority].f_GetLast());
				iQueue = (ThreadLocal.m_pThisQueue - &m_Queues[_Priority].f_GetFirst()) + 1;
				if (iQueue >= m_nThreads)
					iQueue = 0;
			}

			auto &Queue = m_Queues[_Priority][iQueue];
			if (fp_AddToQueue(Queue, fg_Move(_ToQueue)))
				Queue.m_Event.f_Signal();
		}
		
		void CConcurrencyManager::fp_QueueJob(EPriority _Priority, mint _iFixedCore, NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_ToQueue)
		{
			auto &ThreadLocal = *m_ThreadLocal;
			mint iJobQueue;
			if (_iFixedCore < m_nThreads)
				iJobQueue = _iFixedCore;
			else
			{
				do
				{
					if (ThreadLocal.m_pCurrentActor)
					{
						mint iCurrentCore = ((TCActorInternal<CActor> *)ThreadLocal.m_pCurrentActor->self.m_pThis)->mp_iFixedCore;
						if (iCurrentCore < m_nThreads)
						{
							iJobQueue = iCurrentCore;
							break;
						}
					}
					auto &iJobQueueNew = ThreadLocal.m_JobQueueIndex[_Priority];
					if (iJobQueueNew >= m_nThreads)
						iJobQueueNew = 0;
					iJobQueue = iJobQueueNew;
					++iJobQueueNew;
				}
				while (false)
					;
			}
			
			auto &Queue = m_Queues[_Priority][iJobQueue];
			if (fp_AddToQueue(Queue, fg_Move(_ToQueue)))
				Queue.m_Event.f_Signal();
		}
		
		constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);

		bool CConcurrencyManager::fp_AddToQueue(CQueue &_Queue, NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			auto &ThreadLocal = *m_ThreadLocal;
			
			if (ThreadLocal.m_pThisQueue == &_Queue)
			{
				_Queue.m_JobQueue.f_AddToQueueLocal(fg_Move(_Functor));
				return false;
			}
			_Queue.m_JobQueue.f_AddToQueue(fg_Move(_Functor));
			
			mint Value = _Queue.m_Working.f_FetchAdd(1);
			return Value == 0;
		}
		
		void CConcurrencyManager::fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread)
		{
			auto &ThreadLocal = *m_ThreadLocal;
			ThreadLocal.m_pThisQueue = &_Queue;
			while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
			{
				NTime::CCyclesClock Clock;
				Clock.f_Start();
				while (true)
				{
#if DMibPPtrBits > 32
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#endif

					bool bDoMore = true;
					while (bDoMore)
					{
						bDoMore = false;
						mint OriginalWorking = _Queue.m_Working.f_FetchOr(gc_ProcessingMask);
						if ((OriginalWorking & gc_ProcessingMask) == 0)
						{
							auto UnLock 
								= fg_OnScopeExit
								(
									[&]
									{
										mint NewWorking = _Queue.m_Working.f_Exchange(0);
										if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
											bDoMore = true;
									}
								)
							;

							bool bDoneSomething = true;
							while (bDoneSomething)
							{
								bDoneSomething = false;
								if (_Queue.m_JobQueue.f_TransferThreadSafeQueue())
									bDoneSomething = true;
								while (auto *pJob = _Queue.m_JobQueue.f_FirstQueueEntry())
								{
									(*pJob)();
									_Queue.m_JobQueue.f_PopQueueEntry(pJob);
									bDoneSomething = true;
								}
							}
						}
					}
					if (Clock.f_GetTime() > 0.000035) // Loop for at least 35 µs before going to kernel
						break;
				}
				_Queue.m_Event.f_Wait();
			}
		}


		void CConcurrencyManager::f_BlockOnDestroy()
		{
			if (m_bDestroyed)
				return;
			m_bDestroyed = true;
			fp_InitConcurrentActors(); // Make sure concurrent actors are created
			f_GetTimerActor(); // Make sure timer actor is created
			{
#ifdef DMibDebug
				NTime::CCyclesClock Clock;
				Clock.f_Start();
#endif
				// Disregard concurrent actor from destroy
				mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen() + m_ConcurrentActors[EPriority_Low].f_GetLen() + 1;
				while (m_nActors.f_Load() > nExpectedActors)
				{
					NSys::fg_Thread_SmallestSleep();
					
#ifdef DMibDebug
					if (Clock.f_GetTime() > 10.0)
					{
						DMibDTrace("Shutting down of actors is taking a long time. Waiting for actors:{\n}", 0);
						DMibLock(m_ActorListLock);
						for (auto &Actor : this->m_Actors)
						{
							if (Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CConcurrentActor") >= 0 || Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CTimerActor") >= 0)
								continue;
							DMibConOut("\t{}   RefCount {}   WeakCount {}{\n}", Actor.m_ActorTypeName << Actor.f_RefCountGet() << Actor.f_WeakRefCountGet());
						}
						Clock.f_Start();
					}
#endif
				}
			}

			// Delete the timer actor
			{
				{
					DMibLock(m_pTimerActorLock);
					if (m_pTimerActor)
					{
						m_pTimerActor->f_Destroy();
						m_pTimerActor = nullptr;
					}
				}
				mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen() + m_ConcurrentActors[EPriority_Low].f_GetLen();
				while (m_nActors.f_Load() > nExpectedActors)
					NSys::fg_Thread_SmallestSleep();
			}
			
			// Finally delete the concurrent actor
			{
				{
					DMibLock(m_pConcurrentActorLock);
					for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
					{
						for (auto &Actor : m_ConcurrentActors[Prio])
							Actor->fp_Terminate(nullptr);
					}
					for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
						m_ConcurrentActors[Prio].f_Clear();
					
					m_nThreads = 0;
				}
				while (m_nActors.f_Load() > 0)
					NSys::fg_Thread_SmallestSleep();
			}
		}

		inline_never mint CConcurrencyManager::fp_InitConcurrentActors()
		{
			DMibLock(m_pConcurrentActorLock);
			mint nActors = m_nConcurrentActors.f_Load();
			if (!nActors)
			{
				nActors = m_nThreads;
				
				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
					m_ConcurrentActors[Prio].f_SetLen(nActors);
				
				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
				{
					mint iActor = 0;
					for (auto &Actor : m_ConcurrentActors[Prio])
					{
						if (Prio == EPriority_Normal)
							Actor = fg_ConstructActor<CConcurrentActor>();
						else if (Prio == EPriority_Low)
							Actor = fg_ConstructActor<CConcurrentActorLowPrio>();
						Actor->f_SetFixedCore(iActor);
						++iActor;
					}
				}
				
				m_nConcurrentActors = nActors;
			}
			return nActors;
		}
		
		TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorForThisThread(EPriority _Priority)
		{
			mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (!nActors)
				nActors = fp_InitConcurrentActors();
			
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_pThisQueue && ThreadLocal.m_pThisQueue >= &m_Queues[_Priority].f_GetFirst() && ThreadLocal.m_pThisQueue < &m_Queues[_Priority].f_GetLast())
			{
				mint iQueue = ThreadLocal.m_pThisQueue - &m_Queues[_Priority].f_GetFirst();
				auto &ToReturn = m_ConcurrentActors[_Priority].f_GetArray()[iQueue];
				return ToReturn;
			}
			if (ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
				ThreadLocal.m_iConcurrentActor[_Priority] = 0;
			auto &ToReturn = m_ConcurrentActors[_Priority].f_GetArray()[ThreadLocal.m_iConcurrentActor[_Priority]];
			++ThreadLocal.m_iConcurrentActor[_Priority];
			return ToReturn;
		}

		TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor()
		{
			mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (!nActors)
				nActors = fp_InitConcurrentActors();
			
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_iConcurrentActor[EPriority_Normal] >= nActors)
				ThreadLocal.m_iConcurrentActor[EPriority_Normal] = 0;
			auto &ToReturn = m_ConcurrentActors[EPriority_Normal].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_Normal]];
			++ThreadLocal.m_iConcurrentActor[EPriority_Normal];
			return ToReturn;
		}

		TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorLowPrio()
		{
			mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
			if (!nActors)
				nActors = fp_InitConcurrentActors();
			
			auto &ThreadLocal = *m_ThreadLocal;
			if (ThreadLocal.m_iConcurrentActor[EPriority_Low] >= nActors)
				ThreadLocal.m_iConcurrentActor[EPriority_Low] = 0;
			auto &ToReturn = m_ConcurrentActors[EPriority_Low].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_Low]];
			++ThreadLocal.m_iConcurrentActor[EPriority_Low];
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
		
		TCActor<CActor> CConcurrencyManager::f_CurrentActor()
		{
			auto &ThreadLocal = *m_ThreadLocal;
			if (!ThreadLocal.m_pCurrentActor)
				return fg_Default();
			return fg_ThisActor(ThreadLocal.m_pCurrentActor);
		}

		TCActor<CActor> fg_CurrentActor()
		{
			return fg_ConcurrencyManager().f_CurrentActor();
		}

		auto fg_DiscardResult() -> decltype(NConcurrency::TCActor<NConcurrency::CConcurrentActor>() / NPrivate::CDiscardResultFunctor())
		{
			return NConcurrency::TCActor<NConcurrency::CConcurrentActor>() / NPrivate::CDiscardResultFunctor();
		}
		
#if DMibConcurrencyDebugActorCallstacks
		namespace NPrivate
		{
			CAsyncCallstacks *fg_SetConcurrentCallstacks(CConcurrencyManager &_ConcurrencyManager, CAsyncCallstacks *_pCallstacks)
			{
				auto &ThreadLocal = *_ConcurrencyManager.m_ThreadLocal;
				auto pOld = ThreadLocal.m_pCallstacks;
				ThreadLocal.m_pCallstacks = _pCallstacks;
				return pOld;
			}
		}
#endif
	}
}

