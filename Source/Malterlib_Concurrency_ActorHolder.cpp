 // Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{
		ICDistributedActorData::~ICDistributedActorData()
		{
		}

		CActorHolder::CActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
			: mp_pConcurrencyManager(_pConcurrencyManager)
			, mp_bImmediateDelete(_bImmediateDelete)
			, mp_Priority(_Priority)
			, mp_iFixedCore(DMibBitRangeTyped(0, sizeof(mint)*8 - 3, mint))
			, mp_pDistributedActorData(fg_Move(_pDistributedActorData))
		{
		}
		
		CActorHolder::~CActorHolder()
		{
#ifdef DMibDebug
			DMibLock(mp_pConcurrencyManager->m_ActorListLock);
			m_ActorLink.f_Unlink();
#endif
			--mp_pConcurrencyManager->m_nActors;
		}

		NPtr::TCSharedPointer<ICDistributedActorData> &CActorHolder::f_GetDistributedActorData()
		{
			return mp_pDistributedActorData;
		}
		
		NPtr::TCSharedPointer<ICDistributedActorData> const &CActorHolder::f_GetDistributedActorData() const
		{
			return mp_pDistributedActorData;
		}
		
		void CActorHolder::f_SetFixedCore(mint _iFixedCore)
		{
			mp_iFixedCore = _iFixedCore;
		}

		constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);
		
		void CActorHolder::fp_Construct()
		{
			mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
			DMibFastCheck((OriginalWorking & gc_ProcessingMask) == 0);
			NPrivate::CCurrentActorScope CurrentActor(*mp_pConcurrencyManager, mp_pActor.f_Get());
			mp_pActor->f_Construct();
			mint NewWorking = mp_Working.f_Exchange(0);
			if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
				f_QueueProcess([]{}, false); // Reschedule
		}
		
		bool CActorHolder::fp_AddToQueue(FActorQueueDispatch &&_Functor)
		{
			auto &ThreadLocal = *mp_pConcurrencyManager->m_ThreadLocal;
			
			if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
			{
				mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor));
				return false;
			}
			mp_ConcurrentRunQueue.f_AddToQueue(fg_Move(_Functor));
			
			mint Value = mp_Working.f_FetchAdd(1);
			return Value == 0;
		}

		bool CActorHolder::fp_DequeueProcess(bool _bRun)
		{
			if (auto *pJob = mp_ConcurrentRunQueue.f_FirstQueueEntry())
			{
				if (_bRun)
					(*pJob)();
				mp_ConcurrentRunQueue.f_PopQueueEntry(pJob);
				return true;
			}
			return false;
		}
		
		void CActorHolder::f_RunProcess()
		{
			auto &ThreadLocal = *mp_pConcurrencyManager->m_ThreadLocal;
			auto pOldHolder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = this;
			
			auto Cleanup = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldHolder;
				}
			;
			
			bool bDoMore = true;
			while (bDoMore)
			{
				bDoMore = false;
				mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
				if ((OriginalWorking & gc_ProcessingMask) == 0)
				{
					auto UnLock 
						= fg_OnScopeExit
						(
							[&]
							{
								mint NewWorking = mp_Working.f_Exchange(0);
								if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
									bDoMore = true;
							}
						)
					;

					bool bDoneSomething = true;
					while (bDoneSomething)
					{
						bDoneSomething = false;
						if (mp_ConcurrentRunQueue.f_TransferThreadSafeQueue())
							bDoneSomething = true;
						while (fp_DequeueProcess(mp_pActor != nullptr))
							bDoneSomething = true;
					}
				}
			}
		}

		bool CActorHolder::f_ImmediateDelete() const
		{
			return mp_bImmediateDelete;
		}

		void CActorHolder::f_DestroyThreaded()
		{
		}

		
		bool CActorHolder::f_IsDestroyed() const
		{
			return mp_bDestroyed.f_Load() == 0;
		}

		
		void CActorHolder::fp_Terminate()
		{
			smint Expected = 0;
			if (mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
			{
				fp_Destroy();
			}
		}
	
		void CActorHolder::f_Destroy()
		{
			TCActor<CActor> pActor = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));

			pActor(&CActor::f_Destroy)
				> fg_AnyConcurrentActor() / [pActor](TCAsyncResult<void> &&_Result) mutable
				{
					_Result.f_Get();
					pActor->fp_Terminate();
				}
			;
		}
		
		void CActorHolder::f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop)
		{
			// Already destroyed
			NThread::CMutual ResultLock;
			TCAsyncResult<void> Result;
			
			{
				TCActor<CActor> pActor = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));

				if (_EventLoop.m_fProcess && _EventLoop.m_fWake)
				{
					align_cacheline NAtomic::TCAtomic<smint> Finished;
					pActor->f_Destroy
						(
							fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result) mutable
							{
								{
									DMibLock(ResultLock);
									Result = fg_Move(_Result);
								}
								Finished.f_Exchange(1);
								_EventLoop.m_fWake();
							}
						)
					;
					
					while (!Finished.f_Load())
					{
						_EventLoop.m_fProcess();
					}
				}
				else
				{
					NThread::CEventAutoReset Event;

					pActor->f_Destroy
						(
							fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result) mutable
							{
								{
									DMibLock(ResultLock);
									Result = fg_Move(_Result);
								}
								Event.f_Signal();
							}
						)
					;

					Event.f_Wait();
				}
			}

			{
				DMibLock(ResultLock);
				Result.f_Get();
			}

		}
	
		aint CActorHolder::f_RefCountDecrease()
		{
			// TODO: Investigate this when using weak actors
			aint Ret = CSuper::f_RefCountDecrease();
			if (Ret == 1)
			{
				smint Expected = 0;
				if (mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
				{
					// Only our own reference is left, schedule destroy
					fp_Destroy();
				}
			}
			return Ret;
		}
		
		
		void CActorHolder::fp_Destroy()
		{
			auto OnExit
				= fg_OnScopeExit
				(
					[this]()
					{
						if (mp_pActor)
						{
							mp_pActor.f_Clear();
							mp_bDestroyed.f_Exchange(2);
							TCActor<CActor> pToDelete = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));
							if (CSuper::f_RefCountDecrease() == 1)
							{
								// Dispatch to our queue again, otherwise we ourselves could be in callstack
								mp_pConcurrencyManager->f_DispatchFirstOnCurrentThread
									(
										mp_Priority
										, [pToDelete]
										{
										}
									)
								;
							}
						}
					}
				)
			;
			f_QueueProcess
				(
					[OnExit = fg_Move(OnExit)]() mutable
					{
					}
					, true
				)
			;
		}
		

		///
		/// CSeparateThreadActorHolder
		///
		CSeparateThreadActorHolder::CSeparateThreadActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, NStr::CStr const &_ThreadName
			)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
			, mp_ThreadName(_ThreadName)
		{

		}

		CSeparateThreadActorHolder::~CSeparateThreadActorHolder()
		{
			DMibRequire(m_pThread.f_IsEmpty());
		}
		void CSeparateThreadActorHolder::fp_Construct()
		{
			m_pThread = NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
						{
							NTime::CCyclesClock Clock;
							Clock.f_Start();
							while (true)
							{
								f_RunProcess();
								if (Clock.f_GetTime() > 0.000035) // Run for at least 35 µs
									break;
							}
							_pThread->m_EventWantQuit.f_Wait();
						}
						return 0;
					}
					, mp_ThreadName
				)
			;
			CDefaultActorHolder::fp_Construct();
		}
		void CSeparateThreadActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			if (fp_AddToQueue(fg_Move(_Functor)))
				m_pThread->m_EventWantQuit.f_Signal();
		}
		
		void CSeparateThreadActorHolder::f_DestroyThreaded()
		{
			m_pThread.f_Clear();
			CDefaultActorHolder::f_DestroyThreaded();
		}

		///
		/// CDispatchingActorHolder
		///
		
		CDispatchingActorHolder::CDispatchingActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, NFunction::TCFunction<void (FActorQueueDispatch &&_Dispatch)> const &_Dispatcher
			)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
			, m_Dispatcher(_Dispatcher)
		{
		}

		void CDispatchingActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			if (fp_AddToQueue(fg_Move(_Functor)))
			{
				NPtr::TCSharedPointer<CDispatchingActorHolder, NPtr::CSupportWeakTag, CInternalActorAllocator> pThis = fg_Explicit(this);
				m_Dispatcher
					(
						[pThis]()
						{
							pThis->f_RunProcess();
						}						
					)
				;
			}
		}

		///
		/// CDefaultActorHolder
		///

		void CDefaultActorHolder::fp_Construct()
		{
			CActorHolder::fp_Construct();
		}			
		CDefaultActorHolder::CDefaultActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
			: CActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		{
		}

		CConcurrentRunQueue::CConcurrentRunQueue()
		{
			static_assert(sizeof(CQueueEntry) == gc_ActorQueueDispatchFunctionMemory, "");
		}
		
		CConcurrentRunQueue::~CConcurrentRunQueue()
		{
			CQueueEntry *pEntry = mp_pFirstQueued.f_Exchange(nullptr);
			
			while (pEntry)
			{
				auto *pNextEntry = pEntry->m_pNextQueued.f_Load();
				
				delete pEntry;
				
				pEntry = pNextEntry;
			}
		}
		
		void CConcurrentRunQueue::f_AddToQueue(FActorQueueDispatch &&_Functor)
		{
			NPtr::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
			CQueueEntry *pNewEntry = pNewEntryUnique.f_Detach();
			while (true)
			{
				CQueueEntry *pFirstEntry = mp_pFirstQueued.f_Load();
				pNewEntry->m_pNextQueued = pFirstEntry;
				if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry))
					break;
			}
		}
		
		void CConcurrentRunQueue::f_AddToQueueLocal(FActorQueueDispatch &&_Functor)
		{
			NPtr::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
			pNewEntryUnique->m_Link.f_Construct();
			mp_LocalQueue.f_Insert(pNewEntryUnique.f_Detach());
		}
		
		void CConcurrentRunQueue::f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor)
		{
			NPtr::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
			pNewEntryUnique->m_Link.f_Construct();
			mp_LocalQueue.f_InsertFirst(pNewEntryUnique.f_Detach());
		}
		
		FActorQueueDispatch *CConcurrentRunQueue::f_FirstQueueEntry()
		{
			if (mp_LocalQueue.f_IsEmpty())
				return nullptr;
			return &mp_LocalQueue.f_GetFirst()->m_fToCall;
		}
		
		CConcurrentRunQueue::CQueueEntry::CQueueEntry(FActorQueueDispatch &&_fToCall)
			: m_fToCall(fg_Move(_fToCall))
		{
		}
		CConcurrentRunQueue::CQueueEntry::~CQueueEntry()
		{
			m_fToCall.f_Clear();
			m_Link.f_Unlink();
		}
		
		void CConcurrentRunQueue::f_PopQueueEntry(FActorQueueDispatch *_pEntry)
		{
			mint Offset = DMibPOffsetOf(CQueueEntry, m_fToCall);
			CQueueEntry *pEntry = (CQueueEntry *)((uint8 *)_pEntry - Offset);
			delete pEntry;
		}

		bool CConcurrentRunQueue::f_IsEmpty()
		{
			return mp_pFirstQueued.f_Load() == nullptr && mp_LocalQueue.f_IsEmpty();
		}

		bool CConcurrentRunQueue::f_TransferThreadSafeQueue()
		{
			CQueueEntry *pEntry = mp_pFirstQueued.f_Exchange(nullptr);
			if (!pEntry)
				return false;

			CQueueEntry *pInsertAfter = nullptr;
			if (!mp_LocalQueue.f_IsEmpty())
				pInsertAfter = mp_LocalQueue.f_GetLast();
			while (pEntry)
			{
				auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
				
				pEntry->m_Link.f_Construct();
				if (pInsertAfter)
					mp_LocalQueue.f_InsertAfter(pEntry, pInsertAfter);
				else
					mp_LocalQueue.f_InsertFirst(pEntry);
				
				pEntry = pNextEntry;
			}
			return true;
		}
		
		CDefaultActorHolder::~CDefaultActorHolder()
		{
		}

		void CDefaultActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			if (fp_AddToQueue(fg_Move(_Functor)))
			{
				NPtr::TCSharedPointer<CDefaultActorHolder, NPtr::CSupportWeakTag, CInternalActorAllocator> pThis = fg_Explicit(this);
				
				if (_bSame)
				{
					mp_pConcurrencyManager->f_DispatchFirstOnCurrentThread
						(
							this->mp_Priority
							, [pThis = fg_Move(pThis)]()
							{
								pThis->f_RunProcess();
							}		
						)
					;
				}
				else
				{
					mp_pConcurrencyManager->fp_QueueJob
						(
							this->mp_Priority
							, this->mp_iFixedCore
							, [pThis = fg_Move(pThis)]()
							{
								pThis->f_RunProcess();
							}		
						)
					;
				}
			}
		}
	}
}

