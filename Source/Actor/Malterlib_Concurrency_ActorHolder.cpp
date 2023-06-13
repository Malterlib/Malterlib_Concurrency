 // Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	ICDistributedActorData::~ICDistributedActorData()
	{
	}

	static constexpr uint32 gc_NoFixedCore = TCLimitsInt<uint32>::mc_Max;

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

	constexpr static const mint gc_ProcessingMask = DMibBitTyped(sizeof(mint) * 8 - 1, mint);

	CActorHolder::~CActorHolder()
	{
#if DMibConfig_Concurrency_DebugBlockDestroy
		DMibLock(mp_pConcurrencyManager->m_ActorListLock);
		m_ActorLink.f_Unlink();
#endif
		mp_Working.f_Exchange(0);

		fp_DeleteActor();
		mp_pConcurrencyManager->fp_RemovedActor();
	}

#if DMibConfig_Tests_Enable
	void CActorHolder::f_TestDetach()
	{
		fp_DetachActor();
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

	void CActorHolder::f_Yield()
	{
		DMibFastCheck(fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder == this);
		mp_bYield = true;

		// Make sure that anything that has been scheduled at this point is before us in the queue
		mp_ConcurrentRunQueue.f_TransferThreadSafeQueue(mp_ConcurrentRunQueueLocal);
	}

	void CActorHolder::fp_StartQueueProcessing()
	{

	}

	void CActorHolder::fp_DeleteActor()
	{
		auto *pActor = mp_pActorUnsafe.f_Exchange(nullptr);
		if (pActor)
			pActor->~CActor();
	}

	void CActorHolder::fp_DetachActor()
	{
		mp_pActorUnsafe.f_Store(nullptr);
	}

	CActor *CActorHolder::fp_GetActorRelaxed() const
	{
		return mp_pActorUnsafe.f_Load(NAtomic::EMemoryOrder_Relaxed);
	}

	void CActorHolder::fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		bool bNeedKickstart = false;
		{
			mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
			DMibFastCheck((OriginalWorking & gc_ProcessingMask) == 0);
			fp_StartQueueProcessing();

			auto pOldActorHolder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
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
			auto CleanupWorking = g_OnScopeExit / [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHolder;
#	if DMibEnableSafeCheck > 0
					ThreadLocal.m_pCurrentlyConstructingActor = pOldConstructing;
#endif
					ThreadLocal.m_pCurrentActor = pOldActor;
					ThreadLocal.m_bCurrentlyProcessingInActorHolder = bCurrentlyProcessingInActorHolder;

					mint NewWorking = mp_Working.f_Exchange(0);
					if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
						fp_QueueProcess([]{}, ThreadLocal); // Reschedule
				}
			;

			auto CleanupConstruction = g_OnScopeExit / [&]
				{
					this->fp_DetachActor();
				}
			;

			_fConstruct();

			// Construction was successful
			CleanupConstruction.f_Clear();

			fp_GetActorRelaxed()->fp_Construct();

			bNeedKickstart = !mp_ConcurrentRunQueue.f_IsEmpty(mp_ConcurrentRunQueueLocal);
		}

		if (bNeedKickstart)
			fp_QueueProcess([]{}, ThreadLocal);
	}

	bool CActorHolder::fp_AddToQueue(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
		{
			mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal);
			return _ThreadLocal.m_bForceNonLocal;
		}
		else if (_ThreadLocal.m_pThisQueue && mp_iFixedCore == _ThreadLocal.m_pThisQueue->m_iQueue && _ThreadLocal.m_pThisQueue->m_Priority == mp_Priority)
		{
			if (mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal))
			{
				mint Value = mp_Working.f_FetchAdd(1);
				return Value == 0;
			}
			return _ThreadLocal.m_bForceNonLocal;
		}
		mp_ConcurrentRunQueue.f_AddToQueue(fg_Move(_Functor));

		mint Value = mp_Working.f_FetchAdd(1);
		return Value == 0;
	}

	bool CActorHolder::fp_DequeueProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
#if DMibEnableSafeCheck > 0
		auto OriginalExceptions = NException::fg_UncaughtExceptions();
#endif
		if (auto *pJob = mp_ConcurrentRunQueue.f_FirstQueueEntry(mp_ConcurrentRunQueueLocal))
		{
#if DMibEnableSafeCheck > 0
			auto Cleanup = g_OnScopeExit / [&]
				{
					DMibFastCheck(NException::fg_UncaughtExceptions() == OriginalExceptions); // Unwinding exceptions through queue processing is not supported
				}
			;
#endif
			if (fp_GetActorRelaxed() != nullptr)
				(*pJob)();
			mp_ConcurrentRunQueue.f_PopQueueEntry(pJob);
			return !!_ThreadLocal.m_pCurrentlyProcessingActorHolder;
		}
		return false;
	}

	void CActorHolder::fp_RunProcess()
	{
		bool bDoMore = true;
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		{
			auto pOldHolder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = this;
			bool bCurrentlyProcessingInActorHolder = ThreadLocal.m_bCurrentlyProcessingInActorHolder;
			ThreadLocal.m_bCurrentlyProcessingInActorHolder = true;

			auto Cleanup = g_OnScopeExit / [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldHolder;
					ThreadLocal.m_bCurrentlyProcessingInActorHolder = bCurrentlyProcessingInActorHolder;
				}
			;

			while (bDoMore)
			{
				bDoMore = false;
				mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
				if ((OriginalWorking & gc_ProcessingMask) == 0)
				{
					auto UnLock = fg_OnScopeExit
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
						while (fp_DequeueProcess(ThreadLocal) && !mp_bYield)
							bDoneSomething = true;

						if (!ThreadLocal.m_pCurrentlyProcessingActorHolder)
						{
							UnLock.f_Clear();
							return;
						}

						if (mp_bYield) [[unlikely]]
						{
							mp_bYield = false;

							UnLock.f_Clear();
							mp_Working.f_FetchAnd(~gc_ProcessingMask);

							ThreadLocal.m_bForceNonLocal = true;
							fp_QueueProcess([]{}, ThreadLocal);
							ThreadLocal.m_bForceNonLocal = false;

							return;
						}
					}
				}
			}
		}
	}

	bool CActorHolder::f_ImmediateDelete() const
	{
		return mp_bImmediateDelete;
	}

	bool CActorHolder::f_IsAlwaysAlive() const
	{
		return mp_bIsAlwaysAlive;
	}

	void CActorHolder::fp_DestroyThreaded()
	{
	}

	bool CActorHolder::f_IsHolderDestroyed() const
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

		auto pActor = fp_GetActorRelaxed();

		if (!pActor || !ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return false;

		if (ThreadLocal.m_pCurrentActor)
		{
			if (ThreadLocal.m_pCurrentActor != pActor)
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

		auto pActor = fp_GetActorRelaxed();

		if (ThreadLocal.m_pCurrentActor == pActor)
			return true;
		if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this)
			return true;
		if (ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder == this)
			return true;
		if (ThreadLocal.m_pCurrentlyDestructingActor == pActor)
			return true;

		return false;
	}
#endif

	bool CActorHolder::fp_Terminate()
	{
		smint Expected = 0;
		if (mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
		{
			fp_DestroyActorHolder(nullptr, fg_Explicit(this), fg_ConcurrencyThreadLocal());
			return true;
		}
		return false;
	}

	inline_always auto CActorHolder::fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &_Promise)
	{
		DMibFastCheck(_pActorHolder != nullptr);

		return [pActorInternal = fg_Move(_pActorHolder), _Promise](TCAsyncResult<void> &&_Result) mutable
			{
				auto &ActorInternal = *pActorInternal;
				if (smint Expected = 0; ActorInternal.mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
				{
#if DMibEnableSafeCheck > 0
					if (auto *pActor = ActorInternal.fp_GetActorRelaxed())
						pActor->fp_CheckDestroy();
#endif

					ActorInternal.fp_DestroyActorHolder
						(
#if defined(DCompiler_MSVC) || DMibConfig_RefCountDebugging
							NFunction::TCFunctionSmallMutable<void ()>
							(
#endif
								[_Promise = fg_Move(_Promise), Result = fg_Move(_Result)]() mutable
								{
									_Promise.f_SetResult(fg_Move(Result));
								}
#if defined(DCompiler_MSVC) || DMibConfig_RefCountDebugging
							)
#endif
							, fg_Move(pActorInternal)
							, fg_ConcurrencyThreadLocal()
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

	inline_never bool CActorHolder::fsp_ScheduleActorDestroy(CActorHolder *_pActorHolder)
	{
		DMibFastCheck(!_pActorHolder->f_IsAlwaysAlive());
		DMibFastCheck(!_pActorHolder->f_ImmediateDelete());

		if (!_pActorHolder->mp_bHasOverriddenDestroy)
			return false;

		auto pActor = _pActorHolder->fp_GetActorRelaxed();

		if (!pActor)
			return false;

		if (pActor->f_IsDestroyed())
			return false;

		TCActorHolderSharedPointer<CActorHolder> pSelfReference;
		{
			DMibRefCountDebuggingOnly(NStorage::CRefCountDebugReference TempRef);
			_pActorHolder->m_RefCount.f_Increase
				(
#if DMibConfig_RefCountDebugging
#	if DMibEnableSafeCheck > 0
					TempRef, true
#	else
					TempRef
#	endif
#else
#	if DMibEnableSafeCheck > 0
					true
#	endif
#endif
				)
			;

			DMibRefCountDebuggingOnly(_pActorHolder->m_RefCount.f_Remove(TempRef));

			pSelfReference = fg_Attach(_pActorHolder);

			mint Destroyed = _pActorHolder->mp_bDestroyed.f_Exchange(0);
			if (Destroyed != 1)
				DMibPDebugBreak;
		}

		_pActorHolder->f_Call<CActor, TCFuture<void> (CActor::*)()>
			(
				NPrivate::fg_BindHelper<false, NMib::NMeta::TCTypeList<>>
				(
					&CActor::fp_DestroyInternal
					, NStorage::TCTuple<>()
				)
				, fg_TempCopy(NPrivate::fg_DirectResultActor())
				, [pSelfReference = fg_Move(pSelfReference)](TCAsyncResult<void> &&) mutable
				{
				}
			)
		;

		return true;
	}

	inline_never bool CActorHolder::fsp_ScheduleActorHolderDestroy(CActorHolder *_pActorHolder, bool &o_bImmediateDelete)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (_pActorHolder == ThreadLocal.m_pCurrentlyDestructingActorHolder)
			return true;

		smint Expected = 0;
		if (_pActorHolder->mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
		{
			if (CActorHolder::fsp_ScheduleActorDestroy(_pActorHolder))
				return true;

			DMibFastCheck(!_pActorHolder->f_ImmediateDelete()); // Should have been terminated first
			_pActorHolder->fp_DestroyActorHolder(nullptr, nullptr, ThreadLocal);
			return true;
		}

		bool bDestroyingInThreadPoolQueue = (!ThreadLocal.m_bCurrentlyProcessingInActorHolder && ThreadLocal.m_pThisQueue);

		if (bDestroyingInThreadPoolQueue || _pActorHolder->f_ImmediateDelete())
			o_bImmediateDelete = true;

		return false;
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

			auto pActor = This.fp_GetActorRelaxed();
			if (pActor)
			{
#if DMibEnableSafeCheck > 0

				auto pOldDestructingActor = ThreadLocal.m_pCurrentlyDestructingActor;
				ThreadLocal.m_pCurrentlyDestructingActor = pActor;
				auto Cleanup = g_OnScopeExit / [&]
					{
						ThreadLocal.m_pCurrentlyDestructingActor = pOldDestructingActor;
					}
				;
#endif
				pActor->self.m_pThis.f_SetBits(1);
				pActor->fp_AbortSuspendedCoroutines();
				This.fp_DeleteActor();
			}
			This.mp_bDestroyed.f_Exchange(2);

			bool bShouldDelete;

			if (m_pSelfReference)
			{
				auto pLastDestructing = ThreadLocal.m_pCurrentlyDestructingActorHolder;
				ThreadLocal.m_pCurrentlyDestructingActorHolder = &This;
				auto Cleanup = g_OnScopeExit / [&]
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
					This.m_RefCount.f_WeakSetCapturedDelete(CapturedDelete);
					if (This.m_RefCount.f_WeakDecrease(DMibRefCountDebuggingOnly(nullptr)) == 0)
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
				This.mp_pConcurrencyManager->fp_DispatchOnCurrentThreadOrConcurrentFirst
					(
						This.f_GetPriority()
						, g_OnScopeExit / [pThis = &This, fOnDestroyed = fg_Move(m_fOnDestroyed)]() mutable
						{
							auto &This = *pThis;

							NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(&This);
							This.m_RefCount.f_WeakSetCapturedDelete(CapturedDelete);
							if (This.m_RefCount.f_WeakDecrease(DMibRefCountDebuggingOnly(nullptr)) == 0)
							{
								if (CapturedDelete.m_Size)
									CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
								else
									CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
							}

							if (fOnDestroyed)
								fOnDestroyed();
						}
						, ThreadLocal
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
			, CConcurrencyThreadLocal &_ThreadLocal
		)
	{
		fp_QueueProcessDestroy(CDestroyHandler(fg_Move(_fOnDestroyed), this, fg_Move(_pSelfReference)), _ThreadLocal);
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

	NStr::CStr const &CSeparateThreadActorHolder::f_GetThreadName()
	{
		return mp_ThreadName;
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
				, f_ConcurrencyManager().f_GetExecutionPriority(f_GetPriority())
			)
		;
	}

	void CSeparateThreadActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
			m_pThread->m_EventWantQuit.f_Signal();
	}

	void CSeparateThreadActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		// Reference this so it doesn't go out of scope if queue is processed before thread has been notified
		TCActorHolderSharedPointer<CSeparateThreadActorHolder> pThis = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
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

	void CDispatchingActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
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

	void CDispatchingActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		TCActorHolderSharedPointer<CDispatchingActorHolder> pThis = fg_Explicit(this);
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
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
					DMibFastCheck(!pDelegateTo->f_IsHolderDestroyed());
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

	void CDelegatedActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		mp_pDelegateTo->f_QueueProcessDestroy
			(
				[this, Functor = fg_Move(_Functor)]() mutable
				{
					if (this->fp_GetActorRelaxed())
						Functor();
				}
			)
		;
	}

	void CDelegatedActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		mp_pDelegateTo->f_QueueProcess
			(
				[pThisWeak = TCActorHolderWeakPointer<CDelegatedActorHolder>{this}, Functor = fg_Move(_Functor)]() mutable
				{
					auto pThis = pThisWeak.f_Lock();
					if (pThis && pThis->fp_GetActorRelaxed())
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

	void CDirectCallActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		_Functor();
	}

	void CDirectCallActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
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

	void CShamActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		_Functor();
	}

	void CShamActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
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

	void CConcurrentRunQueue::f_AddToQueue(FActorQueueDispatch &&_Functor)
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

	void CConcurrentRunQueue::f_PopQueueEntry(FActorQueueDispatch *_pEntry)
	{
		mint Offset = DMibPOffsetOf(CQueueEntry, m_fToCall);
		CQueueEntry *pEntry = (CQueueEntry *)((uint8 *)_pEntry - Offset);
		fg_DeleteObjectDefiniteType(NMib::NMemory::CDefaultAllocator(), pEntry);
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

	void CDefaultActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			mp_pConcurrencyManager->fp_DispatchOnCurrentThreadOrConcurrent
				(
					this->f_GetPriority()
					, [this]() // This referenced in _Fuctor
					{
						this->fp_RunProcess();
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CDefaultActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
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
					, _ThreadLocal
				)
			;
		}
	}

	void CActorHolder::f_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
	{
		fp_QueueProcessDestroy(fg_Move(_Functor), fg_ConcurrencyThreadLocal());
	}

	void CActorHolder::f_QueueProcess(FActorQueueDispatch &&_Functor)
	{
		fp_QueueProcess(fg_Move(_Functor), fg_ConcurrencyThreadLocal());
	}
}
