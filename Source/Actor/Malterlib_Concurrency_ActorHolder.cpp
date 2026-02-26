 // Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

//#define DDoWorkPolling

namespace NMib::NConcurrency
{
#if DMibConfig_RefCountDebugging != 1
	static_assert(sizeof(TCActorInternal<CActor>) == DMibPMemoryCacheLineSize * 3);
#endif

	ICDistributedActorData::~ICDistributedActorData()
	{
	}

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

	void CActorHolder::f_SetFixedQueue(mint _iFixedQueue)
	{
		mp_iFixedQueue = _iFixedQueue;
	}

	void CActorHolder::f_Yield()
	{
		DMibFastCheck(fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder == this);
		fp_Yield();
	}

	void CActorHolder::fp_Yield()
	{
		mp_bYield = true;

		// Make sure that anything that has been scheduled at this point is before us in the queue
		mp_ConcurrentRunQueue.f_TransferThreadSafeQueue(mp_ConcurrentRunQueueLocal);
	}

	void CActorHolder::fp_StartQueueProcessing()
	{

	}

	void CActorHolder::fp_DeleteActor()
	{
		auto *pActor = mp_pActorUnsafe.f_Exchange(nullptr, NAtomic::gc_MemoryOrder_Relaxed);
		if (pActor)
			pActor->~CActor();
	}

	void CActorHolder::fp_DetachActor()
	{
		mp_pActorUnsafe.f_Store(nullptr, NAtomic::gc_MemoryOrder_Relaxed);
	}

	CActor *CActorHolder::fp_GetActorRelaxed() const
	{
		return mp_pActorUnsafe.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
	}

	void CActorHolder::fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		bool bNeedKickstart = false;
		{
			mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
			DMibFastCheck((OriginalWorking & gc_ProcessingMask) == 0);
			fp_StartQueueProcessing();

#if DMibEnableSafeCheck > 0
			auto pOldConstructing = ThreadLocal.m_pCurrentlyConstructingActor;
			ThreadLocal.m_pCurrentlyConstructingActor = (CActor *)_pActorMemory;
#endif
			auto CleanupWorking = g_OnScopeExit / [&]
				{
#if DMibEnableSafeCheck > 0
					ThreadLocal.m_pCurrentlyConstructingActor = pOldConstructing;
#endif
 					mint NewWorking = mp_Working.f_Exchange(0);
					if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
						fp_QueueRunProcess(ThreadLocal); // Reschedule
				}
			;

			CCurrentlyProcessingActorScope ProcessingScope(ThreadLocal, this);

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
			fp_QueueRunProcess(ThreadLocal);
	}

	bool CActorHolder::fp_AddToQueue(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pCurrentlyProcessingActorHolder == this && _ThreadLocal.m_bCurrentlyProcessingInActorHolder)
		{
			mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Entry), mp_ConcurrentRunQueueLocal);
			return false;
		}
		else if (_ThreadLocal.m_pThisQueue && mp_iFixedQueue == _ThreadLocal.m_pThisQueue->m_iQueue && _ThreadLocal.m_pThisQueue->m_Priority == mp_Priority)
		{
			if (mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Entry), mp_ConcurrentRunQueueLocal))
			{
				mint Value = mp_Working.f_FetchAdd(1);
				return Value == 0;
			}
			return false;
		}
		mp_ConcurrentRunQueue.f_AddToQueue(fg_Move(_Entry));

		mint Value = mp_Working.f_FetchAdd(1);
		return Value == 0
#if DMibConfig_Tests_Enable && !DTests_PerfTests
			|| _ThreadLocal.m_bForceWakeUp
#endif
		;
	}

	bool CActorHolder::fp_AddToQueue(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_pCurrentlyProcessingActorHolder == this && _ThreadLocal.m_bCurrentlyProcessingInActorHolder)
		{
			mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal);
			return false;
		}
		else if (_ThreadLocal.m_pThisQueue && mp_iFixedQueue == _ThreadLocal.m_pThisQueue->m_iQueue && _ThreadLocal.m_pThisQueue->m_Priority == mp_Priority)
		{
			if (mp_ConcurrentRunQueue.f_AddToQueueLocal(fg_Move(_Functor), mp_ConcurrentRunQueueLocal))
			{
				mint Value = mp_Working.f_FetchAdd(1);
				return Value == 0;
			}
			return false;
		}
		mp_ConcurrentRunQueue.f_AddToQueue(fg_Move(_Functor));

		mint Value = mp_Working.f_FetchAdd(1);
		return Value == 0
#if DMibConfig_Tests_Enable && !DTests_PerfTests
			|| _ThreadLocal.m_bForceWakeUp
#endif
		;
	}

	bool CActorHolder::fp_DequeueProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
#if DMibEnableSafeCheck > 0
		auto OriginalExceptions = NException::fg_UncaughtExceptions();
#endif
		if (auto *pJob = mp_ConcurrentRunQueueLocal.m_LocalQueue.f_GetFirst())
		{
#if DMibEnableSafeCheck > 0
			auto Cleanup = g_OnScopeExit / [&]
				{
					DMibFastCheck(NException::fg_UncaughtExceptions() == OriginalExceptions); // Unwinding exceptions through queue processing is not supported
				}
			;
#endif
			pJob->m_Link.f_UnsafeUnlink();
			if (mp_pActorUnsafe.f_NonAtomic())
				pJob->f_Call(_ThreadLocal);
			else
				pJob->f_Cleanup();

			return !!_ThreadLocal.m_pCurrentlyProcessingActorHolder;
		}
		return false;
	}

	void CActorHolder::fp_RunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		bool bDoMore = true;
		auto &ThreadLocal = _ThreadLocal;
		{
			CCurrentlyProcessingActorScope ProcessingScope(ThreadLocal, this);

			while (bDoMore)
			{
				bDoMore = false;
				mint OriginalWorking = mp_Working.f_FetchOr(gc_ProcessingMask);
				if ((OriginalWorking & gc_ProcessingMask) != 0)
					break;

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

				if (ThreadLocal.m_pThisQueue)
					mp_iLastQueue.f_Exchange(ThreadLocal.m_pThisQueue->m_iQueue, NAtomic::gc_MemoryOrder_Relaxed);

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
						fp_QueueRunProcess(ThreadLocal);
						ThreadLocal.m_bForceNonLocal = false;

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

	bool CActorHolder::f_IsAlwaysAlive() const
	{
		return mp_bIsAlwaysAlive;
	}

	void CActorHolder::fp_DestroyThreaded()
	{
	}

	bool CActorHolder::f_IsHolderDestroyed() const
	{
		return mp_Destroyed.f_Load(NAtomic::gc_MemoryOrder_Relaxed) != 0;
	}

	bool CActorHolder::f_IsCurrentActorAndProcessing() const
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		auto pActor = fp_GetActorRelaxed();

		if (!pActor || !ThreadLocal.m_pCurrentlyProcessingActorHolder)
			return false;

		if (this == ThreadLocal.m_pCurrentlyProcessingActorHolder && ThreadLocal.m_bCurrentlyProcessingInActorHolder)
			return true;

		return false;
	}

#if DMibEnableSafeCheck > 0
	bool CActorHolder::f_CurrentlyProcessing() const
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		if (ThreadLocal.m_pCurrentlyProcessingActorHolder == this && ThreadLocal.m_bCurrentlyProcessingInActorHolder)
			return true;
		if (ThreadLocal.m_pCurrentlyDestructingActor == fp_GetActorRelaxed())
			return true;

		return false;
	}
#endif

	bool CActorHolder::fp_Terminate()
	{
		uint8 Expected = 0;
		if (mp_Destroyed.f_CompareExchangeStrong(Expected, 1))
		{
			TCActorHolderSharedPointer<CActorHolder> pReferenceHolder = fg_Explicit(this); // Reference needs to be alive during actor queueing
			fp_DestroyActorHolder(nullptr, fg_TempCopy(pReferenceHolder), fg_ConcurrencyThreadLocal());
			return true;
		}

		return false;
	}

	inline_always auto CActorHolder::fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &&_Promise)
	{
		DMibFastCheck(_pActorHolder != nullptr);

		return [pActorInternal = fg_Move(_pActorHolder), Promise = fg_Move(_Promise)](TCAsyncResult<void> &&_Result) mutable
			{
				auto &ActorInternal = *pActorInternal;
				if (uint8 Expected = 0; ActorInternal.mp_Destroyed.f_CompareExchangeStrong(Expected, 1))
				{
#if DMibEnableSafeCheck > 0
					if (auto *pActor = ActorInternal.fp_GetActorRelaxed())
						pActor->fp_CheckDestroy();
#endif
					auto pReferenceHolder = pActorInternal; // Reference needs to be alive during actor queueing
					ActorInternal.fp_DestroyActorHolder
						(
#if DMibConfig_RefCountDebugging | DMibConfig_Concurrency_DebugActorCallstacks
							NFunction::TCFunctionSmallMutable<void ()>
							(
#endif
								[Promise = fg_Move(Promise), Result = fg_Move(_Result)]() mutable
								{
									Promise.f_SetResult(fg_Move(Result));
								}
#if DMibConfig_RefCountDebugging | DMibConfig_Concurrency_DebugActorCallstacks
							)
#endif
							, fg_Move(pActorInternal)
							, fg_ConcurrencyThreadLocal()
						)
					;
				}
				else
					Promise.f_SetResult(fg_Move(_Result));
			}
		;
	}

	TCFuture<void> CActorCommon::f_Destroy() &&
	{
		auto pActorInternal = (static_cast<TCActor<> &>(*this)).m_pInternalActor;
		if (!pActorInternal)
			return g_Void;

		// Atomically check and set destroy initiated flag
		if (pActorInternal->mp_DestroyInitiated.f_Exchange(1) != 0)
			return TCPromise<void>() <<= CAsyncResult::fs_ActorAlreadyDestroyedException();

		TCPromiseFuturePair<void> Promise;
		if (pActorInternal->fp_GetActorRelaxed())
			(static_cast<TCActor<> &&>(*this)).f_Bind<&CActor::fp_DestroyInternal>() > g_DirectResult / CActorHolder::fsp_DestroyHandler(fg_Move(pActorInternal), fg_Move(Promise.m_Promise));
		else
		{
			TCAsyncResult<void> Result;
			Result.f_SetException(CAsyncResult::fs_ActorCalledDeletedException());
			CActorHolder::fsp_DestroyHandler(fg_Move(pActorInternal), fg_Move(Promise.m_Promise))(fg_Move(Result));
		}

		return fg_Move(Promise.m_Future);
	}

	TCFuture<void> CActorCommon::f_ForceDestroy() &&
	{
		auto pActorInternal = (static_cast<TCActor<> &>(*this)).m_pInternalActor;
		if (!pActorInternal)
			return g_Void;

		// No mp_DestroyInitiated check - always proceed
		// Always use immediate destruction path (like the "else" case in f_Destroy)
		TCPromiseFuturePair<void> Promise;
		TCAsyncResult<void> Result;
		Result.f_SetException(CAsyncResult::fs_ActorCalledDeletedException());
		CActorHolder::fsp_DestroyHandler(fg_Move(pActorInternal), fg_Move(Promise.m_Promise))(fg_Move(Result));

		return fg_Move(Promise.m_Future);
	}

	void CActorHolder::f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop)
	{
		// Already destroyed
		struct CState
		{
			NThread::CMutual m_ResultLock;
			TCAsyncResult<void> m_Result;
			align_cacheline NAtomic::TCAtomic<smint> m_Finished;
		};

		CState State;

		{
			TCActor<CActor> pActor = fp_GetAsActor<CActor>();

			if (_EventLoop.m_fProcess && _EventLoop.m_fWake)
			{
				fg_Move(pActor).f_Destroy().f_OnResultSet
					(
						[&](TCAsyncResult<void> &&_Result)
						{
							DMibLock(State.m_ResultLock);
							State.m_Result = fg_Move(_Result);
							State.m_Finished.f_Exchange(1);
							_EventLoop.m_fWake();
						}
					)
				;

				while (State.m_Finished.f_Load() != 1)
					_EventLoop.m_fProcess();
			}
			else
			{
				NThread::CEventAutoReset Event;

				fg_Move(pActor).f_Destroy().f_OnResultSet
					(
						[&](TCAsyncResult<void> &&_Result)
						{
							DMibLock(State.m_ResultLock);
							State.m_Result = fg_Move(_Result);
							State.m_Finished.f_Exchange(1);
							Event.f_Signal();
						}
					)
				;

				while (State.m_Finished.f_Load() != 1)
					Event.f_Wait();
			}
		}

		{
			DMibLock(State.m_ResultLock);
			State.m_Result.f_Get();
		}
	}

	inline_never bool CActorHolder::fsp_ScheduleActorDestroy(CActorHolder *_pActorHolder, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(!_pActorHolder->f_IsAlwaysAlive());
		DMibFastCheck(!_pActorHolder->f_ImmediateDelete());

		auto pActor = _pActorHolder->fp_GetActorRelaxed();

		if (!pActor)
			return false;

		if (!_pActorHolder->mp_bHasOverriddenDestroy && pActor->mp_SuspendedCoroutines.f_IsEmpty())
			return false;

		if (pActor->f_IsDestroyed())
			return false;

		TCActorHolderSharedPointer<CActorHolder> pSelfReference;
		{
			DIfRefCountDebugging(NStorage::CRefCountDebugReference TempRef);
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

			DIfRefCountDebugging(_pActorHolder->m_RefCount.f_Remove(TempRef));

			pSelfReference = fg_Attach(_pActorHolder);

			// Set destroy initiated flag before async destruction
			// (user's fp_Destroy() could create new TCActor references that call f_Destroy())
			[[maybe_unused]] mint OldValue = _pActorHolder->mp_DestroyInitiated.f_Exchange(1);
			DMibFastCheck(OldValue == 0);

			mint Destroyed = _pActorHolder->mp_Destroyed.f_Exchange(0);
			if (Destroyed != 1)
				DMibPDebugBreak;
		}

		auto pReferenceHolder = pSelfReference; // Reference needs to be alive during actor queueing
		fg_CallActorFutureDirectCoroutine<false, void, CActor>
			(
				static_cast<TCActorInternal<CActor> *>(_pActorHolder)
				, &CActor::fp_DestroyInternal
				, [pSelfReference = fg_Move(pSelfReference)](TCAsyncResult<void> &&) mutable
				{
					// If fp_DestroyInternal was not able to be called, prevent infinite destroy loop
					auto *pActor = pSelfReference->fp_GetActorRelaxed();
					if (pActor)
						pActor->self.m_pThis.f_SetBits(1);
				}
				, NMeta::TCTypeList<>()
			)
		;

		return true;
	}

	inline_never bool CActorHolder::fs_ScheduleActorHolderDestroy(CActorHolder *_pActorHolder, bool &o_bImmediateDelete)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (_pActorHolder == ThreadLocal.m_pCurrentlyDestructingActorHolder)
			return true;

		uint8 Expected = 0;
		if (_pActorHolder->mp_Destroyed.f_CompareExchangeStrong(Expected, 1))
		{
			if (CActorHolder::fsp_ScheduleActorDestroy(_pActorHolder, ThreadLocal))
				return true;

			DMibFastCheck(!_pActorHolder->f_ImmediateDelete()); // Should have been terminated first
			_pActorHolder->fp_DestroyActorHolder(nullptr, nullptr, ThreadLocal);
			return true;
		}

		bool bDestroyingInThreadPoolQueue =
			(
				!(ThreadLocal.m_pCurrentlyProcessingActorHolder && ThreadLocal.m_bCurrentlyProcessingInActorHolder)
				&& ThreadLocal.m_pThisQueue
			)
		;

		if (bDestroyingInThreadPoolQueue || _pActorHolder->f_ImmediateDelete())
			o_bImmediateDelete = true;

		return false;
	}

	struct CActorHolder::CDestroyHandler
	{
		CDestroyHandler
			(
				FDestroyHolderOnDestroyed &&_fOnDestroyed
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

		void operator () (CConcurrencyThreadLocal &_ThreadLocal)
		{
		}

		~CDestroyHandler()
		{
			if (!m_bIsValid)
				return;

			auto &This = *m_pThis;

			This.mp_OnTerminate.f_ExtractAll
				(
					[&](auto &&_Handle)
					{
						_Handle->m_fOnTerminate();
					}
				)
			;

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
			This.mp_Destroyed.f_Exchange(2);

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
					if (This.m_RefCount.f_WeakDecrease(DIfRefCountDebugging(nullptr)) == 0)
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
						, g_OnScopeExit / [pThis = &This, fOnDestroyed = fg_Move(m_fOnDestroyed)](auto && ...p_Params) mutable
						{
							auto &This = *pThis;

							NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(&This);
							This.m_RefCount.f_WeakSetCapturedDelete(CapturedDelete);
							if (This.m_RefCount.f_WeakDecrease(DIfRefCountDebugging(nullptr)) == 0)
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

		FDestroyHolderOnDestroyed m_fOnDestroyed;
		CActorHolder *m_pThis;
		TCActorHolderSharedPointer<CActorHolder> m_pSelfReference;
		bool m_bIsValid = true;
	};

	void CActorHolder::fp_DestroyActorHolder
		(
			FDestroyHolderOnDestroyed &&_fOnDestroyed
			, TCActorHolderSharedPointer<CActorHolder> &&_pSelfReference
			, CConcurrencyThreadLocal &_ThreadLocal
		)
	{
		fp_QueueProcessDestroy(CDestroyHandler(fg_Move(_fOnDestroyed), this, fg_Move(_pSelfReference)), _ThreadLocal);
	}

	TCFuture<void> CActorCommon::fp_DestroyUnused() &&
	{
		auto pActorInternal = (static_cast<TCActor<> &>(*this)).m_pInternalActor;
		if (!pActorInternal)
			return g_Void;

		TCPromiseFuturePair<void> Promise;

		TCAsyncResult<void> Result;
		if (pActorInternal->fp_GetActorRelaxed())
			Result.f_SetResult();
		else
			Result.f_SetException(CAsyncResult::fs_ActorCalledDeletedException());

		auto &ActorInternal = *pActorInternal;
		if (uint8 Expected = 0; ActorInternal.mp_Destroyed.f_CompareExchangeStrong(Expected, 1))
		{
#if DMibEnableSafeCheck > 0
			if (auto *pActor = ActorInternal.fp_GetActorRelaxed())
				pActor->fp_CheckDestroy();
#endif
			ActorInternal.mp_pConcurrencyManager->fp_DispatchOnCurrentThreadOrConcurrent
				(
					EPriority_Normal
					, NFunction::TCFunctionSmallMovable<void (CConcurrencyThreadLocal &_ThreadLocal)>
					(
						CActorHolder::CDestroyHandler
						(
		#if DMibConfig_RefCountDebugging | DMibConfig_Concurrency_DebugActorCallstacks
							NFunction::TCFunctionSmallMutable<void ()>
							(
		#endif
								[Promise = fg_Move(Promise.m_Promise), Result = fg_Move(Result)]() mutable
								{
									Promise.f_SetResult(fg_Move(Result));
								}
		#if DMibConfig_RefCountDebugging | DMibConfig_Concurrency_DebugActorCallstacks
							)
		#endif
							, &ActorInternal
							, fg_TempCopy(pActorInternal)
						)
					)
					, fg_ConcurrencyThreadLocal()
				)
			;
		}
		else
			Promise.m_Promise.f_SetResult(fg_Move(Result));

		return fg_Move(Promise.m_Future);
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
		DMibRequire(mp_pThread.f_IsEmpty());
	}

	NStr::CStr const &CSeparateThreadActorHolder::f_GetThreadName()
	{
		return mp_ThreadName;
	}

	void CSeparateThreadActorHolder::fp_QueueJob(FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal)
	{
		auto pQueueEntry = CConcurrentRunQueueNonVirtualNoAlloc::fs_QueueEntry(fg_Move(_ToQueue));

		if (_ThreadLocal.m_pCurrentlyProcessingActorHolder == this && _ThreadLocal.m_bCurrentlyProcessingInActorHolder)
		{
			if (!_ThreadLocal.m_bForceNonLocal) [[likely]]
			{
				mp_JobQueue.f_AddToQueueLocal(fg_Move(pQueueEntry), mp_JobQueueLocal);
				return;
			}
		}
		mp_JobQueue.f_AddToQueue(fg_Move(pQueueEntry));

		mint Value = mp_JobQueueWorking.f_FetchAdd(1);
		if (Value == 0)
		{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (ThreadLocal.m_nWaits)
			{
				--ThreadLocal.m_nWaits;
				NSys::fg_Thread_Sleep(0.1f);
			}
#endif
			mp_pThread->m_EventWantQuit.f_Signal();
		}
	}

	void CSeparateThreadActorHolder::fp_RunQueue(CConcurrencyThreadLocal &_ThreadLocal)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
#if DMibPPtrBits > 32
		auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#endif
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
				mint OriginalWorking = mp_JobQueueWorking.f_FetchOr(gc_ProcessingMask);
				if ((OriginalWorking & gc_ProcessingMask) == 0)
				{
					auto UnLock
						= fg_OnScopeExit
						(
							[&]
							{
								mint NewWorking = mp_JobQueueWorking.f_Exchange(0);
								if ((NewWorking & (~gc_ProcessingMask)) != OriginalWorking)
									bDoMore = true;
							}
						)
					;

					bool bDoneSomething = true;
					while (bDoneSomething)
					{
						bDoneSomething = false;
						if (mp_JobQueue.f_TransferThreadSafeQueue(mp_JobQueueLocal))
							bDoneSomething = true;
						while (auto *pJob = mp_JobQueueLocal.m_LocalQueue.f_GetFirst())
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
	}

	void CSeparateThreadActorHolder::fp_StartQueueProcessing()
	{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
		fp64 BusyWaitTime = 0.0;
		if (fg_ConcurrencyThreadLocal().m_bForceBusyWait)
			BusyWaitTime = 0.1;
#endif
		DMibLock(mp_ThreadLock);
		mp_pThread = NThread::CThreadObject::fs_StartThread
			(
				[
					this
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
					, BusyWaitTime
#endif
				](NThread::CThreadObject *_pThread) -> aint
				{
					{
						DMibLock(mp_ThreadLock);
					}
					auto &ThreadLocal = fg_ConcurrencyThreadLocal();
					while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
					{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
						NTime::CCyclesClock Clock;
						Clock.f_Start();
						while (true)
						{
							fp_RunQueue(ThreadLocal);
							if (BusyWaitTime == 0.0 || Clock.f_GetTime() > BusyWaitTime)
								break;
						}
#else
						fp_RunQueue(ThreadLocal);
#endif
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
		// Make sure the memory isn't deallocated
		TCActorHolderWeakPointer<CDefaultActorHolder> pStayAlive = fg_Explicit(this);

		DMibLock(mp_ThreadLock);
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			fp_QueueJob
				(
					[this, pStayAlive = fg_Move(pStayAlive)](CConcurrencyThreadLocal &_ThreadLocal)
					{
						if (this->mp_Destroyed.f_Load() >= 3)
							return;

						this->fp_RunProcess(_ThreadLocal);
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CSeparateThreadActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
		fp_QueueJob
			(
				[pThis = TCActorHolderSharedPointer<CSeparateThreadActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
				{
					DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
					pThis->fp_RunProcess(_ThreadLocal);
				}
				, _ThreadLocal
			)
		;
	}

	void CSeparateThreadActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
			fp_QueueJob
				(
					[pThis = TCActorHolderSharedPointer<CSeparateThreadActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
					{
						DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
						pThis->fp_RunProcess(_ThreadLocal);
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CSeparateThreadActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Entry), _ThreadLocal))
		{
			DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
			fp_QueueJob
				(
					[pThis = TCActorHolderSharedPointer<CSeparateThreadActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
					{
						DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
						pThis->fp_RunProcess(_ThreadLocal);
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CSeparateThreadActorHolder::fp_DestroyThreaded()
	{
		{
			DMibLock(mp_ThreadLock);
			mp_pThread.f_Clear();
		}
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
			, NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> &&_Dispatcher
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		, mp_Dispatcher(fg_Move(_Dispatcher))
	{
	}

	void CDispatchingActorHolder::fp_DestroyThreaded()
	{
		{
			DMibLock(mp_DestroyLock);
			mp_Destroyed.f_Exchange(4);
		}
		CDefaultActorHolder::fp_DestroyThreaded();
	}

	void CDispatchingActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibLock(mp_DestroyLock);
		// Make sure the memory isn't deallocated
		TCActorHolderWeakPointer<CDefaultActorHolder> pStayAlive = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			mp_Dispatcher
				(
					[this, pStayAlive = fg_Move(pStayAlive)](CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						if (this->mp_Destroyed.f_Load() >= 4)
							return;

						fp_RunProcess(_ThreadLocal);
					}
				)
			;
		}
	}

	void CDispatchingActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
		mp_Dispatcher
			(
				[pThis = TCActorHolderSharedPointer<CDispatchingActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal) mutable
				{
					pThis->fp_RunProcess(_ThreadLocal);
				}
			)
		;
	}

	void CDispatchingActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
			mp_Dispatcher
				(
					[pThis = TCActorHolderSharedPointer<CDispatchingActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						pThis->fp_RunProcess(_ThreadLocal);
					}
				)
			;
		}
	}


	void CDispatchingActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (fp_AddToQueue(fg_Move(_Entry), _ThreadLocal))
		{
			mp_Dispatcher
				(
					[pThis = TCActorHolderSharedPointer<CDispatchingActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						pThis->fp_RunProcess(_ThreadLocal);
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
				[pDelegateTo = _DelegateToActor.m_pInternalActor, pThis = TCActorHolderSharedPointer<CDelegatedActorHolder>{this}](CConcurrencyThreadLocal &_ThreadLocal)
				{
					DMibFastCheck(!pDelegateTo->f_IsHolderDestroyed());
					COnTerminate OnTerminate;
					OnTerminate.m_fOnTerminate = [pThisWeak = pThis->fp_GetAsActor<CActor>().f_Weak(), pThisHolder = pThis.f_Get()]
						{
							auto pThis = pThisWeak.f_Lock();
							if (!pThis)
								return;
							pThisHolder->mp_pOnTerminateEntry = nullptr;
							fg_Move(pThis).f_ForceDestroy().f_DiscardResult();
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
				[pOnTerminateEntry = mp_pOnTerminateEntry, pDelegateTo = mp_pDelegateTo](CConcurrencyThreadLocal &_ThreadLocal)
				{
					pDelegateTo->mp_OnTerminate.f_TryRemovePointerBasedComparison(pOnTerminateEntry);
				}
			)
		;
	}

	void CDelegatedActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		auto pReferenceHolder = mp_pDelegateTo; // Reference needs to be alive during actor queueing
		mp_pDelegateTo->f_QueueProcessDestroy
			(
				[this, Functor = fg_Move(_Functor)](CConcurrencyThreadLocal &_ThreadLocal) mutable
				{
					if (this->fp_GetActorRelaxed())
						Functor(_ThreadLocal);
				}
				, _ThreadLocal
			)
		;
	}

	void CDelegatedActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
	}

	void CDelegatedActorHolder::fp_Yield()
	{
		mp_pDelegateTo->fp_Yield();
	}

	void CDelegatedActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		mp_pDelegateTo->f_QueueProcess
			(
				[pThisWeak = TCActorHolderWeakPointer<CDelegatedActorHolder>{this}, Functor = fg_Move(_Functor)](CConcurrencyThreadLocal &_ThreadLocal) mutable
				{
					auto pThis = pThisWeak.f_Lock();
					if (pThis && pThis->fp_GetActorRelaxed())
					{
						CCurrentlyProcessingActorScope ProcessingScope(_ThreadLocal, pThis.f_Get());

						Functor(_ThreadLocal);
					}
				}
				, _ThreadLocal
			)
		;
	}

	void CDelegatedActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		mp_pDelegateTo->f_QueueProcess
			(
				[pThisWeak = TCActorHolderWeakPointer<CDelegatedActorHolder>{this}, Entry = fg_Move(_Entry)](CConcurrencyThreadLocal &_ThreadLocal) mutable
				{
					auto pThis = pThisWeak.f_Lock();
					if (pThis && pThis->fp_GetActorRelaxed())
					{
						CCurrentlyProcessingActorScope ProcessingScope(_ThreadLocal, pThis.f_Get());

						Entry.f_Detach()->f_Call(_ThreadLocal);
					}
				}
				, _ThreadLocal
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
		_Functor(_ThreadLocal);
	}

	void CDirectCallActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
	}

	void CDirectCallActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		CCurrentActorScope Scope(_ThreadLocal, this);

		_Functor(_ThreadLocal);
	}

	void CDirectCallActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		CCurrentActorScope Scope(_ThreadLocal, this);

		_Entry.f_Detach()->f_Call(_ThreadLocal);
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
		_Functor(_ThreadLocal);
	}

	void CShamActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(false); // Should never be used directly
	}

	void CShamActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(false); // Should never be used directly
	}

	void CShamActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(false); // Should never be used directly
	}

	///

	CDefaultActorHolder::~CDefaultActorHolder()
	{
	}

	void CDefaultActorHolder::fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		// Make sure the memory isn't deallocated
		TCActorHolderWeakPointer<CDefaultActorHolder> pStayAlive = fg_Explicit(this);

		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
			mp_pConcurrencyManager->fp_DispatchOnCurrentThreadOrConcurrent
				(
					this->f_GetPriority()
					, [this, pStayAlive = fg_Move(pStayAlive)](CConcurrencyThreadLocal &_ThreadLocal)
					{
						if (this->mp_Destroyed.f_Load() >= 3)
							return;

						this->fp_RunProcess(_ThreadLocal);
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CDefaultActorHolder::fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
		mp_pConcurrencyManager->fp_QueueJob
			(
				this->f_GetPriority()
				, this->mp_iFixedQueue
				, this->mp_iLastQueue.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
				, [pThis = TCActorHolderSharedPointer<CDefaultActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
				{
					DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
					pThis->fp_RunProcess(_ThreadLocal);
				}
				, _ThreadLocal
			)
		;
	}

	void CDefaultActorHolder::fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
		if (fp_AddToQueue(fg_Move(_Functor), _ThreadLocal))
		{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (ThreadLocal.m_nWaits)
			{
				--ThreadLocal.m_nWaits;
				NSys::fg_Thread_Sleep(0.1f);
			}
#endif
			DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
			mp_pConcurrencyManager->fp_QueueJob
				(
					this->f_GetPriority()
					, this->mp_iFixedQueue
					, this->mp_iLastQueue.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
					, [pThis = TCActorHolderSharedPointer<CDefaultActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
					{
						DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
						pThis->fp_RunProcess(_ThreadLocal);
					}
					, _ThreadLocal
				)
			;
		}
	}

	void CDefaultActorHolder::fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal)
	{
		DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
		if (fp_AddToQueue(fg_Move(_Entry), _ThreadLocal))
		{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (ThreadLocal.m_nWaits)
			{
				--ThreadLocal.m_nWaits;
				NSys::fg_Thread_Sleep(0.1f);
			}
#endif
			DMibFastCheck(m_RefCount.m_RefCount.f_Load() >= 0);
			mp_pConcurrencyManager->fp_QueueJob
				(
					this->f_GetPriority()
					, this->mp_iFixedQueue
					, this->mp_iLastQueue.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
					, [pThis = TCActorHolderSharedPointer<CDefaultActorHolder>(fg_Explicit(this))](CConcurrencyThreadLocal &_ThreadLocal)
					{
						DMibFastCheck(pThis->m_RefCount.m_RefCount.f_Load() >= 0);
						pThis->fp_RunProcess(_ThreadLocal);
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

	void CActorHolder::f_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry)
	{
		fp_QueueProcessEntry(fg_Move(_Entry), fg_ConcurrencyThreadLocal());
	}
}
