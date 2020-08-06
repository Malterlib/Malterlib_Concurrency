 // Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	ICDistributedActorData::~ICDistributedActorData()
	{
	}

	static constexpr mint gc_NoFixedCore = DMibBitRangeTyped(0, sizeof(mint)*8 - 4, mint);

	CActorHolder::CActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: mp_pConcurrencyManager(_pConcurrencyManager)
		, mp_bImmediateDelete(_bImmediateDelete)
		, mp_Priority(_Priority)
		, mp_iFixedCore(gc_NoFixedCore)
		, mp_pDistributedActorData(fg_Move(_pDistributedActorData))
	{
		_pConcurrencyManager->fp_AddedActor();
#if DMibConfig_Concurrency_DebugBlockDestroy
		{
			DMibLock(_pConcurrencyManager->m_ActorListLock);
			_pConcurrencyManager->m_Actors.f_Insert(this);
		}
#endif
	}

	CActorHolder::~CActorHolder()
	{
#if DMibConfig_Concurrency_DebugBlockDestroy
		DMibLock(mp_pConcurrencyManager->m_ActorListLock);
		m_ActorLink.f_Unlink();
#endif
		mp_pConcurrencyManager->fp_RemovedActor();
	}

#if DMibConfig_Tests_Enable
	void CActorHolder::f_TestDetach()
	{
		mp_pActor.f_Detach();
	}
#endif

	CDistributedActorHostInfo CActorHolder::f_GetHostInfo()
	{
		if (!mp_pDistributedActorData)
			return {};
		return mp_pDistributedActorData->f_GetHostInfo();
	}

	NStorage::TCSharedPointer<ICDistributedActorData> &CActorHolder::f_GetDistributedActorData()
	{
		return mp_pDistributedActorData;
	}

	NStorage::TCSharedPointer<ICDistributedActorData> const &CActorHolder::f_GetDistributedActorData() const
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

	void CActorHolder::fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory)
	{
		bool bNeedKickstart = false;
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();

			mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
			DMibFastCheck((OriginalWorking & gc_ProcessingMask) == 0);
			fp_StartQueueProcessing();

			auto pOldActorHalder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
#	if DMibEnableSafeCheck > 0
			auto pOldConstructing = ThreadLocal.m_pCurrentlyConstructingActor;
#endif
			auto pOldActor = ThreadLocal.m_pCurrentActor;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = this;
			bool bCurrentlyProcessingInActorHolder = ThreadLocal.m_bCurrentlyProcessingInActorHolder;
			ThreadLocal.m_bCurrentlyProcessingInActorHolder = true;
#	if DMibEnableSafeCheck > 0
			ThreadLocal.m_pCurrentlyConstructingActor = (CActor *)_pActorMemory;
#endif
			auto CleanupWorking = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHalder;
#	if DMibEnableSafeCheck > 0
					ThreadLocal.m_pCurrentlyConstructingActor = pOldConstructing;
#endif
					ThreadLocal.m_pCurrentActor = pOldActor;
					ThreadLocal.m_bCurrentlyProcessingInActorHolder = bCurrentlyProcessingInActorHolder;

					mint NewWorking = mp_Working.f_Exchange(0);
					if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
						fp_QueueProcess([]{}); // Reschedule
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

			bNeedKickstart = !mp_ConcurrentRunQueue.f_IsEmpty(mp_ConcurrentRunQueueLocal);
		}

		if (bNeedKickstart)
			fp_QueueProcess([]{});
	}

	bool CActorHolder::fp_AddToQueue(FActorQueueDispatch &&_Functor)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
		{
			mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal);
			return false;
		}
		else if (ThreadLocal.m_pThisQueue && mp_iFixedCore == ThreadLocal.m_pThisQueue->m_iQueue && ThreadLocal.m_pThisQueue->m_Priority == mp_Priority)
		{
			if (mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal))
			{
				mint Value = mp_Working.f_FetchAdd(1);
				return Value == 0;
			}
			return false;
		}
		mp_ConcurrentRunQueue.f_AddToQueue(fg_Move(_Functor), mp_ConcurrentRunQueueLocal);

		mint Value = mp_Working.f_FetchAdd(1);
		return Value == 0;
	}

	bool CActorHolder::fp_DequeueProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (auto *pJob = mp_ConcurrentRunQueue.f_FirstQueueEntry(mp_ConcurrentRunQueueLocal))
		{
			if (mp_pActor != nullptr)
				(*pJob)();
			mp_ConcurrentRunQueue.f_PopQueueEntry(pJob, mp_ConcurrentRunQueueLocal);
			return !!_ThreadLocal.m_pCurrentlyProcessingActorHolder;
		}
		return false;
	}

	void CActorHolder::fp_RunProcess()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		auto pOldHolder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
		ThreadLocal.m_pCurrentlyProcessingActorHolder = this;
		bool bCurrentlyProcessingInActorHolder = ThreadLocal.m_bCurrentlyProcessingInActorHolder;
		ThreadLocal.m_bCurrentlyProcessingInActorHolder = true;

		auto Cleanup = g_OnScopeExit > [&]
			{
				ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldHolder;
				ThreadLocal.m_bCurrentlyProcessingInActorHolder = bCurrentlyProcessingInActorHolder;
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
					if (mp_ConcurrentRunQueue.f_TransferThreadSafeQueue(mp_ConcurrentRunQueueLocal))
						bDoneSomething = true;
					while (fp_DequeueProcess(ThreadLocal))
						bDoneSomething = true;

					if (!ThreadLocal.m_pCurrentlyProcessingActorHolder)
					{
						UnLock.f_Clear();
						return;
					}
				}
			}
		}
	}

	bool CActorHolder::f_ImmediateDelete() const
	{
		return mp_bImmediateDelete;
	}

	void CActorHolder::fp_DestroyThreaded()
	{
	}

	bool CActorHolder::f_IsDestroyed() const
	{
		return mp_bDestroyed.f_Load() != 0;
	}

	bool CActorHolder::f_IsProcessedOnActorHolder(CActorHolder const *_pActorHolder) const
	{
		return this == _pActorHolder;
	}

	bool CActorHolder::f_IsCurrentActorAndProcessing() const
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		if (!mp_pActor || !ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return false;

		if (ThreadLocal.m_pCurrentActor)
		{
			if (ThreadLocal.m_pCurrentActor != mp_pActor.f_Get())
				return false;

			return ThreadLocal.m_pCurrentActor->self.m_pThis->f_IsProcessedOnActorHolder(ThreadLocal.m_pCurrentlyProcessingActorHolder);
		}

		if (this == ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return true;

		return false;
	}

#if DMibEnableSafeCheck > 0
	bool CActorHolder::f_CurrentlyProcessing() const
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		if (ThreadLocal.m_pCurrentActor == mp_pActor.f_Get())
			return true;
		if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
			return true;
		if (ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder == this)
			return true;
		if (ThreadLocal.m_pCurrentlyDestructingActor == mp_pActor.f_Get())
			return true;

		return false;
	}
#endif

	bool CActorHolder::fp_Terminate()
	{
		smint Expected = 0;
		if (mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
		{
			fp_DestroyActorHolder(nullptr, fg_Explicit(this));
			return true;
		}
		return false;
	}

	inline_always auto CActorHolder::fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &_Promise)
	{
		return [pActorInternal = fg_Move(_pActorHolder), _Promise](TCAsyncResult<void> &&_Result) mutable
			{
				auto &ActorInternal = *pActorInternal;
				if (smint Expected = 0; ActorInternal.mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
				{
#if DMibEnableSafeCheck > 0
					ActorInternal.mp_pActor->fp_CheckDestroy();
#endif

					ActorInternal.fp_DestroyActorHolder
						(
#if defined(DCompiler_MSVC) || DMibConfig_RefcountDebugging
							NFunction::TCFunctionSmallMutable<void ()>
							(
#endif
								[_Promise = fg_Move(_Promise), Result = _Result]() mutable
								{
									_Promise.f_SetResult(fg_Move(Result));
								}
#if defined(DCompiler_MSVC) || DMibConfig_RefcountDebugging
							)
#endif
							, fg_Move(pActorInternal)
						)
					;
				}
				else
					_Promise.f_SetResult(fg_Move(_Result));
			}
		;
	}

	TCFuture<void> CActorCommon::f_Destroy() &
	{
		TCPromise<void> Promise;
		(static_cast<TCActor<> &>(*this))(&CActor::fp_DestroyInternal) > NPrivate::fg_DirectResultActor()
			/ CActorHolder::fsp_DestroyHandler((static_cast<TCActor<> &>(*this)).m_pInternalActor, Promise)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<void> CActorCommon::f_Destroy() &&
	{
		TCPromise<void> Promise;
		auto pActorInternal = (static_cast<TCActor<> &>(*this)).m_pInternalActor;
		(static_cast<TCActor<> &&>(*this))(&CActor::fp_DestroyInternal) > NPrivate::fg_DirectResultActor() / CActorHolder::fsp_DestroyHandler(fg_Move(pActorInternal), Promise);
		return Promise.f_MoveFuture();
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

				fg_Move(pActor).f_Destroy() > NPrivate::fg_DirectResultActor() / [&](TCAsyncResult<void> &&_Result)
					{
						DMibLock(ResultLock);
						Result = fg_Move(_Result);
						Finished.f_Exchange(1);
						_EventLoop.m_fWake();
					}
				;

				while (Finished.f_Load() != 1)
					_EventLoop.m_fProcess();
			}
			else
			{
				align_cacheline NAtomic::TCAtomic<smint> Finished;
				NThread::CEventAutoReset Event;

				fg_Move(pActor).f_Destroy() > NPrivate::fg_DirectResultActor() / [&](TCAsyncResult<void> &&_Result)
					{
						DMibLock(ResultLock);
						Result = fg_Move(_Result);
						Finished.f_Exchange(1);
						Event.f_Signal();
					}
				;

				while (Finished.f_Load() != 1)
					Event.f_Wait();
			}
		}

		{
			DMibLock(ResultLock);
			Result.f_Get();
		}
	}

	struct CActorHolder::CDestroyHandler
	{
		CDestroyHandler
			(
				NFunction::TCFunctionNoAllocMovable<void ()> &&_fOnDestroyed
				, CActorHolder *_pThis
				, TCActorHolderSharedPointer<CActorHolder> &&_pSelfReference
			)
			: m_fOnDestroyed(fg_Move(_fOnDestroyed))
			, m_pThis(_pThis)
			, m_pSelfReference(fg_Move(_pSelfReference))
		{
		}

		CDestroyHandler(CDestroyHandler const &_Other) = delete;

		CDestroyHandler(CDestroyHandler &&_Other)
			: m_fOnDestroyed(fg_Move(_Other.m_fOnDestroyed))
			, m_pThis(_Other.m_pThis)
			, m_pSelfReference(fg_Move(_Other.m_pSelfReference))
			, m_bIsValid(fg_Exchange(_Other.m_bIsValid, false))
		{
		}

		void operator () ()
		{
		}

		~CDestroyHandler()
		{
			if (!m_bIsValid)
				return;

			auto &This = *m_pThis;

			while (auto pOnTerminate = This.mp_OnTerminate.f_FindAny())
			{
				pOnTerminate->m_fOnTerminate();
				This.mp_OnTerminate.f_Remove(pOnTerminate);
			}

			auto &ThreadLocal = fg_ConcurrencyThreadLocal();

			auto pActor = This.mp_pActor.f_Get();
			if (pActor)
			{
#if DMibEnableSafeCheck > 0

				auto pOldDestructingActor = ThreadLocal.m_pCurrentlyDestructingActor;
				ThreadLocal.m_pCurrentlyDestructingActor = pActor;
				auto Cleanup = g_OnScopeExit > [&]
					{
						ThreadLocal.m_pCurrentlyDestructingActor = pOldDestructingActor;
					}
				;
#endif
				pActor->self.m_pThis.f_SetBits(1);
				pActor->fp_AbortSuspendedCoroutines();
				This.mp_pActor.f_Clear();
			}
			This.mp_bDestroyed.f_Exchange(2);

			bool bShouldDelete;
			bool bWasSelfRef = false;

			if (m_pSelfReference)
			{
				bWasSelfRef = true;
				auto pLastDestructing = ThreadLocal.m_pCurrentlyDestructingActorHolder;
				ThreadLocal.m_pCurrentlyDestructingActorHolder = &This;
				auto Cleanup = g_OnScopeExit > [&]
					{
						ThreadLocal.m_pCurrentlyDestructingActorHolder = pLastDestructing;
					}
				;

				bShouldDelete = m_pSelfReference.f_Clear();
			}
			else
				bShouldDelete = true;

			if (bShouldDelete)
			{
				if (ThreadLocal.m_pThisQueue)
				{
					if (ThreadLocal.m_pCurrentlyProcessingActorHolder == &This)
					{
						ThreadLocal.m_pCurrentlyProcessingActorHolder = nullptr;
						This.mp_ConcurrentRunQueue.f_RemoveAllExceptFirst(This.mp_ConcurrentRunQueueLocal);
					}
					else
						This.mp_ConcurrentRunQueue.f_RemoveAll(This.mp_ConcurrentRunQueueLocal);

					NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(&This);
					This.f_WeakRefCountSetCapturedDelete(CapturedDelete);
					if (This.f_WeakRefCountDecrease(DMibRefcountDebuggingOnly(nullptr)) == 0)
					{
						if (CapturedDelete.m_Size)
							CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
						else
							CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
					}

					if (m_fOnDestroyed)
						m_fOnDestroyed();
					return;
				}

				// Dispatch to our queue again, otherwise we ourselves could be in callstack
				This.mp_pConcurrencyManager->f_DispatchOnCurrentThreadOrConcurrentFirst
					(
						This.f_GetPriority()
						, g_OnScopeExit > [pThis = &This, fOnDestroyed = fg_Move(m_fOnDestroyed)]() mutable
						{
							auto &This = *pThis;
							NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(&This);
							This.f_WeakRefCountSetCapturedDelete(CapturedDelete);
							if (This.f_WeakRefCountDecrease(DMibRefcountDebuggingOnly(nullptr)) == 0)
							{
								if (CapturedDelete.m_Size)
									CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
								else
									CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
							}

							if (fOnDestroyed)
								fOnDestroyed();
						}
					)
				;
			}
			else
			{
				if (m_fOnDestroyed)
					m_fOnDestroyed();
				return;
			}
		}

		NFunction::TCFunctionNoAllocMovable<void ()> m_fOnDestroyed;
		CActorHolder *m_pThis;
		TCActorHolderSharedPointer<CActorHolder> m_pSelfReference;
		bool m_bIsValid = true;
	};

	void CActorHolder::fp_DestroyActorHolder
		(
		 	NFunction::TCFunctionNoAllocMovable<void ()> &&_fOnDestroyed
		 	, TCActorHolderSharedPointer<CActorHolder> &&_pSelfReference
		)
	{
		fp_QueueProcessDestroy(CDestroyHandler(fg_Move(_fOnDestroyed), this, fg_Move(_pSelfReference)));
	}

	///
	/// CSeparateThreadActorHolder
	///

	CSeparateThreadActorHolder::CSeparateThreadActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
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
							fp_RunProcess();
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

	void CSeparateThreadActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		if (fp_AddToQueue(fg_Move(_Functor)))
			m_pThread->m_EventWantQuit.f_Signal();
	}

	void CSeparateThreadActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		// Reference this so it doesn't go out of scope if queue is processed before thread has been notified
		TCActorHolderSharedPointer<CSeparateThreadActorHolder> pThis = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor)))
			m_pThread->m_EventWantQuit.f_Signal();
	}

	void CSeparateThreadActorHolder::fp_DestroyThreaded()
	{
		m_pThread.f_Clear();
		CDefaultActorHolder::fp_DestroyThreaded();
	}

	///
	/// CDispatchingActorHolder
	///

	CDispatchingActorHolder::CDispatchingActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			, NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> &&_Dispatcher
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		, m_Dispatcher(fg_Move(_Dispatcher))
	{
	}

	void CDispatchingActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		if (fp_AddToQueue(fg_Move(_Functor)))
		{
			m_Dispatcher
				(
					[this]() mutable
					{
						fp_RunProcess();
					}
				)
			;
		}
	}

	void CDispatchingActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		TCActorHolderSharedPointer<CDispatchingActorHolder> pThis = fg_Explicit(this);
		if (fp_AddToQueue(fg_Move(_Functor)))
		{
			m_Dispatcher
				(
					[pThis = fg_Move(pThis)]() mutable
					{
						pThis->fp_RunProcess();
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
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
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
					OnTerminate.m_fOnTerminate = [pThisWeak = pThis->fp_GetAsActor<CActor>().f_Weak(), pThisHolder = pThis.f_Get()]
						{
							auto pThis = pThisWeak.f_Lock();
							if (!pThis)
								return;
							pThisHolder->mp_pOnTerminateEntry = nullptr;
							fg_Move(pThis).f_Destroy() > fg_DiscardResult();
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

		mp_pDelegateTo->f_QueueProcess
			(
				[pOnTerminateEntry = mp_pOnTerminateEntry, pDelegateTo = mp_pDelegateTo]
				{
					pDelegateTo->mp_OnTerminate.f_TryRemovePointerBasedComparison(pOnTerminateEntry);
				}
			)
		;
	}

	bool CDelegatedActorHolder::f_IsProcessedOnActorHolder(CActorHolder const *_pActorHolder) const
	{
		if (this == _pActorHolder)
			return true;

		return mp_pDelegateTo->f_IsProcessedOnActorHolder(_pActorHolder);
	}

#if DMibEnableSafeCheck > 0
	bool CDelegatedActorHolder::f_CurrentlyProcessing() const
	{
		return mp_pDelegateTo->f_CurrentlyProcessing();
	}
#endif

	void CDelegatedActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		mp_pDelegateTo->f_QueueProcessDestroy
			(
				[this, Functor = fg_Move(_Functor)]() mutable
				{
					if (this->mp_pActor)
						Functor();
				}
			)
		;
	}

	void CDelegatedActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		mp_pDelegateTo->f_QueueProcess
			(
				[pThisWeak = TCActorHolderWeakPointer<CDelegatedActorHolder>{this}, Functor = fg_Move(_Functor)]() mutable
				{
					auto pThis = pThisWeak.f_Lock();
					if (pThis && pThis->mp_pActor)
						Functor();
				}
			)
		;
	}

	///
	/// CDefaultActorHolder
	///

	CDefaultActorHolder::CDefaultActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
	}

	///
	/// CDirectCallActorHolder
	///

	CDirectCallActorHolder::CDirectCallActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
	}

	CDirectCallActorHolder::~CDirectCallActorHolder()
	{
	}

	void CDirectCallActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		_Functor();
	}

	void CDirectCallActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		_Functor();
	}

	///
	/// CShamActorHolder
	///

	CShamActorHolder::CShamActorHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
	}

	CShamActorHolder::~CShamActorHolder()
	{
	}

	void CShamActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		_Functor();
	}

	void CShamActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		DMibFastCheck(false); // Should never be used directly
	}

	///

	CConcurrentRunQueue::CConcurrentRunQueue()
	{
		static_assert(sizeof(CQueueEntry) == gc_ActorQueueDispatchFunctionMemory);
		static_assert((sizeof(CQueueEntry::m_Link) + sizeof(CQueueEntry::m_fToCall)) == gc_ActorQueueDispatchFunctionMemory);
	}

	CConcurrentRunQueue::CLocalQueueData::~CLocalQueueData()
	{
		while (CQueueEntry *pEntry = m_LocalQueue.f_Pop())
			delete pEntry;
	}

	CConcurrentRunQueue::~CConcurrentRunQueue()
	{
		CQueueEntry *pEntry = mp_pFirstQueued.f_Exchange(nullptr);
		if (!pEntry)
			return;

		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);
			pEntry->m_Link.f_Construct();
			delete pEntry;
			pEntry = pNextEntry;
		}
	}

	void CConcurrentRunQueue::f_AddToQueue(FActorQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue)
	{
		NStorage::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		CQueueEntry *pNewEntry = pNewEntryUnique.f_Detach();
		while (true)
		{
			CQueueEntry *pFirstEntry = mp_pFirstQueued.f_Load();
			pNewEntry->m_pNextQueued = pFirstEntry;
			if (mp_pFirstQueued.f_CompareExchangeStrong(pFirstEntry, pNewEntry))
				break;
		}
	}

	bool CConcurrentRunQueue::f_AddToQueueLocal(FActorQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_Insert(pNewEntryUnique.f_Detach());
		return bWasEmpty;
	}

	bool CConcurrentRunQueue::f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue)
	{
		bool bWasEmpty = _LocalQueue.m_LocalQueue.f_IsEmpty();
		NStorage::TCUniquePointer<CQueueEntry> pNewEntryUnique = fg_Construct(fg_Move(_Functor));
		pNewEntryUnique->m_Link.f_Construct();
		_LocalQueue.m_LocalQueue.f_InsertFirst(pNewEntryUnique.f_Detach());
		return bWasEmpty;
	}

	void CConcurrentRunQueue::f_RemoveAllExceptFirst(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (CQueueEntry *pEntry = _LocalQueue.m_LocalQueue.f_GetLast())
		{
			if (pEntry->m_Link.f_IsAloneInList())
				break;
			delete pEntry;
		}
	}

	void CConcurrentRunQueue::f_RemoveAll(CLocalQueueData &_LocalQueue)
	{
		f_TransferThreadSafeQueue(_LocalQueue);

		while (CQueueEntry *pEntry = _LocalQueue.m_LocalQueue.f_Pop())
			delete pEntry;
	}

	FActorQueueDispatch *CConcurrentRunQueue::f_FirstQueueEntry(CLocalQueueData &_LocalQueue)
	{
		if (_LocalQueue.m_LocalQueue.f_IsEmpty())
			return nullptr;
		return &_LocalQueue.m_LocalQueue.f_GetFirst()->m_fToCall;
	}

	CConcurrentRunQueue::CQueueEntry::CQueueEntry(FActorQueueDispatch &&_fToCall)
		: m_fToCall(fg_Move(_fToCall))
	{
	}

	CConcurrentRunQueue::CQueueEntry::~CQueueEntry()
	{
		m_Link.f_Unlink();
	}

	void CConcurrentRunQueue::f_PopQueueEntry(FActorQueueDispatch *_pEntry, CLocalQueueData &_LocalQueue)
	{
		mint Offset = DMibPOffsetOf(CQueueEntry, m_fToCall);
		CQueueEntry *pEntry = (CQueueEntry *)((uint8 *)_pEntry - Offset);
		delete pEntry;
	}

	bool CConcurrentRunQueue::f_OneOrLessInQueue(CLocalQueueData &_LocalQueue)
	{
		return mp_pFirstQueued.f_Load() == nullptr && (_LocalQueue.m_LocalQueue.f_IsEmpty() || _LocalQueue.m_LocalQueue.f_GetFirst()->m_Link.f_IsAloneInList());
	}

	bool CConcurrentRunQueue::f_IsEmpty(CLocalQueueData &_LocalQueue)
	{
		return mp_pFirstQueued.f_Load() == nullptr && _LocalQueue.m_LocalQueue.f_IsEmpty();
	}

	bool CConcurrentRunQueue::f_TransferThreadSafeQueue(CLocalQueueData &_LocalQueue)
	{
		CQueueEntry *pEntry = mp_pFirstQueued.f_Exchange(nullptr);
		if (!pEntry)
			return false;

		CQueueEntry *pInsertAfter = nullptr;
		if (!_LocalQueue.m_LocalQueue.f_IsEmpty())
			pInsertAfter = _LocalQueue.m_LocalQueue.f_GetLast();
		while (pEntry)
		{
			auto *pNextEntry = pEntry->m_pNextQueued.f_Load(NAtomic::EMemoryOrder_Relaxed);

			pEntry->m_Link.f_Construct();
			if (pInsertAfter)
				_LocalQueue.m_LocalQueue.f_InsertAfter(pEntry, pInsertAfter);
			else
				_LocalQueue.m_LocalQueue.f_InsertFirst(pEntry);

			pEntry = pNextEntry;
		}
		return true;
	}

	CDefaultActorHolder::~CDefaultActorHolder()
	{
	}

	void CDefaultActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		if (fp_AddToQueue(fg_Move(_Functor)))
		{
			mp_pConcurrencyManager->f_DispatchOnCurrentThreadOrConcurrent
				(
					this->f_GetPriority()
					, [this]() // This referenced in _Fuctor
					{
						this->fp_RunProcess();
					}
				)
			;
		}
	}

	void CDefaultActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		if (fp_AddToQueue(fg_Move(_Functor)))
		{
			TCActorHolderSharedPointer<CDefaultActorHolder> pThis = fg_Explicit(this);
			mp_pConcurrencyManager->fp_QueueJob
				(
					this->f_GetPriority()
					, this->mp_iFixedCore
					, [pThis = fg_Move(pThis)]()
					{
						pThis->fp_RunProcess();
					}
				)
			;
		}
	}

	void CActorHolder::f_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		fp_QueueProcessDestroy(fg_Move(_Functor));
	}

	void CActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		fp_QueueProcess(fg_Move(_Functor));
	}
}
