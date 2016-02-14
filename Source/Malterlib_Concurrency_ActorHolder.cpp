 // Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{

		CActorHolder::CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority)
			: mp_pConcurrencyManager(_pConcurrencyManager)
			, mp_bImmediateDelete(_bImmediateDelete)
			, mp_Priority(_Priority)
			, mp_iFixedCore(DMibBitRangeTyped(0, sizeof(mint)*8 - 2, mint))
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
		
		void CActorHolder::f_SetFixedCore(mint _iFixedCore)
		{
			mp_iFixedCore = _iFixedCore;
		}
		
		void CActorHolder::fp_Construct()
		{
			NPrivate::CCurrentActorScope CurrentActor(*mp_pConcurrencyManager, mp_pActor.f_Get());
			mp_pActor->f_Construct();
		}
		
		constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);
		
		bool CActorHolder::fp_AddToQueue(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			auto &ThreadLocal = *mp_pConcurrencyManager->m_ThreadLocal;
			
			if (ThreadLocal.m_pCurrentActor == this->mp_pActor.f_Get())
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
				mp_ConcurrentRunQueue.f_PopFirstQueueEntry();
				return true;
			}
			return false;
		}
		
		void CActorHolder::f_RunProcess()
		{
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

		void CActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			return fp_QueueProcess(fg_Move(_Functor));
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
				> fg_ConcurrentActor() / [pActor](TCAsyncResult<void> &&_Result)
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
							if (f_RefCountDecrease() == 1)
							{
								// Remove actor holder on concurrent actor, otherwise we ourselves could be in callstack
								fg_ConcurrentActor()
									(
										&CActor::f_Dispatch
										, [pToDelete]
										{
										}
									) 
									> fg_DiscardResult()
								;
							}
						}
					}
				)
			;
			auto OnExitLambda = fg_LambdaMove(fg_Move(OnExit));
			f_QueueProcess
				(
					[OnExitLambda]()
					{
					}
				)
			;
		}
		

		///
		/// CSeparateThreadActorHolder
		///
		CSeparateThreadActorHolder::CSeparateThreadActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NStr::CStr const &_ThreadName)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority)
			, mp_ThreadName(_ThreadName)
		{

		}

		CSeparateThreadActorHolder::~CSeparateThreadActorHolder()
		{
			DMibRequire(m_pThread.f_IsEmpty());
		}
		void CSeparateThreadActorHolder::fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcess(fg_Move(_Functor));
		}
		void CSeparateThreadActorHolder::fp_Construct()
		{
			m_pThread = NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
						{
							f_RunProcess();
							_pThread->m_EventWantQuit.f_Wait();
						}
						return 0;
					}
					, mp_ThreadName
				)
			;
			CDefaultActorHolder::fp_Construct();
		}
		void CSeparateThreadActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
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
		
		CDispatchingActorHolder::CDispatchingActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> const &_Dispatcher)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority)
			, m_Dispatcher(_Dispatcher)
		{
		}

		void CDispatchingActorHolder::fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcess(fg_Move(_Functor));
		}
		
		void CDispatchingActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
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

		void CDefaultActorHolder::fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcess(fg_Move(_Functor));
		}
		
		void CDefaultActorHolder::fp_Construct()
		{
			CActorHolder::fp_Construct();
		}			
		CDefaultActorHolder::CDefaultActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority)
			: CActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority)
		{
		}

		CConcurrentRunQueue::CConcurrentRunQueue()
		{
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
		
		void CConcurrentRunQueue::f_AddToQueue(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			CQueueEntry *pNewEntry = DMibNew CQueueEntry();
			pNewEntry->m_fToCall = fg_Move(_Functor);
			while (true)
			{
				CQueueEntry *pFirstEntry = mp_pFirstQueued.f_Load();
				pNewEntry->m_pNextQueued = pFirstEntry;
				if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry))
					break;
			}
		}
		
		void CConcurrentRunQueue::f_AddToQueueLocal(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
//			f_TransferThreadSafeQueue();
			mp_LocalQueue.f_Insert(fg_Move(_Functor));
		}
		
		NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> *CConcurrentRunQueue::f_FirstQueueEntry()
		{
			if (mp_LocalQueue.f_IsEmpty())
				return nullptr;
			return &mp_LocalQueue.f_GetFirst();
		}
		
		void CConcurrentRunQueue::f_PopFirstQueueEntry()
		{
			mp_LocalQueue.f_Remove(mp_LocalQueue.f_GetFirst());
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

			NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> *pInsertAfter = nullptr;
			if (!mp_LocalQueue.f_IsEmpty())
				pInsertAfter = &mp_LocalQueue.f_GetLast();
			while (pEntry)
			{
				auto *pNextEntry = pEntry->m_pNextQueued.f_Load();
				
				if (pInsertAfter)
				{
					mp_LocalQueue.f_InsertAfter(fg_Move(pEntry->m_fToCall), *pInsertAfter);
				}
				else
					mp_LocalQueue.f_InsertFirst(fg_Move(pEntry->m_fToCall));
				
				delete pEntry;
				
				pEntry = pNextEntry;
			}
			return true;
		}
		
		CDefaultActorHolder::~CDefaultActorHolder()
		{
		}

		void CDefaultActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			if (fp_AddToQueue(fg_Move(_Functor)))
			{
				NPtr::TCSharedPointer<CDefaultActorHolder, NPtr::CSupportWeakTag, CInternalActorAllocator> pThis = fg_Explicit(this);
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

