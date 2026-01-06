// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/LogError>

//#define DDoWorkPolling

namespace NMib
{
	NConcurrency::CConcurrencyThreadLocal &CPromiseThreadLocal::f_ConcurrencyThreadLocal()
	{
		if (!m_pConcurrencyThreadLocal) [[unlikely]]
			m_pConcurrencyThreadLocal = &NConcurrency::fg_ConcurrencyThreadLocal();

		return *m_pConcurrencyThreadLocal;
	}

#if DMibEnableSafeCheck > 0
	CPromiseThreadLocal::~CPromiseThreadLocal()
	{
		DMibFastCheck(!m_pCoroutinePromiseAllocation);
		DMibFastCheck(m_PreviousCoroutinePromiseAllocations.f_IsEmpty());
	}
#endif

	inline_never void CPromiseThreadLocal::fp_PushAllocationSlowPath(void *_pAllocation)
	{
		m_PreviousCoroutinePromiseAllocations.f_InsertLast(m_pCoroutinePromiseAllocation);
		m_pCoroutinePromiseAllocation = _pAllocation;
	}

	inline_never void *CPromiseThreadLocal::fp_PopAllocationSlowPath()
	{
		auto pRet = m_pCoroutinePromiseAllocation;
		m_pCoroutinePromiseAllocation = m_PreviousCoroutinePromiseAllocations.f_PopBack();

		return pRet;
	}

	inline_always_lto void CPromiseThreadLocal::f_PushAllocation(void *_pAllocation)
	{
		if (!m_pCoroutinePromiseAllocation) [[likely]]
		{
			m_pCoroutinePromiseAllocation = _pAllocation;
			return;
		}

		fp_PushAllocationSlowPath(_pAllocation);
	}

	inline_always_lto void *CPromiseThreadLocal::f_PopAllocation()
	{
		if (m_PreviousCoroutinePromiseAllocations.f_IsEmpty()) [[likely]]
			return fg_Exchange(m_pCoroutinePromiseAllocation, nullptr);

		return fp_PopAllocationSlowPath();
	}
}

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
			EExecutionPriority m_DefaultExecutionPriority[EPriority_Max]
#ifdef DPlatformFamily_macOS
				= {EExecutionPriority_BelowNormal, EExecutionPriority_Normal, EExecutionPriority_Normal}
#else
				= {EExecutionPriority_Lowest, EExecutionPriority_Normal, EExecutionPriority_Normal}
#endif
			;
		};

		constinit TCSubSystem<CSubSystem_Concurrency, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Concurrency = {DAggregateInit};
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

	NConcurrency::TCActor<NConcurrency::CConcurrentActor> const &fg_ConcurrentActorHighCPU()
	{
		return fg_ConcurrencyManager().f_GetConcurrentActorHighCPU();
	}

	CBlockingActorCheckout fg_BlockingActor()
	{
		return fg_ConcurrencyManager().f_GetBlockingActor();
	}

	TCActor<CDirectCallActor> const &fg_DirectCallActor()
	{
		return fg_ConcurrencyManager().f_GetDirectCallActor();
	}

	TCActor<CThisConcurrentActor> const &fg_ThisConcurrentActor()
	{
		return fg_ConcurrencyManager().f_GetThisConcurrentActor();
	}

	TCActor<CThisConcurrentActorLowPrio> const &fg_ThisConcurrentActorLowPrio()
	{
		return fg_ConcurrencyManager().f_GetThisConcurrentActorLowPrio();
	}

	TCActor<CThisConcurrentActorHighCPU> const &fg_ThisConcurrentActorHighCPU()
	{
		return fg_ConcurrencyManager().f_GetThisConcurrentActorHighCPU();
	}

	TCActor<COtherConcurrentActor> const &fg_OtherConcurrentActor()
	{
		return fg_ConcurrencyManager().f_GetOtherConcurrentActor();
	}

	TCActor<COtherConcurrentActorLowPrio> const &fg_OtherConcurrentActorLowPrio()
	{
		return fg_ConcurrencyManager().f_GetOtherConcurrentActorLowPrio();
	}

	TCActor<COtherConcurrentActorHighCPU> const &fg_OtherConcurrentActorHighCPU()
	{
		return fg_ConcurrencyManager().f_GetOtherConcurrentActorHighCPU();
	}

	TCActor<CDynamicConcurrentActor> const &fg_DynamicConcurrentActor()
	{
		return fg_ConcurrencyManager().f_GetDynamicConcurrentActor();
	}

	TCActor<CDynamicConcurrentActorLowPrio> const &fg_DynamicConcurrentActorLowPrio()
	{
		return fg_ConcurrencyManager().f_GetDynamicConcurrentActorLowPrio();
	}

	TCActor<CDynamicConcurrentActorHighCPU> const &fg_DynamicConcurrentActorHighCPU()
	{
		return fg_ConcurrencyManager().f_GetDynamicConcurrentActorHighCPU();
	}

//#define DMibConcurrency 1

	CConcurrencyThreadLocal::CConcurrencyThreadLocal()
		: m_SystemThreadLocal(fg_SystemThreadLocal())
	{

		m_SystemThreadLocal.m_PromiseThreadLocal.m_pConcurrencyThreadLocal = this;

		for (mint iQueue = 0; iQueue < EPriority_Max; ++iQueue)
		{
			NMisc::CRandomShiftRNG RandomGenerator
				{
					uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
					, uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
					, uint32(NTime::NPlatform::fg_Timer_CyclesFast() & constant_int64(0xFFFFFFFF))
				}
			;
#ifdef DMibConcurrency
			mint nThreads = DMibConcurrency;
#else
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
#endif
			m_JobQueueIndex[iQueue] = RandomGenerator.f_GetValue<uint32>(0, nThreads);
			m_iConcurrentActor[iQueue] = RandomGenerator.f_GetValue<uint32>(0, nThreads);
		}
	}

	CConcurrencyThreadLocal::~CConcurrencyThreadLocal() = default;

	void fg_ConcurrencyThreadLocalInit()
	{
		[[maybe_unused]] auto &Local = *NPrivate::g_SubSystem_Concurrency->m_ThreadLocal;
	}

	mark_nodebug inline_always_lto CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal()
	{
#ifdef DMibConfig_ManualForeignThreadInitialization
		DMibFastCheck(NPrivate::g_SubSystem_Concurrency.f_WasCreated() && NPrivate::g_SubSystem_Concurrency.f_GetUnsafe()->m_ThreadLocal.f_TryGet());
		// Make sure to call fg_SystemThreadInit() on non-Malterlib threads.

		return *NPrivate::g_SubSystem_Concurrency.f_GetUnsafe()->m_ThreadLocal.f_TryGet();
#else
		return *NPrivate::g_SubSystem_Concurrency->m_ThreadLocal;
#endif
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

	CConcurrencyManager::CConcurrencyManager(EExecutionPriority _ExecutionPriority[EPriority_Max])
	{
		// Init time before starting any threads
		NTime::CSystem_Time::fs_TimeInitDone();

#ifdef DMibConcurrency
		mint nThreads = DMibConcurrency;
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
		m_ThisConcurrentActor = f_ConstructActor(fg_Construct<CThisConcurrentActorImpl>());
		m_ThisConcurrentActorLowPrio = f_ConstructActor(fg_Construct<CThisConcurrentActorLowPrioImpl>());
		m_ThisConcurrentActorHighCPU = f_ConstructActor(fg_Construct<CThisConcurrentActorHighCPUImpl>());
		m_OtherConcurrentActor = f_ConstructActor(fg_Construct<COtherConcurrentActorImpl>());
		m_OtherConcurrentActorLowPrio = f_ConstructActor(fg_Construct<COtherConcurrentActorLowPrioImpl>());
		m_OtherConcurrentActorHighCPU = f_ConstructActor(fg_Construct<COtherConcurrentActorHighCPUImpl>());
		m_DynamicConcurrentActor = f_ConstructActor(fg_Construct<CDynamicConcurrentActorImpl>());
		m_DynamicConcurrentActorLowPrio = f_ConstructActor(fg_Construct<CDynamicConcurrentActorLowPrioImpl>());
		m_DynamicConcurrentActorHighCPU = f_ConstructActor(fg_Construct<CDynamicConcurrentActorHighCPUImpl>());

		m_DirectCallActorRef = m_DirectCallActor;
		m_ThisConcurrentActorRef = m_ThisConcurrentActor;
		m_ThisConcurrentActorLowPrioRef = m_ThisConcurrentActorLowPrio;
		m_ThisConcurrentActorHighCPURef = m_ThisConcurrentActorHighCPU;
		m_OtherConcurrentActorRef = m_OtherConcurrentActor;
		m_OtherConcurrentActorLowPrioRef = m_OtherConcurrentActorLowPrio;
		m_OtherConcurrentActorHighCPURef = m_OtherConcurrentActorHighCPU;
		m_DynamicConcurrentActorRef = m_DynamicConcurrentActor;
		m_DynamicConcurrentActorLowPrioRef = m_DynamicConcurrentActorLowPrio;
		m_DynamicConcurrentActorHighCPURef = m_DynamicConcurrentActorHighCPU;
	}

#if DMibEnableSafeCheck > 0

	namespace
	{
		struct CCoroutineActor : public CActor
		{
			TCFuture<void> f_ReferenceCoroutine(uint32 _Reference)
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
		CoroutineActor.f_Bind<&CCoroutineActor::f_ReferenceCoroutine, EVirtualCall::mc_NotVirtual>(5).f_CallSync();

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
		m_bStopped = true;
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
				fg_Move(m_pTimerActor).f_Destroy().f_DiscardResult();
		}

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

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
							if (!Queue.m_JobQueueLocal.m_LocalQueue.f_GetFirst())
								continue;
							bDoneSomething = true;
							while (auto pEntry = Queue.m_JobQueueLocal.m_LocalQueue.f_GetFirst())
							{
								pEntry->m_Link.f_UnsafeUnlink();
								pEntry->f_Call(ThreadLocal);
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
		if (!m_bStopped)
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
#if DMibConfig_Concurrency_DebugFutures
		{
			DMibLock(m_FutureListLock);
			if (!this->m_Futures.f_IsEmpty())
			{
				DMibTrace("Futures alive in ~CConcurrencyManager():{\n}", 0);
				for (auto &Future : this->m_Futures)
				{
					DMibTrace("    TCFuture<{}>   RefCount {}{\n}", Future.m_FutureTypeName << Future.m_RefCount.f_Get());

	#if DMibConfig_RefCountDebugging
					mint iCallstack = 0;
					DMibLock(Future.m_RefCount.m_Debug->m_Lock);
					for (auto &Callstack : Future.m_RefCount.m_Debug->m_Callstacks)
					{
						DMibTrace2("        Reference callstack {} {}\n", iCallstack, Callstack.m_CallstackLen);
						Callstack.f_Trace(12);
						++iCallstack;
					}
	#endif
				}
			}
			DMibFastCheck(m_Futures.f_IsEmpty());
		}
#endif
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

		NStr::CStr Name;
		EExecutionPriority ThreadPrio = _pThis->m_ExecutionPriority[m_Priority];
		switch (m_Priority)
		{
		case EPriority_Low:
			Name = NStr::gc_Str<"LP">;
			break;
		case EPriority_NormalHighCPU:
			Name = NStr::gc_Str<"HC">;
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
					, NStr::CStrNonTracked::CFormat("Pool{} {}") << Name << m_iQueue
					, ThreadPrio
				)
			)
		;
		m_bThreadCreated.f_Exchange(true);
	}

	inline_always void CConcurrencyManager::CQueue::f_Signal(CConcurrencyManager *_pThis)
	{
		m_Event.f_Signal();
		if (!m_bThreadCreated.f_Load(NAtomic::EMemoryOrder_Relaxed))
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

	void CConcurrencyManager::fp_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pThisQueue)
		{
			_ThreadLocal.m_pThisQueue->m_JobQueue.f_AddToQueueLocalFirst(fg_Move(_ToQueue), _ThreadLocal.m_pThisQueue->m_JobQueueLocal);
			return;
		}

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		if (_ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
			_ThreadLocal.m_iConcurrentActor[_Priority] = 0;
		auto &Queue = m_Queues[_Priority].f_GetArray()[_ThreadLocal.m_iConcurrentActor[_Priority]];
		++_ThreadLocal.m_iConcurrentActor[_Priority];

		if (fp_AddToQueue(Queue, fg_Move(_ToQueue), _ThreadLocal))
			Queue.f_Signal(this);
	}

	void CConcurrencyManager::f_DispatchOnCurrentThreadOrConcurrentFirst(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue)
	{
		fp_DispatchOnCurrentThreadOrConcurrentFirst(_Priority, fg_Move(_ToQueue), fg_ConcurrencyThreadLocal());
	}

	void CConcurrencyManager::fp_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pThisQueue)
		{
			_ThreadLocal.m_pThisQueue->m_JobQueue.f_AddToQueueLocal(fg_Move(_ToQueue), _ThreadLocal.m_pThisQueue->m_JobQueueLocal);
			return;
		}

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		if (_ThreadLocal.m_iConcurrentActor[_Priority] >= nActors)
			_ThreadLocal.m_iConcurrentActor[_Priority] = 0;
		auto &Queue = m_Queues[_Priority].f_GetArray()[_ThreadLocal.m_iConcurrentActor[_Priority]];
		++_ThreadLocal.m_iConcurrentActor[_Priority];

		if (fp_AddToQueue(Queue, fg_Move(_ToQueue), _ThreadLocal))
			Queue.f_Signal(this);
	}

	void CConcurrencyManager::f_DispatchOnCurrentThreadOrConcurrent(EPriority _Priority, FActorQueueDispatchNoAlloc &&_ToQueue)
	{
		fp_DispatchOnCurrentThreadOrConcurrent(_Priority, fg_Move(_ToQueue), fg_ConcurrencyThreadLocal());
	}

	void CConcurrencyManager::fp_QueueJob(EPriority _Priority, mint _iFixedQueue, mint _iLastQueue, FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal)
	{
		auto pQueueEntry = CConcurrentRunQueueNonVirtualNoAlloc::fs_QueueEntry(fg_Move(_ToQueue));

		mint iJobQueue;
		if (_iFixedQueue < m_nThreads)
			iJobQueue = _iFixedQueue;
		else
		{
			do
			{
				mint iThisQueue = TCLimitsInt<mint>::mc_Max;
				if (_ThreadLocal.m_pThisQueue)
				{
					iThisQueue = _ThreadLocal.m_pThisQueue->m_iQueue;
					if (_ThreadLocal.m_bForceNonLocal) [[unlikely]]
					{
						iJobQueue = iThisQueue;
						break;
					}
					if (_ThreadLocal.m_pThisQueue->m_Priority == _Priority && _ThreadLocal.m_pThisQueue->m_JobQueue.f_IsEmpty(_ThreadLocal.m_pThisQueue->m_JobQueueLocal))
					{
						iJobQueue = iThisQueue;
						break;
					}
				}

				if (_ThreadLocal.m_pCurrentlyProcessingActorHolder && _ThreadLocal.m_bCurrentlyProcessingInActorHolder)
				{
					mint iCurrentQueue = _ThreadLocal.m_pCurrentlyProcessingActorHolder->mp_iFixedQueue;
					if (iCurrentQueue < m_nThreads)
					{
						iJobQueue = iCurrentQueue;
						break;
					}
				}

				if (_iLastQueue < m_nThreads)
				{
					auto &Queue = m_Queues[_Priority].f_GetArray()[_iLastQueue];
					if (Queue.m_JobQueue.f_AddToQueueIfEmpty(fg_Move(pQueueEntry)))
					{
						mint Value = Queue.m_Working.f_FetchAdd(1);
						if (Value == 0)
							Queue.f_Signal(this);
						return;
					}
				}

				auto &iJobQueueNew = _ThreadLocal.m_JobQueueIndex[_Priority];
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
		if (fp_AddToQueue(Queue, fg_Move(pQueueEntry), _ThreadLocal))
			Queue.f_Signal(this);
	}

	constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);

	bool CConcurrencyManager::fp_AddToQueue(CQueue &_Queue, NStorage::TCUniquePointer<CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc> &&_pQueueEntry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pThisQueue == &_Queue)
		{
			if (!_ThreadLocal.m_bForceNonLocal) [[likely]]
			{
				_Queue.m_JobQueue.f_AddToQueueLocal(fg_Move(_pQueueEntry), _Queue.m_JobQueueLocal);
				return false;
			}
		}
		_Queue.m_JobQueue.f_AddToQueue(fg_Move(_pQueueEntry));

		mint Value = _Queue.m_Working.f_FetchAdd(1);
		return Value == 0;
	}

	bool CConcurrencyManager::fp_AddToQueue(CQueue &_Queue, FActorQueueDispatchNoAlloc &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pThisQueue == &_Queue)
		{
			if (!_ThreadLocal.m_bForceNonLocal) [[likely]]
			{
				_Queue.m_JobQueue.f_AddToQueueLocal(fg_Move(_Functor), _Queue.m_JobQueueLocal);
				return false;
			}
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
#ifdef DDoWorkPolling
			NTime::CCyclesClock Clock;
			Clock.f_Start();
#endif
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
							while (auto *pJob = _Queue.m_JobQueueLocal.m_LocalQueue.f_GetFirst())
							{
#if DMibPPtrBits > 32
								if (!Checkout.f_IsCheckedOut())
									Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#endif

								pJob->m_Link.f_UnsafeUnlink();
								pJob->f_Call(ThreadLocal);
								bDoneSomething = true;
							}
						}
					}
				}

#if DMibPPtrBits > 32
				Checkout = NMemory::CMemoryManagerCheckout(nullptr);
#endif
#ifdef DDoWorkPolling
				if (Clock.f_GetTime() > 0.000'035) // Loop for at least 35 µs before going to kernel
					break;
#else
				break;
#endif
			}
			_Queue.m_Event.f_Wait();
		}
	}

	void CConcurrencyManager::fp_AddedActor()
	{
		DMibFastCheck(!m_bFinishedDestroying);

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

	void CConcurrencyManager::f_EnableShutdownLogging(bool _bEnabled)
	{
		m_bShutdownLogging = _bEnabled;
	}

	void CConcurrencyManager::f_BlockOnDestroy()
	{
		if (m_bDestroyed)
			return;
		fp_InitConcurrentActors(); // Make sure concurrent actors are created
		auto &TimerActor = f_GetTimerActor();

		m_bDestroyed = true;

		NTime::CClock Clock{true};
		NTime::CClock TimerClock{true};

		static constexpr mint c_nDirectDeleteActors
			= sizeof(m_DirectCallActor) / sizeof(m_DirectCallActor) // NOLINT
			+ sizeof(m_ThisConcurrentActor) / sizeof(m_ThisConcurrentActor) // NOLINT
			+ sizeof(m_ThisConcurrentActorLowPrio) / sizeof(m_ThisConcurrentActorLowPrio) // NOLINT
			+ sizeof(m_ThisConcurrentActorHighCPU) / sizeof(m_ThisConcurrentActorHighCPU) // NOLINT
			+ sizeof(m_OtherConcurrentActor) / sizeof(m_OtherConcurrentActor) // NOLINT
			+ sizeof(m_OtherConcurrentActorLowPrio) / sizeof(m_OtherConcurrentActorLowPrio) // NOLINT
			+ sizeof(m_OtherConcurrentActorHighCPU) / sizeof(m_OtherConcurrentActorHighCPU) // NOLINT
			+ sizeof(m_DynamicConcurrentActor) / sizeof(m_DynamicConcurrentActor) // NOLINT
			+ sizeof(m_DynamicConcurrentActorLowPrio) / sizeof(m_DynamicConcurrentActorLowPrio) // NOLINT
			+ sizeof(m_DynamicConcurrentActorHighCPU) / sizeof(m_DynamicConcurrentActorHighCPU) // NOLINT
		;

		if (m_bShutdownLogging)
			DMibLog(Info, "Shutting down concurrency manager: Waiting for actors to dissapear");

#if DMibConfig_Concurrency_DebugBlockDestroy
		bool bAborted = false;
#endif
		{
			// Disregard concurrent actors, timer actor and direct delete actors from destroy
			auto fHasUserActors = [&]
				{
					DMibLock(m_BlockingActorsLock);
					mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen()
						+ m_ConcurrentActors[EPriority_NormalHighCPU].f_GetLen()
						+ m_ConcurrentActors[EPriority_Low].f_GetLen()
						+ 1
						+ c_nDirectDeleteActors
						+ m_nBlockingActors
					;

					return fp_NumActors() > nExpectedActors;
				}
			;
#if DMibConfig_Concurrency_DebugBlockDestroy
			volatile static bool s_AbortLoop = false;
#endif
			TimerActor(&CTimerActor::f_FireAtExit).f_CallSync();

			bool bLoggedLongTimeShutdown = false;

			while (fHasUserActors())
			{
				if (TimerClock.f_GetTime() > 0.010)
				{
					if (Clock.f_GetTime() > 10.0)
					{
						TimerActor(&CTimerActor::f_FireAllTimeouts).f_CallSync();
						if (m_bShutdownLogging && !bLoggedLongTimeShutdown)
						{
							bLoggedLongTimeShutdown = true;
							DMibLog(Info, "Shutting down concurrency manager: Shutdown is taking a long time, firing all timeouts", bLoggedLongTimeShutdown);
						}
					}
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
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CConcurrentActorHighCPUImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CTimerActor") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDirectCallActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CThisConcurrentActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CThisConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CThisConcurrentActorHighCPUImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::COtherConcurrentActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::COtherConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::COtherConcurrentActorHighCPUImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDynamicConcurrentActorImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDynamicConcurrentActorLowPrioImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CDynamicConcurrentActorHighCPUImpl") >= 0
									|| Actor.m_ActorTypeName.f_Find("NMib::NConcurrency::CBlockingActor") >= 0
								)
							{
								continue;
							}

							DMibTrace
								(
									"    {}   Destroyed {}   RefCount {}   WeakCount {}{\n}"
									, Actor.m_ActorTypeName << Actor.mp_Destroyed.f_Load() << Actor.m_RefCount.f_Get() << Actor.m_RefCount.f_WeakGet()
								)
							;

	#if DMibConfig_RefCountDebugging
							mint iCallstack = 0;
							DMibLock(Actor.m_RefCount.m_Debug->m_Lock);
							for (auto &Callstack : Actor.m_RefCount.m_Debug->m_Callstacks)
							{
								DMibTrace2("        Reference callstack {} {}\n", iCallstack, Callstack.m_CallstackLen);
								Callstack.f_Trace(12);
								++iCallstack;
							}
							iCallstack = 0;
							for (auto &Callstack : Actor.m_RefCount.m_Debug->m_WeakCallstacks)
							{
								DMibTrace2("        Weak reference callstack {} {}\n", iCallstack, Callstack.m_CallstackLen);
								Callstack.f_Trace(12);
								++iCallstack;
							}
	#endif
						}
					}

#if DMibConfig_Concurrency_DebugFutures
					{
						DMibLock(m_FutureListLock);
						if (!this->m_Futures.f_IsEmpty())
						{
							DMibTrace("Futures alive:{\n}", 0);
							for (auto &Future : this->m_Futures)
							{
								DMibTrace("    TCFuture<{}>   RefCount {}{\n}", Future.m_FutureTypeName << Future.m_RefCount.f_Get());

		#if DMibConfig_RefCountDebugging
								mint iCallstack = 0;
								DMibLock(Future.m_RefCount.m_Debug->m_Lock);
								for (auto &Callstack : Future.m_RefCount.m_Debug->m_Callstacks)
								{
									DMibTrace2("        Reference callstack {} {}\n", iCallstack, Callstack.m_CallstackLen);
									Callstack.f_Trace(12);
									++iCallstack;
								}
		#endif
							}
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

		auto fDestroyFreeBlockingActors = [&]()
			{
				DMibLock(m_BlockingActorsLock);
				NContainer::TCVector<mint> ToRemove;
				for (auto &pActor : m_BlockingActors)
				{
					if (pActor->m_Link.f_IsInList())
						ToRemove.f_Insert(&pActor - m_BlockingActors.f_GetArray());
				}

				for (auto &iRemove : ToRemove.f_Reverse())
				{
					m_BlockingActors.f_Remove(iRemove);
					--m_nBlockingActors;
				}
			}
		;

		fDestroyFreeBlockingActors();

		auto fWaitForAllDone = [&]()
			{
				for (bool bAllDone = false; !bAllDone;)
				{
					TCFutureVector<bool> ConcurrentActorSyncs;
					{
						DMibLock(m_TimerActorLock);
						if (m_pTimerActor)
						{
							g_Dispatch(m_pTimerActor) / []() -> bool
								{
									auto pInternalActor = fg_GetActorInternal(fg_CurrentActor());
									return pInternalActor->mp_ConcurrentRunQueue.f_OneOrLessInQueue(pInternalActor->mp_ConcurrentRunQueueLocal);
								}
								> ConcurrentActorSyncs
							;
						}
					}

					{
						DMibLock(m_ThreadCreateLock);
						for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
						{
							for (auto &Actor : m_ConcurrentActors[Prio])
							{
								DMibFastCheck(Actor->mp_iFixedQueue != gc_InvalidQueue);
								if (!m_Queues[Prio][Actor->mp_iFixedQueue].m_bThreadCreated.f_Load())
									continue;

								g_Dispatch(Actor) / []() -> bool
									{
										auto pInternalActor = fg_GetActorInternal(fg_CurrentActor());
										return pInternalActor->mp_ConcurrentRunQueue.f_OneOrLessInQueue(pInternalActor->mp_ConcurrentRunQueueLocal);
									}
									> ConcurrentActorSyncs
								;
							}
						}
					}

					auto Done = fg_AllDoneWrapped(ConcurrentActorSyncs).f_CallSync();
					bAllDone = true;
					for (auto &Done : Done)
					{
						if (Done && !*Done)
						{
							bAllDone = false;
							break;
						}
					}
				}
			}
		;

		if (m_bShutdownLogging)
			DMibLog(Info, "Shutting down concurrency manager: Waiting for built in actors to exit");
		fWaitForAllDone();

		// Delete the timer actor
		{
			{
				DMibLock(m_TimerActorLock);
				if (m_pTimerActor)
					fg_Move(m_pTimerActor).f_Destroy().f_CallSync();
			}
			mint nExpectedActors = m_ConcurrentActors[EPriority_Normal].f_GetLen()
				+ m_ConcurrentActors[EPriority_NormalHighCPU].f_GetLen()
				+ m_ConcurrentActors[EPriority_Low].f_GetLen()
				+ c_nDirectDeleteActors
			;
			while (fp_NumActors() > nExpectedActors)
			{
				fDestroyFreeBlockingActors();
				NSys::fg_Thread_SmallestSleep();
			}
		}

		if (m_bShutdownLogging)
			DMibLog(Info, "Shutting down concurrency manager: Waiting for blocking actors to exit");
		fWaitForAllDone(); // Sync up to make sure nothing is still waiting to process on actors

		m_bFinishedDestroying = true;

		m_bDestroyingAlwaysAliveActors = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bDestroyingAlwaysAliveActors = false;
			}
		;

		// Delete the concurrent actors
		{
			TCFutureVector<void> Destroys;
			{
				DMibLock(m_pConcurrentActorLock);
				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
				{
					for (auto &Actor : m_ConcurrentActors[Prio])
					{
						DMibFastCheck(Actor->mp_iFixedQueue != gc_InvalidQueue);
						DMibLock(m_ThreadCreateLock);
						if (!m_Queues[Prio][Actor->mp_iFixedQueue].m_bThreadCreated.f_Load())
						{
							Actor.f_Unsafe_AccessInternal()->mp_Priority = EPriority_Normal; // Force to normal priority so we don't create threads just for this
							fg_Move(Actor).fp_DestroyUnused() > Destroys;
						}
						else
							fg_Move(Actor).f_Destroy() > Destroys;
					}
				}

				for (mint Prio = EPriority_Low; Prio < EPriority_Max; ++Prio)
					m_ConcurrentActors[Prio].f_Clear();

				m_nThreads = 0;
			}
			fg_AllDoneWrapped(Destroys).f_CallSync();
			while (fp_NumActors() > c_nDirectDeleteActors)
				NSys::fg_Thread_SmallestSleep();
		}

		// Finally delete the direct call actor

		{
			m_ThisConcurrentActor->fp_Terminate();
			m_ThisConcurrentActor.f_Clear();
			m_ThisConcurrentActorLowPrio->fp_Terminate();
			m_ThisConcurrentActorLowPrio.f_Clear();
			m_ThisConcurrentActorHighCPU->fp_Terminate();
			m_ThisConcurrentActorHighCPU.f_Clear();
			m_OtherConcurrentActor->fp_Terminate();
			m_OtherConcurrentActor.f_Clear();
			m_OtherConcurrentActorLowPrio->fp_Terminate();
			m_OtherConcurrentActorLowPrio.f_Clear();
			m_OtherConcurrentActorHighCPU->fp_Terminate();
			m_OtherConcurrentActorHighCPU.f_Clear();
			m_DynamicConcurrentActor->fp_Terminate();
			m_DynamicConcurrentActor.f_Clear();
			m_DynamicConcurrentActorLowPrio->fp_Terminate();
			m_DynamicConcurrentActorLowPrio.f_Clear();
			m_DynamicConcurrentActorHighCPU->fp_Terminate();
			m_DynamicConcurrentActorHighCPU.f_Clear();
			m_DirectCallActor->fp_Terminate();
			m_DirectCallActor.f_Clear();
		}

		if (m_bShutdownLogging)
			DMibLog(Info, "Shutting down concurrency manager: Waiting for low level actors to disappear");

		while (fp_NumActors() > 0)
			NSys::fg_Thread_SmallestSleep();

		if (m_bShutdownLogging)
			DMibLog(Info, "Shutting down concurrency manager: Done");
	}

	bool CConcurrencyManager::f_DestroyingAlwaysAliveActors() const
	{
		return m_bDestroyingAlwaysAliveActors.f_Load();
	}

	mint CConcurrencyManager::f_GetConcurrency() const
	{
		return m_nThreads;
	}

	mint CConcurrencyManager::f_GetQueue() const
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue)
			return ThreadLocal.m_pThisQueue->m_iQueue;

		return TCLimitsInt<mint>::mc_Max;
	}

	inline_never mint CConcurrencyManager::fp_InitConcurrentActors()
	{
		DMibLock(m_pConcurrentActorLock);
		mint nActors = m_nConcurrentActors.f_Load();
		if (nActors)
			return nActors;

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
				else if (Prio == EPriority_NormalHighCPU)
					Actor = f_ConstructActor(fg_Construct<CConcurrentActorHighCPUImpl>());
				else if (Prio == EPriority_Low)
					Actor = f_ConstructActor(fg_Construct<CConcurrentActorLowPrioImpl>());
				Actor->f_SetFixedQueue(iActor);
				m_ConcurrentActorsRef[Prio][iActor] = Actor;
				++iActor;
			}
		}

		m_nConcurrentActors.f_Store(nActors, NAtomic::EMemoryOrder_Release);
		return nActors;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorForThisThread(EPriority _Priority)
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pThisQueue && ThreadLocal.m_pThisQueue->m_Priority == _Priority)
		{
			auto &ToReturn = m_ConcurrentActorsRef[_Priority].f_GetArray()[ThreadLocal.m_pThisQueue->m_iQueue];
			return ToReturn;
		}
		auto &iConcurrentActor = ThreadLocal.m_iConcurrentActor[_Priority];
		if (iConcurrentActor >= nActors)
			iConcurrentActor = 0;
		auto &ToReturn = m_ConcurrentActorsRef[_Priority].f_GetArray()[iConcurrentActor];
		++iConcurrentActor;
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorForOtherThread(EPriority _Priority)
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mint NonAllowedQueue = TCLimitsInt<mint>::mc_Max;
		if (ThreadLocal.m_pThisQueue && ThreadLocal.m_pThisQueue->m_Priority == _Priority)
			NonAllowedQueue = ThreadLocal.m_pThisQueue->m_iQueue;

		auto &iConcurrentActor = ThreadLocal.m_iConcurrentActor[_Priority];
		if (iConcurrentActor >= nActors)
			iConcurrentActor = 0;

		if (nActors > 1)
		{
			while (iConcurrentActor == NonAllowedQueue)
			{
				++iConcurrentActor;
				if (iConcurrentActor >= nActors)
					iConcurrentActor = 0;
			}
		}
		auto &ToReturn = m_ConcurrentActorsRef[_Priority].f_GetArray()[iConcurrentActor];
		++iConcurrentActor;
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor(TCWeakActor<CActor> const &_Actor)
	{
		// Returns a actor that is consistent based on weak pointer

		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		DMibFastCheck(nActors > 0);

		mint HashData = reinterpret_cast<mint>(static_cast<void const *>(_Actor.fp_Get()));

		mint iActor = fg_JenkinsHash((ch8 const *)&HashData, sizeof(HashData), 0) % nActors;

		auto &ToReturn = m_ConcurrentActorsRef[EPriority_Normal].f_GetArray()[iActor];
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActor()
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
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
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_iConcurrentActor[EPriority_Low] >= nActors)
			ThreadLocal.m_iConcurrentActor[EPriority_Low] = 0;
		auto &ToReturn = m_ConcurrentActorsRef[EPriority_Low].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_Low]];
		++ThreadLocal.m_iConcurrentActor[EPriority_Low];
		return ToReturn;
	}

	TCActor<CConcurrentActor> const &CConcurrencyManager::f_GetConcurrentActorHighCPU()
	{
		mint nActors = m_nConcurrentActors.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!nActors)
			nActors = fp_InitConcurrentActors();

		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_iConcurrentActor[EPriority_NormalHighCPU] >= nActors)
			ThreadLocal.m_iConcurrentActor[EPriority_NormalHighCPU] = 0;
		auto &ToReturn = m_ConcurrentActorsRef[EPriority_NormalHighCPU].f_GetArray()[ThreadLocal.m_iConcurrentActor[EPriority_NormalHighCPU]];
		++ThreadLocal.m_iConcurrentActor[EPriority_NormalHighCPU];
		return ToReturn;
	}

	CBlockingActorCheckout::CBlockingActorCheckout(CConcurrencyManager *_pConcurrencyManager, CBlockingActorStorage *_pBlockingActorStorage)
		: mp_pConcurrencyManager(_pConcurrencyManager)
		, mp_pBlockingActorStorage(_pBlockingActorStorage)
	{
	}

	CBlockingActorCheckout::CBlockingActorCheckout(CBlockingActorCheckout &&_Other)
		: mp_pConcurrencyManager(fg_Exchange(_Other.mp_pConcurrencyManager, nullptr))
		, mp_pBlockingActorStorage(fg_Exchange(_Other.mp_pBlockingActorStorage, nullptr))
	{
	}

	CBlockingActorCheckout &CBlockingActorCheckout::operator = (CBlockingActorCheckout &&_Other)
	{
		mp_pConcurrencyManager = fg_Exchange(_Other.mp_pConcurrencyManager, nullptr);
		mp_pBlockingActorStorage = fg_Exchange(_Other.mp_pBlockingActorStorage, nullptr);

		return *this;
	}

	void CBlockingActorCheckout::fsp_LogError(NStr::CStr const &_Category, NStr::CStr const &_Error, CAsyncResult const &_Result)
	{
		_Result > fg_LogError(_Category, _Error ? _Error : NStr::gc_Str<"Error in blocking actor operation">.m_Str);
	}

	TCActor<CBlockingActor> const &CBlockingActorCheckout::f_Actor() const
	{
		DMibFastCheck(mp_pBlockingActorStorage);
		return mp_pBlockingActorStorage->m_Actor;
	}

	CBlockingActorCheckout::~CBlockingActorCheckout()
	{
		if (!mp_pConcurrencyManager || !mp_pBlockingActorStorage)
			return;

		DMibLock(mp_pConcurrencyManager->m_BlockingActorsLock);
		mp_pConcurrencyManager->m_FreeBlockingActors.f_Insert(*mp_pBlockingActorStorage);
	}

	void CBlockingActorCheckout::f_Clear()
	{
		if (!mp_pConcurrencyManager || !mp_pBlockingActorStorage)
			return;

		DMibLock(mp_pConcurrencyManager->m_BlockingActorsLock);
		mp_pConcurrencyManager->m_FreeBlockingActors.f_Insert(*mp_pBlockingActorStorage);
		mp_pConcurrencyManager = nullptr;
		mp_pBlockingActorStorage = nullptr;
	}

	bool CBlockingActorCheckout::f_IsValid() const
	{
		return !!mp_pBlockingActorStorage;
	}

	CRoundRobinBlockingActors::CRoundRobinBlockingActors(mint _Capacity)
		: m_Capacity(_Capacity)
	{
		m_Checkouts.f_Reserve(_Capacity);
	}

	CBlockingActorCheckout &CRoundRobinBlockingActors::operator *()
	{
		mint iCheckout = m_iCheckout;
		++m_iCheckout;
		if (m_iCheckout >= m_Capacity)
			m_iCheckout = 0;

		if (m_Checkouts.f_GetLen() <= iCheckout)
			m_Checkouts.f_Insert(fg_BlockingActor());

		return m_Checkouts[iCheckout];
	}

	CBlockingActorCheckout CConcurrencyManager::f_GetBlockingActor()
	{
		using namespace NStr;

		DMibLock(m_BlockingActorsLock);

		auto *pBlockingActor = m_FreeBlockingActors.f_Pop();
		if (pBlockingActor)
			return CBlockingActorCheckout(this, pBlockingActor);

		auto &pNewActor = m_BlockingActors.f_Insert();
		pNewActor = fg_Construct();
		auto pNewActorRawPtr = pNewActor.f_Get();
		auto iBlockingActor = m_nBlockingActors++;
		{
			DMibUnlock(m_BlockingActorsLock);
			pNewActorRawPtr->m_Actor = fg_Construct(fg_Construct(), "Blocking {}"_f << iBlockingActor);
		}

		return CBlockingActorCheckout(this, pNewActorRawPtr);
	}

	TCActor<CDirectCallActor> const &CConcurrencyManager::f_GetDirectCallActor()
	{
		return m_DirectCallActorRef;
	}

	TCActor<CThisConcurrentActor> const &CConcurrencyManager::f_GetThisConcurrentActor()
	{
		return m_ThisConcurrentActorRef;
	}

	TCActor<CThisConcurrentActorLowPrio> const &CConcurrencyManager::f_GetThisConcurrentActorLowPrio()
	{
		return m_ThisConcurrentActorLowPrioRef;
	}

	TCActor<CThisConcurrentActorHighCPU> const &CConcurrencyManager::f_GetThisConcurrentActorHighCPU()
	{
		return m_ThisConcurrentActorHighCPURef;
	}

	TCActor<COtherConcurrentActor> const &CConcurrencyManager::f_GetOtherConcurrentActor()
	{
		return m_OtherConcurrentActorRef;
	}

	TCActor<COtherConcurrentActorLowPrio> const &CConcurrencyManager::f_GetOtherConcurrentActorLowPrio()
	{
		return m_OtherConcurrentActorLowPrioRef;
	}

	TCActor<COtherConcurrentActorHighCPU> const &CConcurrencyManager::f_GetOtherConcurrentActorHighCPU()
	{
		return m_OtherConcurrentActorHighCPURef;
	}

	TCActor<CDynamicConcurrentActor> const &CConcurrencyManager::f_GetDynamicConcurrentActor()
	{
		return m_DynamicConcurrentActorRef;
	}

	TCActor<CDynamicConcurrentActorLowPrio> const &CConcurrencyManager::f_GetDynamicConcurrentActorLowPrio()
	{
		return m_DynamicConcurrentActorLowPrioRef;
	}

	TCActor<CDynamicConcurrentActorHighCPU> const &CConcurrencyManager::f_GetDynamicConcurrentActorHighCPU()
	{
		return m_DynamicConcurrentActorHighCPURef;
	}

	TCActor<CTimerActor> const &CConcurrencyManager::f_GetTimerActor()
	{
		if (!m_bTimerActorInit.f_Load(NAtomic::EMemoryOrder_Acquire))
		{
			DMibLock(m_TimerActorLock);
			if (!m_bTimerActorInit.f_Load())
			{
				m_pTimerActor = fg_Construct(fg_Construct(), "Timer");
				m_bTimerActorInit.f_Store(true);
			}
		}
		return m_pTimerActor;
	}

	CFutureCoroutineContextOnResumeScopeAwaiter fg_CurrentActorCheckDestroyedOnResume()
	{
		return fg_CurrentActorCheckDestroyedOnResume(fg_ConcurrencyThreadLocal());
	}

	CFutureCoroutineContextOnResumeScopeAwaiter fg_CurrentActorCheckDestroyedOnResume(CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder);

		return _ThreadLocal.m_pCurrentlyProcessingActorHolder->fp_GetActorRelaxed()->f_CheckDestroyedOnResume();
	}

	mark_nodebug TCActor<CActor> fg_CurrentActor(CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (!_ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return {};

		TCActor<> Actor(fg_Explicit(static_cast<TCActorInternal<CActor> *>(_ThreadLocal.m_pCurrentlyProcessingActorHolder)));

		DMibFastCheck(Actor != fg_DirectCallActor()); // It's not safe to use the direct call actor

		return Actor;
	}

	mark_nodebug inline_always_lto TCActor<CActor> fg_CurrentActor()
	{
		return fg_CurrentActor(fg_ConcurrencyThreadLocal());
	}

	mark_nodebug TCWeakActor<CActor> fg_CurrentActorWeak(CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (!_ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return {};

		TCWeakActor<> Actor(TCActorHolderWeakPointer<TCActorInternal<CActor>>(static_cast<TCActorInternal<CActor> *>(_ThreadLocal.m_pCurrentlyProcessingActorHolder)));

		DMibFastCheck(Actor != fg_DirectCallActor()); // It's not safe to use the direct call actor

		return Actor;
	}

	mark_nodebug inline_always_lto TCWeakActor<CActor> fg_CurrentActorWeak()
	{
		return fg_CurrentActorWeak(fg_ConcurrencyThreadLocal());
	}

	TCFuture<void> fg_DestroySubscription(CActorSubscription &_Subscription)
	{
		if (!_Subscription)
			return g_Void;

		TCFuture<void> Future = _Subscription->f_Destroy();
		_Subscription.f_Clear();

		return Future;
	}

	CConcurrencyManager &fg_CurrentConcurrencyManager()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return ThreadLocal.m_pCurrentlyProcessingActorHolder->f_ConcurrencyManager();
		return fg_ConcurrencyManager();
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
	template struct TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>;
}
