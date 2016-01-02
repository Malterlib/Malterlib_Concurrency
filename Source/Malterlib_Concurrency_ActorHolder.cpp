// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{

		CActorHolder::CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete)
			: m_pConcurrencyManager(_pConcurrencyManager)
			, mp_bImmediateDelete(_bImmediateDelete)
		{
		}
		
		CActorHolder::~CActorHolder()
		{
			DMibLock(m_pConcurrencyManager->m_ActorListLock);
			m_ActorLink.f_Unlink();
		}
		
		void CActorHolder::fp_Construct()
		{
			NPrivate::CCurrentActorScope CurrentActor(mp_pActor.f_Get());
			mp_pActor->f_Construct();
		}
	
		void CActorHolder::f_RunProcess()
		{
			bool bDoneSomething = true;
			while (bDoneSomething)
			{
				bDoneSomething = false;
				if (m_WorkLock.f_TryLock())
				{
					auto UnLock 
						= fg_OnScopeExit
						(
							[&]
							{
								m_WorkLock.f_Unlock();
							}
						)
					;

					while (f_DequeueProcess(mp_pActor != nullptr))
						bDoneSomething = true;
				}
			}
		}

		void CActorHolder::f_RunProcessConcurrent()
		{
			bool bDoneSomething = true;
			while (bDoneSomething)
			{
				bDoneSomething = false;
				{
					DMibLockRead(m_DeleteLock);
					if (mp_pActor)
					{
						while (f_DequeueProcessConcurrent(true))
							bDoneSomething = true;
					}
					else
					{
						while (f_DequeueProcessConcurrent(false))
							bDoneSomething = true;
					}
				}
			}
		}


		void CActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			return fp_QueueProcess(fg_Move(_Functor));
		}
		void CActorHolder::f_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			return fp_QueueProcessConcurrent(fg_Move(_Functor));
		}

		bool CActorHolder::f_DequeueProcess(bool _bRun)
		{
			return fp_DequeueProcess(_bRun);
		}

		bool CActorHolder::f_DequeueProcessConcurrent(bool _bRun)
		{
			return fp_DequeueProcessConcurrent(_bRun);
		}
		bool CActorHolder::f_QueueProcessIsEmpty() const
		{
			return fp_QueueProcessIsEmpty();
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
			return m_bDestroyed.f_Load() == 0;
		}

		
		void CActorHolder::fp_Terminate()
		{
			smint Expected = 0;
			if (m_bDestroyed.f_CompareExchangeStrong(Expected, 1))
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
			
			if (m_bDestroyed.f_Load() == 0)
			{
				{
					TCActor<CActor> pActor = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));

					if (_EventLoop.m_fProcess && _EventLoop.m_fWake)
					{
						NAtomic::TCAtomic<smint> Finished;
						pActor(&CActor::f_Destroy)
							> fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result)
							{
								{
									DMibLock(ResultLock);
									Result = fg_Move(_Result);
								}
								Finished.f_Exchange(1);
								_EventLoop.m_fWake();
							}
						;

						while (!Finished.f_Load())
						{
							_EventLoop.m_fProcess();
						}
					}
					else
					{
						NThread::CEventAutoReset Event;

						pActor(&CActor::f_Destroy)
							> fg_ConcurrentActor() / [&](TCAsyncResult<void> &&_Result)
							{
								{
									DMibLock(ResultLock);
									Result = fg_Move(_Result);
								}
								Event.f_Signal();
							}
						;

						Event.f_Wait();
					}
				}

				fp_Terminate();
				{
					DMibLock(ResultLock);
					Result.f_Get();
				}
			}

			DMibLock(m_WorkLock);
			{
				DMibLock(m_DeleteLock);
				if (mp_pActor)
				{
					mp_pActor.f_Clear();
					m_bDestroyed.f_Exchange(2);
					f_RefCountDecrease();
				}
			}
		}
	
		aint CActorHolder::f_RefCountDecrease()
		{
			// TODO: Investigate this when using weak actors
			aint Ret = CSuper::f_RefCountDecrease();
			if (Ret == 1)
			{
				smint Expected = 0;
				if (m_bDestroyed.f_CompareExchangeStrong(Expected, 1))
				{
					// Only our own reference is left, schedule destroy
					fp_Destroy();
				}
			}
			return Ret;
		}
		
		
		void CActorHolder::fp_Destroy()
		{
			auto fl_OnExit
				= fg_OnScopeExit
				(
					[this]()
					{
						DMibLock(m_DeleteLock);
						if (mp_pActor)
						{
							mp_pActor.f_Clear();
							m_bDestroyed.f_Exchange(2);
							TCActor<CActor> pToDelete = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));
							if (f_RefCountDecrease() == 1)
							{
								this->f_QueueProcessConcurrent
									(
										[pToDelete]()
										{
										}
									)
								;
							}
						}
					}
				)
			;
			auto fl_OnExitLambda = fg_LambdaMove(fg_Move(fl_OnExit));
			f_QueueProcess
				(
					[fl_OnExitLambda]()
					{
					}
				)
			;
		}
		

		///
		/// CSeparateThreadActorHolder
		///
		CSeparateThreadActorHolder::CSeparateThreadActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, NStr::CStr const &_ThreadName)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete)
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
			m_ActorQueue.f_Push(fg_Move(_Functor));
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
		
		CDispatchingActorHolder::CDispatchingActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, NFunction::TCFunction<void (NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Dispatch)> const &_Dispatcher)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete)
			, m_Dispatcher(_Dispatcher)
		{
		}

		void CDispatchingActorHolder::fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcess(fg_Move(_Functor));
		}
		
		void CDispatchingActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			m_ActorQueue.f_Push(fg_Move(_Functor));
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

		///
		/// CDefaultActorHolder
		///

		void CDefaultActorHolder::fp_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcess(fg_Move(_Functor));
		}
		void CDefaultActorHolder::fp_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			f_QueueProcessConcurrent(fg_Move(_Functor));
		}
		bool CDefaultActorHolder::fp_DequeueProcess(bool _bRun)
		{
			return f_DequeueProcess(_bRun);
		}
		bool CDefaultActorHolder::fp_DequeueProcessConcurrent(bool _bRun)
		{
			return f_DequeueProcessConcurrent(_bRun);
		}
		bool CDefaultActorHolder::fp_QueueProcessIsEmpty() const
		{
			return f_QueueProcessIsEmpty();
		}
		void CDefaultActorHolder::fp_Construct()
		{
			CActorHolder::fp_Construct();
		}			
		CDefaultActorHolder::CDefaultActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete)
			: CActorHolder(_pConcurrencyManager, _bImmediateDelete)
		{
		}
		
		void CDefaultActorHolder::f_QueueProcess(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			m_ActorQueue.f_Push(fg_Move(_Functor));
			NPtr::TCSharedPointer<CDefaultActorHolder, NPtr::CSupportWeakTag, CInternalActorAllocator> pThis = fg_Explicit(this);
			m_pConcurrencyManager->fp_QueueJob
				(
					[pThis]()
					{
						pThis->f_RunProcess();
					}						
				)
			;
		}
		void CDefaultActorHolder::f_QueueProcessConcurrent(NFunction::TCFunction<void (), NFunction::CFunctionNoCopyTag> &&_Functor)
		{
			m_ActorQueueConcurrent.f_Push(fg_Move(_Functor));
			NPtr::TCSharedPointer<CDefaultActorHolder, NPtr::CSupportWeakTag, CInternalActorAllocator> pThis = fg_Explicit(this);
			m_pConcurrencyManager->fp_QueueJob
				(
					[pThis]()
					{
						pThis->f_RunProcessConcurrent();
					}						
				)
			;
		}


		bool CDefaultActorHolder::f_DequeueProcess(bool _bRun)
		{
			if (auto pJob = m_ActorQueue.f_Pop())
			{
				if (_bRun)
					(*pJob)();
				return true;
			}
			return false;
		}

		bool CDefaultActorHolder::f_DequeueProcessConcurrent(bool _bRun)
		{
			if (auto pJob = m_ActorQueueConcurrent.f_Pop())
			{
				if (_bRun)
					(*pJob)();
				return true;
			}
			return false;
		}

		bool CDefaultActorHolder::f_QueueProcessIsEmpty() const
		{
			return m_ActorQueue.f_IsEmpty();
		}
		
	}
}

