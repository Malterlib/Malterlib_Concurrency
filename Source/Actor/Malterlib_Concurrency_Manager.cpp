// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	DMibImpErrorClassImplement(CExceptionActorDeleted);
	DMibImpErrorClassImplement(CExceptionActorAlreadyDestroyed);
	DMibImpErrorClassImplement(CExceptionActorIsBeingDestroyed);
	DMibImpErrorClassImplement(CExceptionActorResultWasNotSet);

	namespace NPrivate
	{
		struct CSubSystem_ConcurrencyInit : public CSubSystem
		{
			CSubSystem_ConcurrencyInit()
			{
				NMisc::fg_Random(); // Fix deadlock
			}
		};
		struct CSubSystem_Concurrency : public CSubSystem_ConcurrencyInit
		{
			NThread::CMutual m_ConcurrencyManagerLock;
			NStorage::TCUniquePointer<NConcurrency::CConcurrencyManager, NMemory::CAllocator_NonTrackedHeap> m_pConcurrencyManager;

			void f_PreDestroyThreadSpecific() override
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
					{
						m_pConcurrencyManager = fg_Construct(m_DefaultExecutionPriority);
						m_pConcurrencyManager->f_Init();
					}

					return *m_pConcurrencyManager;
				}
				return *m_pConcurrencyManager;
			}

			NThread::TCThreadLocal<CConcurrencyThreadLocal, NMemory::CAllocator_Heap, NThread::EThreadLocalFlag_AlwaysCreated> m_ThreadLocal;
			EExecutionPriority m_DefaultExecutionPriority[EPriority_Max] = {EExecutionPriority_Lowest, EExecutionPriority_Normal};
		};

		constinit TCSubSystem<CSubSystem_Concurrency, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Concurrency = {DAggregateInit};
	}

#if DMibEnableSafeCheck > 0
	bool fg_CurrentActorProcessingOrOverridden()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		auto pProcessingActorHolder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
		if (!pProcessingActorHolder)
			pProcessingActorHolder = ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder;

		if (!pProcessingActorHolder)
			return false;

		if (ThreadLocal.m_pCurrentActor)
			return ThreadLocal.m_pCurrentActor->self.m_pThis->f_IsProcessedOnActorHolder(pProcessingActorHolder);
		else
			return true; // Current actor is deduced from ThreadLocal.m_pCurrentlyProcessingActorHolder so by definition it's running
	}
#endif

	bool fg_CurrentActorProcessing()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (!ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return false;

		if (ThreadLocal.m_pCurrentActor)
			return ThreadLocal.m_pCurrentActor->self.m_pThis->f_IsProcessedOnActorHolder(ThreadLocal.m_pCurrentlyProcessingActorHolder);
		else
			return true; // Current actor is deduced from ThreadLocal.m_pCurrentlyProcessingActorHolder so by definition it's running
	}

	void fg_SetConcurrencyManagerDefaultExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority)
	{
		NPrivate::g_SubSystem_Concurrency->m_DefaultExecutionPriority[_Priority] = _ExecutionPriority;
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

	TCActor<CDirectCallActor> const &fg_DirectCallActor()
	{
		return fg_ConcurrencyManager().f_GetDirectCallActor();
	}

	TCActor<NPrivate::CDirectResultActor> const &NPrivate::fg_DirectResultActor()
	{
		return fg_ConcurrencyManager().f_GetDirectResultActor();
	}

	TCActor<CAnyConcurrentActor> const &fg_AnyConcurrentActor()
	{
		return fg_ConcurrencyManager().f_GetAnyConcurrentActor();
	}

	TCActor<CAnyConcurrentActorLowPrio> const &fg_AnyConcurrentActorLowPrio()
	{
		return fg_ConcurrencyManager().f_GetAnyConcurrentActorLowPrio();
	}

	TCActor<CDynamicConcurrentActor> const &fg_DynamicConcurrentActor()
	{
		return fg_ConcurrencyManager().f_GetDynamicConcurrentActor();
	}

	TCActor<CDynamicConcurrentActorLowPrio> const &fg_DynamicConcurrentActorLowPrio()
	{
		return fg_ConcurrencyManager().f_GetDynamicConcurrentActorLowPrio();
	}

	CConcurrencyThreadLocal::CConcurrencyThreadLocal()
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

	CConcurrencyThreadLocal::~CConcurrencyThreadLocal() = default;

	CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal()
	{
		return *NPrivate::g_SubSystem_Concurrency->m_ThreadLocal;
	}

	CAllowWrongThreadDestroy::~CAllowWrongThreadDestroy()
	{
#if DMibEnableSafeCheck > 0
		++fg_ConcurrencyThreadLocal().m_AllowWrongThreadDestroySequence;
#endif
	}

	CAllowWrongThreadDestroy g_AllowWrongThreadDestroy;

	///
	/// CConcurrencyManager
	/// ===================
//#define DMibNoConcurrency

	CConcurrencyManager::CConcurrencyManager(EExecutionPriority _ExecutionPriority[EPriority_Max])
	{

#ifdef DMibNoConcurrency
		mint nThreads = 1;
#else
		mint nThreads = NSys::fg_Thread_GetVirtualCores();
#endif
		m_nThreads = nThreads;
		for (EPriority Priority = EPriority_Low; Priority < EPriority_Max; Priority = static_cast<EPriority>(Priority + 1))
		{
			m_ExecutionPriority[Priority] = _ExecutionPriority[Priority];
			auto &Queues = m_Queues[Priority];
			Queues.f_SetLen(nThreads);

			auto &nActors = (m_nActorsPerQueue[Priority] = NContainer::TCVector<CNumActorsPerQueue>(nThreads));
			m_nActorsPerQueueArray[Priority] = nActors.f_GetArray();

			for (mint i = 0; i < nThreads; ++i)
			{
				auto *pQueue = &Queues[i];
				pQueue->m_iQueue = i;
				pQueue->m_Priority = Priority;
				if (Priority == EPriority_Normal)
					pQueue->f_Signal(this);
			}
		}

		m_ActorsOtherMask = fg_RoundPowerOfTwoUp(nThreads) - 1;
		m_nActorsOther = NContainer::TCVector<CNumActorsOther>(m_ActorsOtherMask + 1);

		m_DirectCallActor = f_ConstructActor(fg_Construct<CDirectCallActorImpl>());
		m_AnyConcurrentActor = f_ConstructActor(fg_Construct<CAnyConcurrentActorImpl>());
		m_AnyConcurrentActorLowPrio = f_ConstructActor(fg_Construct<CAnyConcurrentActorLowPrioImpl>());
		m_DynamicConcurrentActor = f_ConstructActor(fg_Construct<CDynamicConcurrentActorImpl>());
		m_DynamicConcurrentActorLowPrio = f_ConstructActor(fg_Construct<CDynamicConcurrentActorLowPrioImpl>());
		m_DirectResultActor = f_ConstructActor(fg_Construct<NPrivate::CDirectResultActorImpl>());

		m_DirectCallActorRef = m_DirectCallActor;
		m_AnyConcurrentActorRef = m_AnyConcurrentActor;
		m_AnyConcurrentActorLowPrioRef = m_AnyConcurrentActorLowPrio;
		m_DynamicConcurrentActorRef = m_DynamicConcurrentActor;
		m_DynamicConcurrentActorLowPrioRef = m_DynamicConcurrentActorLowPrio;
		m_DirectResultActorRef = m_DirectResultActor;
	}

#if DMibEnableSafeCheck > 0

	namespace
	{
		struct CCoroutineActor : public CActor
		{
			TCFuture<void> f_ReferenceCoroutine(uint32 const &_Reference)
			{
				co_return {};
			}
		};
	}

#endif

	void CConcurrencyManager::f_Init()
	{
#if DMibEnableSafeCheck > 0
		// Initialize callstack locations for valid reference coroutines

		TCActor<CCoroutineActor> CoroutineActor = fg_Construct();
		CoroutineActor(&CCoroutineActor::f_ReferenceCoroutine, 5).f_CallSync();

		(
			g_ConcurrentDispatch / []() -> TCFuture<uint32>
			{
				co_return 0;
			}
		).f_CallSync();
#endif
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
		}

		for (mint Prio = 0; Prio < EPriority_Max; ++Prio)
		{
			auto &Queue = m_Queues[Prio];
			for (auto &Queue : Queue)
			{
				if (Queue.m_pThread)
				{
					Queue.m_pThread->f_Stop(true);
					Queue.m_pThread.f_Clear();
				}
			}
		}

		// Delete the timer actor
		{
			DMibLock(m_TimerActorLock);
			if (m_pTimerActor)
				fg_Move(m_pTimerActor).f_Destroy() > fg_DiscardResult();
		}

		auto fProcessQueues = [&]()
			{
				bool bDoneSomething = true;
				while (bDoneSomething)
				{
					bDoneSomething = false;
					for (mint Prio = 0; Prio < EPriority_Max; ++Prio)
					{
						auto &Queue = m_Queues[Prio];
						for (auto &Queue : Queue)
						{
							Queue.m_JobQueue.f_TransferThreadSafeQueue(Queue.m_JobQueueLocal);
							if (!Queue.m_JobQueue.f_FirstQueueEntry(Queue.m_JobQueueLocal))
								continue;
							bDoneSomething = true;
							while (auto pEntry = Queue.m_JobQueue.f_FirstQueueEntry(Queue.m_JobQueueLocal))
							{
								(*pEntry)();
								Queue.m_JobQueue.f_PopQueueEntry(pEntry);
							}
						}
					}
				}
			}
		;

		fProcessQueues();

		// Finally delete the concurrent actor
		{
			DMibLock(m_pConcurrentActorLock);
			for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
			{
				for (auto &Actor : m_ConcurrentActors[Prio])
					Actor->fp_Terminate();
			}
			for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
				m_ConcurrentActors[Prio].f_Clear();

			m_nThreads = 0;
		}

		fProcessQueues();
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
				DMibCheck(Queue.m_JobQueue.f_IsEmpty(Queue.m_JobQueueLocal));
			}
		}
	}

	CConcurrencyManager::CQueue::CQueue(CQueue &&_Other)
	{
		DMibFastCheck(false);
	}

	CConcurrencyManager::CQueue::CQueue()
	{
	}

	inline_never void CConcurrencyManager::CQueue::fp_CreateThread(CConcurrencyManager *_pThis)
	{
		DMibLock(_pThis->m_ThreadCreateLock);
		if (m_pThread)
			return;

		NStr::CStrNonTracked Name;
		EExecutionPriority ThreadPrio = _pThis->m_ExecutionPriority[m_Priority];
		switch (m_Priority)
		{
		case EPriority_Low:
			Name = "(Low)";
			break;
		case EPriority_Normal:
			break;
		case EPriority_Max:
			DMibFastCheck(false);
			break;
		}

		m_pThread =
			(
				NThread::CThreadObjectNonTracked::fs_StartThread
				(
					[_pThis, this](NThread::CThreadObjectNonTracked *_pThread) -> aint
					{
						_pThis->fp_RunThread(*this, _pThread);
						return 0;
					}
					, NStr::CStrNonTracked::CFormat("ActorPool{} {}") << Name << m_iQueue
					, ThreadPrio
				)
			)
		;
	}

	inline_always void CConcurrencyManager::CQueue::f_Signal(CConcurrencyManager *_pThis)
	{
		m_Event.f_Signal();
		if (!m_pThread)
			fp_CreateThread(_pThis);
	}

	void CConcurrencyManager::f_SetExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority)
	{
		m_ExecutionPriority[_Priority] = _ExecutionPriority;
		for (auto &Queue : m_Queues[_Priority])
		{
			if (Queue.m_pThread)
				Queue.m_pThread->f_SetPriority(_ExecutionPriority);
		}
	}

	EExecutionPriority CConcurrencyManager::f_GetExecutionPriority(EPriority _Priority)
	{
		return m_ExecutionPriority[_Priority];
	}

	void CConcurrencyManager::f_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatch &&_ToQueue)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue)
		{
			ThreadLocal.m_pThisQueue->m_JobQueue.f_AddToQueueLocalFirst(fg_Move(_ToQueue), ThreadLocal.m_pThisQueue->m_JobQueueLocal);
			return;
		}

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		if (ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
			ThreadLocal.m_iConcurrentActor[_Priority] = 0;
		auto &Queue = m_Queues[_Priority].f_GetArray()[ThreadLocal.m_iConcurrentActor[_Priority]];
		++ThreadLocal.m_iConcurrentActor[_Priority];

		if (fp_AddToQueue(Queue, fg_Move(_ToQueue), false))
			Queue.f_Signal(this);
	}

	void CConcurrencyManager::f_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatch &&_ToQueue)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue)
		{
			ThreadLocal.m_pThisQueue->m_JobQueue.f_AddToQueueLocal(fg_Move(_ToQueue), ThreadLocal.m_pThisQueue->m_JobQueueLocal);
			return;
		}

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		if (ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
			ThreadLocal.m_iConcurrentActor[_Priority] = 0;
		auto &Queue = m_Queues[_Priority].f_GetArray()[ThreadLocal.m_iConcurrentActor[_Priority]];
		++ThreadLocal.m_iConcurrentActor[_Priority];

		if (fp_AddToQueue(Queue, fg_Move(_ToQueue), false))
			Queue.f_Signal(this);
	}

	void CConcurrencyManager::fp_QueueJob(EPriority _Priority, mint _iFixedCore, FActorQueueDispatch &&_ToQueue, bool _bForceNonLocal)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mint iJobQueue;
		if (_iFixedCore < m_nThreads)
			iJobQueue = _iFixedCore;
		else
		{
			do
			{
				mint iThisQueue = TCLimitsInt<mint>::mc_Max;
				if (ThreadLocal.m_pThisQueue)
					iThisQueue = ThreadLocal.m_pThisQueue->m_iQueue;
				if (ThreadLocal.m_pCurrentActor)
				{
					mint iCurrentCore = ThreadLocal.m_pCurrentActor->self.m_pThis->mp_iFixedCore;
					if (iCurrentCore < m_nThreads)
					{
						iJobQueue = iCurrentCore;
						break;
					}
				}
				auto &iJobQueueNew = ThreadLocal.m_JobQueueIndex[_Priority];
				if (iJobQueueNew == iThisQueue) // Don't queue new actors on the same queue
					++iJobQueueNew;
				if (iJobQueueNew >= m_nThreads)
					iJobQueueNew = 0;
				iJobQueue = iJobQueueNew;
				++iJobQueueNew;
			}
			while (false)
				;
		}

		auto &Queue = m_Queues[_Priority].f_GetArray()[iJobQueue];
		if (fp_AddToQueue(Queue, fg_Move(_ToQueue), _bForceNonLocal))
			Queue.f_Signal(this);
	}

	constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);

	bool CConcurrencyManager::fp_AddToQueue(CQueue &_Queue, FActorQueueDispatch &&_Functor, bool _bForceNonLocal)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		if (ThreadLocal.m_pThisQueue == &_Queue && !_bForceNonLocal)
		{
			_Queue.m_JobQueue.f_AddToQueueLocal(fg_Move(_Functor), _Queue.m_JobQueueLocal);
			return false;
		}
		_Queue.m_JobQueue.f_AddToQueue(fg_Move(_Functor));

		mint Value = _Queue.m_Working.f_FetchAdd(1);
		return Value == 0;
	}

	void CConcurrencyManager::fp_RunThread(CQueue &_Queue, NThread::CThreadObjectNonTracked *_pThread)
	{
#if DMibPPtrBits > 32
		{
			auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
			Checkout.f_TakeOwnership(); // Make each thread own it's memory manager
		}
#endif
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		ThreadLocal.m_pThisQueue = &_Queue;
#if DMibPPtrBits > 32
		auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#endif
		while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
		{
			NTime::CCyclesClock Clock;
			Clock.f_Start();
			while (true)
			{
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
							if (_Queue.m_JobQueue.f_TransferThreadSafeQueue(_Queue.m_JobQueueLocal))
								bDoneSomething = true;
							while (auto *pJob = _Queue.m_JobQueue.f_FirstQueueEntry(_Queue.m_JobQueueLocal))
							{
#if DMibPPtrBits > 32
								if (!Checkout.f_IsCheckedOut())
									Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#endif

								(*pJob)();
								_Queue.m_JobQueue.f_PopQueueEntry(pJob);
								bDoneSomething = true;
							}
						}
					}
				}

#if DMibPPtrBits > 32
				Checkout = NMemory::CMemoryManagerCheckout(nullptr);
#endif
				if (Clock.f_GetTime() > 0.000'035) // Loop for at least 35 µs before going to kernel
					break;
			}
			_Queue.m_Event.f_Wait();
		}
	}

	void CConcurrencyManager::fp_AddedActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue)
		{
			auto &Queue = *ThreadLocal.m_pThisQueue;
			m_nActorsPerQueueArray[Queue.m_Priority][Queue.m_iQueue].m_nActors.f_FetchAdd(1, NAtomic::EMemoryOrder_Relaxed);
			return;
		}

		m_nActorsOther[(ThreadLocal.m_nActorsIndex & m_ActorsOtherMask)].m_nActors.f_FetchAdd(1, NAtomic::EMemoryOrder_Relaxed);
	}

	void CConcurrencyManager::fp_RemovedActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue)
		{
			auto &Queue = *ThreadLocal.m_pThisQueue;
			m_nActorsPerQueueArray[Queue.m_Priority][Queue.m_iQueue].m_nActors.f_FetchSub(1, NAtomic::EMemoryOrder_Release);
			return;
		}

		m_nActorsOther[(ThreadLocal.m_nActorsIndex & m_ActorsOtherMask)].m_nActors.f_FetchSub(1, NAtomic::EMemoryOrder_Release);
	}

	mint CConcurrencyManager::fp_NumActors()
	{
		smint nActorsSum = 0;
		for (auto &nActors : m_nActorsOther)
			nActorsSum += nActors.m_nActors.f_Load();

		for (auto &nActorsPerQueue : m_nActorsPerQueue)
		{
			for (auto &nActors : nActorsPerQueue)
				nActorsSum += nActors.m_nActors.f_Load();
		}

		return nActorsSum;
	}

	void CConcurrencyManager::f_BlockOnDestroy()
	{
		if (m_bDestroyed)
			return;
		m_bDestroyed = true;
		fp_InitConcurrentActors(); // Make sure concurrent actors are created
		auto &TimerActor = f_GetTimerActor();
		NTime::CClock Clock{true};
		NTime::CClock TimerClock{true};

		mint nDirectDeleteActors = 6;

#if DMibConfig_Concurrency_DebugBlockDestroy
		bool bAborted = false;
#endif
		{
			// Disregard concurrent actors, timer actor and direct delete actors from destroy
			mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen() + m_ConcurrentActors[EPriority_Low].f_GetLen() + 1 + nDirectDeleteActors;
#if DMibConfig_Concurrency_DebugBlockDestroy
			volatile static bool s_AbortLoop = false;
#endif
			TimerActor(&CTimerActor::f_FireAtExit).f_CallSync();

			while (fp_NumActors() > nExpectedActors)
			{
				if (TimerClock.f_GetTime() > 0.010)
				{
					if (Clock.f_GetTime() > 10.0)
						TimerActor(&CTimerActor::f_FireAllTimeouts).f_CallSync();
					else
					{
						TimerActor(&CTimerActor::f_FireAtExit).f_CallSync();
					}
					TimerClock.f_Start();
				}

				NSys::fg_Thread_SmallestSleep();
#if DMibConfig_Concurrency_DebugBlockDestroy
				if (Clock.f_GetTime() > 10.0)
				{
					DMibTrace("Shutting down of actors is taking a long time. Waiting for actors:{\n}", 0);
					{
						DMibLock(m_ActorListLock);
						for (auto &Actor : this->m_Actors)
						{
							if
								(
									Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CConcurrentActorImpl") >= 0
								 	|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CTimerActor") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDirectCallActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CAnyConcurrentActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CAnyConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDynamicConcurrentActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDynamicConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::NPrivate::CDirectResultActorImpl") >= 0
								)
							{
								continue;
							}

							DMibTrace
								(
								 	"    {}   Destroyed {}   RefCount {}   WeakCount {}{\n}"
								 	, Actor.m_ActorTypeName << Actor.mp_bDestroyed.f_Load() << Actor.f_RefCountGet() << Actor.f_WeakRefCountGet()
								)
							;

	#if DMibConfig_RefcountDebugging
							mint iCallstack = 0;
							for (auto &Callstack : Actor.m_Debug->m_Callstacks)
							{
								DMibTrace2("        Reference callstack {}\n", iCallstack);
								Callstack.f_Trace(12);
								++iCallstack;
							}
							iCallstack = 0;
							for (auto &Callstack : Actor.m_Debug->m_WeakCallstacks)
							{
								DMibTrace2("        Weak reference callstack {}\n", iCallstack);
								Callstack.f_Trace(12);
								++iCallstack;
							}
	#endif
						}
					}

#if DMibConfig_Concurrency_DebugFutures
					{
						DMibTrace("Futures alive:{\n}", 0);
						DMibLock(m_FutureListLock);
						for (auto &Future : this->m_Futures)
						{
							DMibTrace("    TCFuture<{}>   RefCount {}{\n}", Future.m_FutureTypeName << Future.f_RefCountGet());

	#if DMibConfig_RefcountDebugging
							mint iCallstack = 0;
							for (auto &Callstack : Future.m_Debug->m_Callstacks)
							{
								DMibTrace2("        Reference callstack {}\n", iCallstack);
								Callstack.f_Trace(12);
								++iCallstack;
							}
	#endif
						}
					}
#endif
					Clock.f_Start();
				}
				if (s_AbortLoop)
				{
					bAborted = true;
					break;
				}
#endif
			}
		}

#if DMibConfig_Concurrency_DebugBlockDestroy
		if (bAborted)
			return;
#endif

		auto fWaitForAllDone = [&]()
			{
				for (bool bAllDone = false; !bAllDone;)
				{
					TCActorResultVector<bool> ConcurrentActorSyncs;
					{
						DMibLock(m_TimerActorLock);
						if (m_pTimerActor)
						{
							g_Dispatch(m_pTimerActor) / []() -> bool
								{
									auto pInternalActor = fg_GetActorInternal(fg_CurrentActor());
									return pInternalActor->mp_ConcurrentRunQueue.f_OneOrLessInQueue(pInternalActor->mp_ConcurrentRunQueueLocal);
								}
								> ConcurrentActorSyncs.f_AddResult()
							;
						}
					}

					for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
					{
						for (auto &Actor : m_ConcurrentActors[Prio])
						{
							g_Dispatch(Actor) / []() -> bool
								{
									auto pInternalActor = fg_GetActorInternal(fg_CurrentActor());
									return pInternalActor->mp_ConcurrentRunQueue.f_OneOrLessInQueue(pInternalActor->mp_ConcurrentRunQueueLocal);
								}
								> ConcurrentActorSyncs.f_AddResult()
							;
						}
					}

					auto Done = ConcurrentActorSyncs.f_GetResults().f_CallSync();
					bAllDone = true;
					for (auto &bDone : Done)
					{
						if (!bDone)
						{
							bAllDone = false;
							break;
						}
					}
				}
			}
		;

		fWaitForAllDone();

		// Delete the timer actor
		{
			{
				DMibLock(m_TimerActorLock);
				if (m_pTimerActor)
					fg_Move(m_pTimerActor).f_Destroy().f_CallSync();
			}
			mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen() + m_ConcurrentActors[EPriority_Low].f_GetLen() + nDirectDeleteActors;
			while (fp_NumActors() > nExpectedActors)
				NSys::fg_Thread_SmallestSleep();
		}

		// Sync up to make sure nothing is still waiting to process on actors
		fWaitForAllDone();

		m_bDestroyingAlwaysAliveActors = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bDestroyingAlwaysAliveActors = false;
			}
		;

		// Delete the concurrent actors
		{
			TCActorResultVector<void> Destroys;
			{
				DMibLock(m_pConcurrentActorLock);
				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
				{
					for (auto &Actor : m_ConcurrentActors[Prio])
						fg_Move(Actor).f_Destroy() > Destroys.f_AddResult();
				}
				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
					m_ConcurrentActors[Prio].f_Clear();

				m_nThreads = 0;
			}
			Destroys.f_GetResults().f_CallSync();
			while (fp_NumActors() > nDirectDeleteActors)
				NSys::fg_Thread_SmallestSleep();
		}

		// Finally delete the direct call actor

		{
			m_AnyConcurrentActor->fp_Terminate();
			m_AnyConcurrentActor.f_Clear();
			m_AnyConcurrentActorLowPrio->fp_Terminate();
			m_AnyConcurrentActorLowPrio.f_Clear();
			m_DynamicConcurrentActor->fp_Terminate();
			m_DynamicConcurrentActor.f_Clear();
			m_DynamicConcurrentActorLowPrio->fp_Terminate();
			m_DynamicConcurrentActorLowPrio.f_Clear();
			m_DirectCallActor->fp_Terminate();
			m_DirectCallActor.f_Clear();
			m_DirectResultActor->fp_Terminate();
			m_DirectResultActor.f_Clear();
		}

		while (fp_NumActors() > 0)
			NSys::fg_Thread_SmallestSleep();
	}

	bool CConcurrencyManager::f_DestroyingAlwaysAliveActors() const
	{
		return m_bDestroyingAlwaysAliveActors.f_Load();
	}

	inline_never mint CConcurrencyManager::fp_InitConcurrentActors()
	{
		DMibLock(m_pConcurrentActorLock);
		mint nActors = m_nConcurrentActors.f_Load();
		if (!nActors)
		{
			nActors = m_nThreads;

			for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
			{
				m_ConcurrentActors[Prio].f_SetLen(nActors);
				m_ConcurrentActorsRef[Prio].f_SetLen(nActors);
			}

			for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
			{
				mint iActor = 0;
				for (auto &Actor : m_ConcurrentActors[Prio])
				{
					if (Prio == EPriority_Normal)
						Actor = f_ConstructActor(fg_Construct<CConcurrentActorImpl>());
					else if (Prio == EPriority_Low)
						Actor = f_ConstructActor(fg_Construct<CConcurrentActorLowPrioImpl>());
					Actor->f_SetFixedCore(iActor);
					m_ConcurrentActorsRef[Prio][iActor] = Actor;
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

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue && ThreadLocal.m_pThisQueue->m_Priority == _Priority)
		{
			auto &ToReturn = m_ConcurrentActorsRef[_Priority].f_GetArray()[ThreadLocal.m_pThisQueue->m_iQueue];
			return ToReturn;
		}
		if (ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
			ThreadLocal.m_iConcurrentActor[_Priority] = 0;
		auto &ToReturn = m_ConcurrentActorsRef[_Priority].f_GetArray()[ThreadLocal.m_iConcurrentActor[_Priority]];
		++ThreadLocal.m_iConcurrentActor[_Priority];
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor(TCWeakActor<CActor> const &_Actor)
	{
		// Returns a actor that is consistent based on weak pointer

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		mint HashData = reinterpret_cast<mint>(static_cast<void const *>(_Actor.fp_Get()));

		mint iActor = fg_JenkinsHash((ch8 const *)&HashData, sizeof(HashData), 0) % nActors;

		auto &ToReturn = m_ConcurrentActorsRef[EPriority_Normal].f_GetArray()[iActor];
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor()
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_iConcurrentActor[EPriority_Normal] >= nActors)
			ThreadLocal.m_iConcurrentActor[EPriority_Normal] = 0;
		auto &ToReturn = m_ConcurrentActorsRef[EPriority_Normal].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_Normal]];
		++ThreadLocal.m_iConcurrentActor[EPriority_Normal];
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorLowPrio()
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_iConcurrentActor[EPriority_Low] >= nActors)
			ThreadLocal.m_iConcurrentActor[EPriority_Low] = 0;
		auto &ToReturn = m_ConcurrentActorsRef[EPriority_Low].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_Low]];
		++ThreadLocal.m_iConcurrentActor[EPriority_Low];
		return ToReturn;
	}

	TCActor<CDirectCallActor> const &CConcurrencyManager::f_GetDirectCallActor()
	{
		return m_DirectCallActorRef;
	}

	TCActor<CAnyConcurrentActor> const &CConcurrencyManager::f_GetAnyConcurrentActor()
	{
		return m_AnyConcurrentActorRef;
	}

	TCActor<CAnyConcurrentActorLowPrio> const &CConcurrencyManager::f_GetAnyConcurrentActorLowPrio()
	{
		return m_AnyConcurrentActorLowPrioRef;
	}

	TCActor<NPrivate::CDirectResultActor> const &CConcurrencyManager::f_GetDirectResultActor()
	{
		return m_DirectResultActorRef;
	}

	TCActor<CDynamicConcurrentActor> const &CConcurrencyManager::f_GetDynamicConcurrentActor()
	{
		return m_DynamicConcurrentActorRef;
	}

	TCActor<CDynamicConcurrentActorLowPrio> const &CConcurrencyManager::f_GetDynamicConcurrentActorLowPrio()
	{
		return m_DynamicConcurrentActorLowPrioRef;
	}

	TCActor<CTimerActor> const &CConcurrencyManager::f_GetTimerActor()
	{
		if (!m_bTimerActorInit.f_Load(NAtomic::EMemoryOrder_Acquire))
		{
			DMibLock(m_TimerActorLock);
			if (!m_bTimerActorInit.f_Load())
			{
				m_pTimerActor = fg_Construct(fg_Construct(), "Timer actor");
				m_bTimerActorInit.f_Store(true);
			}
		}
		return m_pTimerActor;
	}

	TCActor<CActor> fg_CurrentActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pCurrentActor)
		{
			DMibFastCheck(fg_ThisActor(ThreadLocal.m_pCurrentActor) != fg_DirectCallActor()); // It's not safe to use the direct call actor
			return fg_ThisActor(ThreadLocal.m_pCurrentActor);
		}

		if (!ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return {};

		TCActor<> Actor(fg_Explicit(static_cast<TCActorInternal<CActor> *>(ThreadLocal.m_pCurrentlyProcessingActorHolder)));

		DMibFastCheck(Actor != fg_DirectCallActor()); // It's not safe to use the direct call actor

		return Actor;
	}

	TCWeakActor<CActor> fg_CurrentActorWeak()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pCurrentActor)
		{
			DMibFastCheck(fg_ThisActor(ThreadLocal.m_pCurrentActor) != fg_DirectCallActor()); // It's not safe to use the direct call actor
			return fg_ThisActorWeak(ThreadLocal.m_pCurrentActor);
		}

		if (!ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return {};

		TCWeakActor<> Actor(TCActorHolderWeakPointer<TCActorInternal<CActor>>(static_cast<TCActorInternal<CActor> *>(ThreadLocal.m_pCurrentlyProcessingActorHolder)));

		DMibFastCheck(Actor != fg_DirectCallActor()); // It's not safe to use the direct call actor

		return Actor;
	}

	TCFuture<void> fg_DestroySubscription(CActorSubscription &_Subscription)
	{
		if (!_Subscription)
			return TCPromise<void>() <<= g_Void;

		TCFuture<void> Future = _Subscription->f_Destroy();
		_Subscription.f_Clear();

		return Future;
	}

	CConcurrencyManager &fg_CurrentConcurrencyManager()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pCurrentActor)
			return ThreadLocal.m_pCurrentActor->f_ConcurrencyManager();
		if (ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return ThreadLocal.m_pCurrentlyProcessingActorHolder->f_ConcurrencyManager();
		return fg_ConcurrencyManager();
	}

	auto fg_DiscardResult() -> decltype(NConcurrency::TCActor<NConcurrency::CDirectCallActor>() / NPrivate::CDiscardResultFunctor())
	{
		return NConcurrency::TCActor<NConcurrency::CDirectCallActor>() / NPrivate::CDiscardResultFunctor();
	}

#if DMibConfig_Concurrency_DebugActorCallstacks
	namespace NPrivate
	{
		CAsyncCallstacks *fg_SetConcurrentCallstacks(CAsyncCallstacks *_pCallstacks, CAsyncCallstacks *_pPredicate)
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			auto pOld = ThreadLocal.m_pCallstacks;
			if (!_pPredicate || pOld == _pPredicate)
				ThreadLocal.m_pCallstacks = _pCallstacks;
			return pOld;
		}
	}
#endif

	constexpr COnScopeExitActorHelper g_OnScopeExitActorInit{};
	COnScopeExitActorHelper const &g_OnScopeExitActor = g_OnScopeExitActorInit;

	CActorSubscriptionReference::~CActorSubscriptionReference() = default;

	TCFuture<void> CActorSubscriptionReference::f_Destroy()
	{
		co_return {};
	}
}

namespace NMib
{
	template class TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>;
}
