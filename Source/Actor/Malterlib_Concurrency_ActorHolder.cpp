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
		
		static constexpr mint gc_NoFixedCore = DMibBitRangeTyped(0, sizeof(mint)*8 - 3, mint);

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
			, mp_iFixedCore(gc_NoFixedCore)
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
		
		void CActorHolder::fp_StartQueueProcessing()
		{
		}

		void CActorHolder::fp_ConstructActor(NFunction::TCFunctionNoAllocMutable<void ()> &&_fConstruct, void *_pActorMemory)
		{
			f_RefCountIncrease();
			
			// Handle exception in construct
			auto CleanupRefCount = g_OnScopeExit > [&]
				{
					f_RefCountDecrease();
				}
			;
			
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			
			mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
			DMibFastCheck((OriginalWorking & gc_ProcessingMask) == 0);
			fp_StartQueueProcessing();
			
			auto pOldActorHalder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
#if defined DMibContractConfigure_CheckEnabled
			auto pOldConstructing = ThreadLocal.m_pCurrentlyConstructingActor;
#endif
			auto pOldActor = ThreadLocal.m_pCurrentActor;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = this;
#if defined DMibContractConfigure_CheckEnabled
			ThreadLocal.m_pCurrentlyConstructingActor = (CActor *)_pActorMemory;
#endif
			
			auto CleanupWorking = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHalder;
#if defined DMibContractConfigure_CheckEnabled
					ThreadLocal.m_pCurrentlyConstructingActor = pOldConstructing;
#endif
					ThreadLocal.m_pCurrentActor = pOldActor;
					
					mint NewWorking = mp_Working.f_Exchange(0);
					if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
						f_QueueProcess([]{}, false); // Reschedule
				}
			;
			
			auto CleanupConstruction = g_OnScopeExit > [&]
				{
					mp_pActor.f_Detach();
				}
			;

			_fConstruct();

			// Construction was successful
			CleanupConstruction.f_Clear();
			
			mp_pActor->fp_Construct();
			
			CleanupRefCount.f_Clear(); // Disable refcount decrease
		}
		
		bool CActorHolder::fp_AddToQueue(FActorQueueDispatch &&_Functor)
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			
			if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
			{
				mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor));
				return false;
			}
			else if (ThreadLocal.m_pThisQueue && mp_iFixedCore == ThreadLocal.m_pThisQueue->m_iQueue && ThreadLocal.m_pThisQueue->m_Priority == mp_Priority)
			{
				if (mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor)))
				{
					mint Value = mp_Working.f_FetchAdd(1);
					return Value == 0;
				}
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
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
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
			return mp_bDestroyed.f_Load() != 0;
		}

		
		bool CActorHolder::fp_Terminate(NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed)
		{
			smint Expected = 0;
			if (mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
			{
				fp_Destroy(fg_Move(_fOnDestroyed));
				return true;
			}
			return false;
		}
		
		TCDispatchedActorCall<void> CActorHolder::f_Destroy2()
		{
			TCActor<CActor> pActor = fp_GetAsActor<CActor>();
			
			return fg_Dispatch
				(
					pActor
					, [pActor]() -> TCContinuation<void>
					{
						TCContinuation<void> Continuation;
						
						pActor->f_Destroy
							(
								NPrivate::fg_DirectResultActor() / [Continuation](TCAsyncResult<void> &&_Result)
								{
									Continuation.f_SetResult(fg_Move(_Result));
								}
							)
						;
						
						return Continuation;
					}
				)
			;
		}

		void CActorHolder::f_DestroyNoResult(ch8 const *_pFile, uint32 _Line)
		{
			TCActor<CActor> pActor = fp_GetAsActor<CActor>();

			pActor(&CActor::fp_DestroyInternal)
				> fg_AnyConcurrentActor() / [pActor, _pFile, _Line](TCAsyncResult<void> &&_Result) mutable
				{
					try
					{
						_Result.f_Get();
					}
					catch (CExceptionActorDeleted const &)
					{
						return; // Already destroyed
					}
					catch (NException::CException const &)
					{
						DMibConErrOut2(DMibPFileLineFormat " Failed to destroy actor: {}\n", _pFile, _Line, _Result.f_GetExceptionStr());
						DMibPDebugBreak;
					}
					pActor->fp_Terminate(nullptr);
				}
			;
		}
		
		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::fp_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> &&_ResultCall, NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed)
		{
			TCActor<CActor> pActor = fp_GetAsActor<CActor>();

			pActor(&CActor::fp_DestroyInternal)
				> fg_AnyConcurrentActor() / [pActor, ResultCall = fg_Move(_ResultCall), fOnDestroyed = fg_Move(_fOnDestroyed)](TCAsyncResult<void> &&_Result) mutable
				{
					if (!pActor->fp_Terminate(fg_Move(fOnDestroyed)))
						fOnDestroyed();
					if (NTraits::TCIsSame<tf_CActor, TCActor<NPrivate::CDirectResultActor>>::mc_Value)
					{
						NPrivate::fg_CallResultFunctorDirect(ResultCall.mp_Functor, fg_Move(_Result));
						return;
					}
					auto ResultActor = ResultCall.mp_Actor.f_GetActor();
					ResultActor->f_QueueProcess
						(
							[ResultCall = fg_Move(ResultCall), Result = fg_Move(_Result)]() mutable
							{
								NPrivate::fg_CallResultFunctor(ResultCall.mp_Functor, ResultCall.mp_Actor.f_GetActor()->fp_GetActor(), fg_Move(Result));
							}
						)
					;
				}
			;
		}
		
		void CActorHolder::f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop)
		{
			// Already destroyed
			NThread::CMutual ResultLock;
			TCAsyncResult<void> Result;
			
			{
				TCActor<CActor> pActor = fp_GetAsActor<CActor>();

				if (_EventLoop.m_fProcess && _EventLoop.m_fWake)
				{
					align_cacheline NAtomic::TCAtomic<smint> Finished;
					pActor->fp_Destroy
						(
							fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result) mutable
							{
								DMibLock(ResultLock);
								Result = fg_Move(_Result);
								if (Finished.f_FetchOr(2) == 1)
									_EventLoop.m_fWake();
							}
							, [&]
							{
								if (Finished.f_FetchOr(1) == 2)
									_EventLoop.m_fWake();
							}
						)
					;
					
					while (Finished.f_Load() != 3)
						_EventLoop.m_fProcess();
				}
				else
				{
					align_cacheline NAtomic::TCAtomic<smint> Finished;
					NThread::CEventAutoReset Event;

					pActor->fp_Destroy
						(
							fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result) mutable
							{
								DMibLock(ResultLock);
								Result = fg_Move(_Result);
								if (Finished.f_FetchOr(2) == 1)
									Event.f_Signal();
							}
							, [&]
							{
								if (Finished.f_FetchOr(1) == 2)
									Event.f_Signal();
							}
						)
					;

					while (Finished.f_Load() != 3)
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
					fp_Destroy(nullptr);
				}
			}
			return Ret;
		}
		
		
		void CActorHolder::fp_Destroy(NFunction::TCFunctionNoAllocMutable<void ()> &&_fOnDestroyed)
		{
			for (auto &OnTerminate : mp_OnTerminate)
				OnTerminate.m_fOnTerminate();
			mp_OnTerminate.f_Clear();
			
			auto OnExit
				= fg_OnScopeExit
				(
					[this, fOnDestroyed = fg_Move(_fOnDestroyed)]() mutable
					{
						if (!mp_pActor)
						{
							if (fOnDestroyed)
								fOnDestroyed();
							return;
						}
						
						mp_pActor.f_Clear();
						mp_bDestroyed.f_Exchange(2);
						TCActor<CActor> pToDelete = fp_GetAsActor<CActor>();
						
						if (CSuper::f_RefCountDecrease() != 1)
						{
							if (fOnDestroyed)
								fOnDestroyed();
							return;
						}
						// Dispatch to our queue again, otherwise we ourselves could be in callstack
						mp_pConcurrencyManager->f_DispatchFirstOnCurrentThread
							(
								mp_Priority
								, fg_OnScopeExit
								(
									[pToDelete, fOnDestroyed = fg_Move(fOnDestroyed)]() mutable
									{
										pToDelete.f_Clear();
										if (fOnDestroyed)
											fOnDestroyed();
									}
								)
							)
						;
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
		
		void CSeparateThreadActorHolder::fp_StartQueueProcessing()
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
		}
		
		void CSeparateThreadActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			// Reference this so it doesn't go out of scope if queue is processed before thread has been notified
			TCActorHolderSharedPointer<CSeparateThreadActorHolder> pThis = fg_Explicit(this);
			
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
				, NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> &&_Dispatcher
			)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
			, m_Dispatcher(fg_Move(_Dispatcher))
		{
		}

		void CDispatchingActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			TCActorHolderSharedPointer<CDispatchingActorHolder> pThis = fg_Explicit(this);
			if (fp_AddToQueue(fg_Move(_Functor)))
			{
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
		/// CDelegatedActorHolder
		///
		
		CDelegatedActorHolder::CDelegatedActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, TCActor<> const &_DelegateToActor
			) 
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
			, mp_pDelegateTo(_DelegateToActor.m_pInternalActor)
		{
			_DelegateToActor.m_pInternalActor->f_QueueProcess
				(
					[pDelegateTo = _DelegateToActor.m_pInternalActor, pThis = TCActorHolderSharedPointer<CDelegatedActorHolder>{this}]
					{
						DMibFastCheck(!pDelegateTo->f_IsDestroyed());
						COnTerminate OnTerminate;
						OnTerminate.m_fOnTerminate = [pThisWeak = pThis.f_Weak()]
							{
								auto pThis = pThisWeak.f_Lock();
								if (!pThis)
									return;
								pThis->mp_pOnTerminateEntry = nullptr;
								pThis->fp_Terminate(nullptr);									
							}
						;
						pThis->mp_pOnTerminateEntry = &pDelegateTo->mp_OnTerminate[fg_Move(OnTerminate)];
					}
				)
			;
		}
		
		CDelegatedActorHolder::~CDelegatedActorHolder()
		{
			if (!mp_pOnTerminateEntry)
				return;
			auto pDelegateTo = mp_pDelegateTo.f_Lock();
			if (!pDelegateTo)
				return;
			pDelegateTo->f_QueueProcess
				(
					[pOnTerminateEntry = mp_pOnTerminateEntry, pDelegateTo]
					{
						pDelegateTo->mp_OnTerminate.f_Remove(*pOnTerminateEntry);
					}
				)
			;
		}
		
		void CDelegatedActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame)
		{
			auto pDelegateTo = mp_pDelegateTo.f_Lock();
			if (pDelegateTo)
			{
				pDelegateTo->f_QueueProcess
					(
						[pThisWeak = TCActorHolderWeakPointer<CDelegatedActorHolder>{this}, Functor = fg_Move(_Functor)]() mutable
						{
							auto pThis = pThisWeak.f_Lock();
							if (pThis && pThis->mp_pActor)
								Functor();
						}
						, _bSame
					)
				;
			}
			else
				CDefaultActorHolder::f_QueueProcess(fg_Move(_Functor), _bSame);
		}

		///
		/// CDefaultActorHolder
		///

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
			f_TransferThreadSafeQueue();
			
			while (CQueueEntry *pEntry = mp_LocalQueue.f_Pop())
				delete pEntry;
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
		
		bool CConcurrentRunQueue::f_AddToQueueLocal(FActorQueueDispatch &&_Functor)
		{
			bool bWasEmpty = mp_LocalQueue.f_IsEmpty();
			NPtr::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
			pNewEntryUnique->m_Link.f_Construct();
			mp_LocalQueue.f_Insert(pNewEntryUnique.f_Detach());
			return bWasEmpty;
		}
		
		bool CConcurrentRunQueue::f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor)
		{
			bool bWasEmpty = mp_LocalQueue.f_IsEmpty();
			NPtr::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
			pNewEntryUnique->m_Link.f_Construct();
			mp_LocalQueue.f_InsertFirst(pNewEntryUnique.f_Detach());
			return bWasEmpty;
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
				TCActorHolderSharedPointer<CDefaultActorHolder> pThis = fg_Explicit(this);
				
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

