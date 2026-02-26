// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency::NPrivate
{
	void TCFutureCoroutineContext<void>::return_value(CVoidTag &&_Value)
	{
		auto *pPromiseData = static_cast<TCPromiseData<void> *>(m_pPromiseData);
		pPromiseData->f_SetResultNoReport();
	}

	void TCFutureCoroutineContext<void>::return_value(CVoidTag const &_Value)
	{
		auto *pPromiseData = static_cast<TCPromiseData<void> *>(m_pPromiseData);
		pPromiseData->f_SetResultNoReport();
	}
}

namespace NMib::NConcurrency
{
	inline_always_lto auto CFutureCoroutineContext::initial_suspend() noexcept -> CInitialSuspend
	{
		auto &ThreadLocal = fg_SystemThreadLocal();

		if (!ThreadLocal.m_CrossActorStateScopes.f_IsEmpty()) [[unlikely]]
		{
			for (auto &Scope : ThreadLocal.m_CrossActorStateScopes)
				Scope.f_InitialSuspend();
		}

		if (ThreadLocal.m_PromiseThreadLocal.m_bSupendOnInitialSuspend)
		{
			ThreadLocal.m_PromiseThreadLocal.m_bSupendOnInitialSuspend = false;
			m_bAwaitingInitialResume = true;
			f_InitialSuspend(ThreadLocal);
			return {.m_bInitialReady = false};
		}
		else
		{
			new (&m_State) CRuntimeState(ThreadLocal.m_pCurrentCoroutineHandler);
			m_bRuntimeStateConstructed = true;
			m_bPreviousCoroutineHandlerConstructed = true;
#if DMibEnableSafeCheck > 0
			DMibFastCheck(!m_bLinkConstructed);
#endif
			ThreadLocal.m_pCurrentCoroutineHandler = &m_CoroutineHandlers;

			return {};
		}
	}

	inline_always_lto auto CFutureCoroutineContext::final_suspend() noexcept -> CFinalAwaiter
	{
#if DMibEnableSafeCheck > 0
		try
		{
			fp_CheckCaptureExceptions();
		}
		catch (NException::CDebugException const &_Exception)
		{
			return TCFuture<void>(_Exception);
		}
#endif
		DMibFastCheck(m_bRuntimeStateConstructed);
		DMibFastCheck(m_bPreviousCoroutineHandlerConstructed);
		DMibFastCheck(!m_bLinkConstructed);

		if (m_State.m_pPreviousCoroutineHandler != &m_CoroutineHandlers)
		{
			auto &ThreadLocal = fg_SystemThreadLocal();
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == &m_CoroutineHandlers);
			ThreadLocal.m_pCurrentCoroutineHandler = m_State.m_pPreviousCoroutineHandler;
			m_bPreviousCoroutineHandlerConstructed = false;
#if DMibEnableSafeCheck > 0
			m_State.m_pPreviousCoroutineHandler = &m_CoroutineHandlers;
#endif
		}

		if (!m_State.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : m_State.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			m_State.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
				return fg_AllDone(DestroyResults);
		}

		m_State.~CRuntimeState();
		m_bRuntimeStateConstructed = false;

		return TCFuture<void>();
	}

	TCFuture<void> CActor::fp_AbortSuspendedCoroutinesWithAsyncDestroy()
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();

		DMibFastCheck(ConcurrencyThreadLocal.m_AsyncDestructors.f_IsEmpty());
		DMibFastCheck(!ConcurrencyThreadLocal.m_bCaptureAsyncDestructors);
		ConcurrencyThreadLocal.m_AsyncDestructors.f_Clear();
		{
			auto Cleanup = g_OnScopeExit / [&, bOld = fg_Exchange(ConcurrencyThreadLocal.m_bCaptureAsyncDestructors, true)]
				{
					ConcurrencyThreadLocal.m_bCaptureAsyncDestructors = bOld;
				}
			;

			auto SuspendedCoroutines = fg_Move(mp_SuspendedCoroutines);

			while (!SuspendedCoroutines.f_IsEmpty())
			{
				auto *pRoutine = SuspendedCoroutines.f_GetFirst();

				// This is to synchronize with TCFuture destruction
				auto pPromiseData = pRoutine->m_pPromiseData;

				DIfRefCountDebugging(NStorage::CRefCountDebugReference DebugRef);
				pPromiseData->m_RefCount.f_Increase(DIfRefCountDebugging(DebugRef));
				auto Cleanup = g_OnScopeExit / [&]
					{
						DIfRefCountDebugging(pPromiseData->m_RefCount.f_Remove(DebugRef));
						[[maybe_unused]] auto bShouldDelete = pPromiseData->m_RefCount.f_PromiseDecrease();
						DMibFastCheck(!bShouldDelete); // We don't know the type so cannot delete it
					}
				;

				DMibFastCheck(pRoutine->m_bLinkConstructed);
				pRoutine->m_State.m_Link.f_UnsafeUnlink();
				pRoutine->m_bLinkConstructed = false;

				if (!pPromiseData->m_AfterSuspend.m_bPendingResult && !(pPromiseData->m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet))
					pRoutine->f_SetExceptionResult(fg_TempCopy(CAsyncResult::fs_ActorCalledDeletedException()));

				pRoutine->f_Abort();
			}
		}

		if (!ConcurrencyThreadLocal.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : ConcurrencyThreadLocal.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			ConcurrencyThreadLocal.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromiseFuturePair<void> Promise;
				fg_AllDone(DestroyResults) > fg_Move(Promise.m_Promise);
				return fg_Move(Promise.m_Future);
			}
		}

		return TCFuture<void>();
	}

	void CActor::fp_AbortSuspendedCoroutines()
	{
		while (!mp_SuspendedCoroutines.f_IsEmpty())
		{
			auto pRoutine = mp_SuspendedCoroutines.f_GetFirst();

			// This is to synchronize with TCFuture destruction
			auto pPromiseData = pRoutine->m_pPromiseData;

			DIfRefCountDebugging(NStorage::CRefCountDebugReference DebugRef);
			pPromiseData->m_RefCount.f_Increase(DIfRefCountDebugging(DebugRef));
			auto Cleanup = g_OnScopeExit / [&]
				{
					DIfRefCountDebugging(pPromiseData->m_RefCount.f_Remove(DebugRef));
					[[maybe_unused]] auto bShouldDelete = pPromiseData->m_RefCount.f_PromiseDecrease();
					DMibFastCheck(!bShouldDelete); // We don't know the type so cannot delete it
				}
			;

			DMibFastCheck(pRoutine->m_bLinkConstructed);
			pRoutine->m_State.m_Link.f_UnsafeUnlink();
			pRoutine->m_bLinkConstructed = false;

			pRoutine->f_SetExceptionResult(fg_TempCopy(CAsyncResult::fs_ActorCalledDeletedException()));
			pRoutine->f_Abort();
		}
	}

	bool CActor::f_SuspendCoroutine(CFutureCoroutineContext &_CoroutineContext)
	{
		if (self.m_pThis->mp_bIsAlwaysAlive)
			return false;

		mp_SuspendedCoroutines.f_UnsafeInsert(_CoroutineContext);

		return true;
	}

	bool CFutureCoroutineContext::f_IsException()
	{
		auto &Result = m_pPromiseData->f_GetAsyncResult();

		return !Result;
	}

	void CFutureCoroutineContext::f_SetExceptionResult(NException::CExceptionPointer &&_pException)
	{
		auto &Result = m_pPromiseData->f_GetAsyncResult();

		Result.f_SetExceptionAppendable(fg_Move(_pException));
		m_pPromiseData->f_OnResultPendingAppendable();
	}

	auto CFutureCoroutineContext::f_PrepareResult(FKeepaliveSetResult *_fSetResult, TCActor<CActor> &&_Actor) -> FDeliverResult
	{
		return [fSetResult = _fSetResult(*this, fg_Move(_Actor))](NStorage::TCUniquePointer<ICOnResumeResult> &&_pResult) mutable -> void
			{
				fSetResult(fg_Move(_pResult));
			}
		;
	}

	void CFutureCoroutineContext::f_Suspend(CSystemThreadLocal &_ThreadLocal, bool _bAddToActor)
#if DMibEnableSafeCheck <= 0
		noexcept
#endif
	{
		DMibFastCheck(m_pPromiseData->m_pCoroutineOwner != fg_DirectCallActor().f_Get()); // It's not safe to suspend on the direct call actor

		DMibFastCheck(m_CoroutineHandlers.m_nThreadLocalScopes == 0); // Oustanding thread local scopes cannot escape suspension point,
		// if needed convert thread local scope to CCoroutineThreadLocalHandler interface

		if (!_ThreadLocal.m_CrossActorStateScopes.f_IsEmpty()) [[unlikely]]
		{
			for (auto &Scope : _ThreadLocal.m_CrossActorStateScopes)
			{
				auto fRestoreScope = Scope.f_StoreState(true);
				if (fRestoreScope)
					m_RestoreScopes.f_Insert(fg_Move(fRestoreScope));
			}
		}

		auto iHandler = m_CoroutineHandlers.m_ThreadLocalHandlers.f_GetIterator();
		iHandler.f_Reverse(m_CoroutineHandlers.m_ThreadLocalHandlers);

		for (; iHandler; --iHandler)
			iHandler->f_Suspend();

		DMibFastCheck(m_bRuntimeStateConstructed);
		DMibFastCheck(m_bPreviousCoroutineHandlerConstructed);
		DMibFastCheck(_ThreadLocal.m_pCurrentCoroutineHandler == &m_CoroutineHandlers);
		DMibFastCheck(m_State.m_pPreviousCoroutineHandler != &m_CoroutineHandlers);

		_ThreadLocal.m_pCurrentCoroutineHandler = m_State.m_pPreviousCoroutineHandler;
		m_bPreviousCoroutineHandlerConstructed = false;
#if DMibEnableSafeCheck > 0
		m_State.m_pPreviousCoroutineHandler = &m_CoroutineHandlers;
#endif

		if (_bAddToActor)
		{
			auto &ConcurrencyThreadLocal = _ThreadLocal.m_PromiseThreadLocal.f_ConcurrencyThreadLocal();

			CActor *pCurrentActor = nullptr;
			if (ConcurrencyThreadLocal.m_pCurrentlyProcessingActorHolder)
				pCurrentActor = ConcurrencyThreadLocal.m_pCurrentlyProcessingActorHolder->fp_GetActorRelaxed();

			// This can happen when trying to suspend a coroutine while destroying an actor.
			// In this case use fg_ContinueRunningOnActor to transfer control of the coroutine to another actor
			// Make sure that you don't access any state that is dangling after the resumtion of the coroutine

			DMibFastCheck(pCurrentActor && pCurrentActor != ConcurrencyThreadLocal.m_pCurrentlyDestructingActor);
			DMibFastCheck(!m_bLinkConstructed);

			if (pCurrentActor)
			{
				m_bLinkConstructed = pCurrentActor->f_SuspendCoroutine(*this);
				if (m_bLinkConstructed)
					DMibFastCheck(m_State.m_Link.f_IsInList());
			}
		}

#if DMibEnableSafeCheck > 0
		try
		{
			if (m_Flags & ECoroutineFlag_UnsafeThisPointer)
				fp_CheckUnsafeThisPointer();
			else if (m_Flags & ECoroutineFlag_UnsafeReferenceParameters)
				DMibFastCheck(m_Flags & ECoroutineFlag_AllowReferences);

			fp_CheckCaptureExceptions();
		}
		catch (NException::CDebugException const &_Exception)
		{
			m_pSuspendDebugException = _Exception.f_ExceptionPointer();
		}
#endif
	}

	void CFutureCoroutineContext::f_InitialSuspend(CSystemThreadLocal &_ThreadLocal)
	{
		m_pPromiseData->m_BeforeSuspend.f_NeedAtomicRead();

		if (!_ThreadLocal.m_CrossActorStateScopes.f_IsEmpty()) [[unlikely]]
		{
			for (auto &Scope : _ThreadLocal.m_CrossActorStateScopes)
			{
				auto fRestoreScope = Scope.f_StoreState(true);
				if (fRestoreScope)
					m_RestoreScopes.f_Insert(fg_Move(fRestoreScope));
			}
		}
	}

	void CFutureCoroutineContext::f_ResumeInitialSuspend(CSystemThreadLocal &_ThreadLocal, FRestoreScopesState &o_RestoreScopes)
	{
		DMibFastCheck(_ThreadLocal.m_pCurrentCoroutineHandler != &m_CoroutineHandlers);
		DMibFastCheck(!m_bRuntimeStateConstructed);

		new (&m_State) CRuntimeState(_ThreadLocal.m_pCurrentCoroutineHandler);
		m_bRuntimeStateConstructed = true;
		m_bPreviousCoroutineHandlerConstructed = true;
#if DMibEnableSafeCheck > 0
		DMibFastCheck(!m_bLinkConstructed);
#endif
		_ThreadLocal.m_pCurrentCoroutineHandler = &m_CoroutineHandlers;

		if (!m_RestoreScopes.f_IsEmpty())
		{
			o_RestoreScopes = fg_Move(m_RestoreScopes);

			for (auto &fRestoreScope : o_RestoreScopes)
				fRestoreScope();
		}
	}

	FRestoreScopesState CFutureCoroutineContext::f_Resume(CSystemThreadLocal &_ThreadLocal, FKeepaliveSetResult *_fSetResult, bool &o_bAborted)
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;
		DMibFastCheck(m_bRuntimeStateConstructed);
		DMibFastCheck(!m_bPreviousCoroutineHandlerConstructed);

		if (m_bLinkConstructed)
		{
			DMibFastCheck(m_State.m_Link.f_IsInList());
			m_State.m_Link.f_UnsafeUnlink();
			m_bLinkConstructed = false;
		}

		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler != &m_CoroutineHandlers);
		m_State.m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = &m_CoroutineHandlers;
		m_bPreviousCoroutineHandlerConstructed = true;

#if DMibEnableSafeCheck > 0
		if (m_pSuspendDebugException)
		{
			NStorage::TCUniquePointer<COnResumeException> pResult = fg_Construct(fg_Move(m_pSuspendDebugException));
			f_HandleAwaitedResult(ConcurrencyThreadLocal, _fSetResult, fg_Move(pResult));
			o_bAborted = true;
			return {};
		}
#endif
		auto iHandler = m_CoroutineHandlers.m_ThreadLocalHandlers.f_GetIterator();
		for (; iHandler; ++iHandler)
		{
			auto pResult = iHandler->f_Resume();
			if (pResult)
			{
				if (iHandler)
					--iHandler;
				for (; iHandler; --iHandler)
					iHandler->f_Suspend();

				f_HandleAwaitedResult(ConcurrencyThreadLocal, _fSetResult, fg_Move(pResult));

				o_bAborted = true;

				return {};
			}
		}

		auto RestoreScopes = fg_Move(m_RestoreScopes);

		for (auto &fRestoreScope : RestoreScopes)
			fRestoreScope();

		return RestoreScopes;
	}

	void CFutureCoroutineContext::f_Abort()
	{
		auto pPromiseData = m_pPromiseData;
		if (!pPromiseData || !pPromiseData->m_AfterSuspend.m_bCoroutineValid)
			return;
		auto Coroutine = fg_Exchange(pPromiseData->m_Coroutine, nullptr);
		NPrivate::fg_DestroyCoroutine(Coroutine);
	}

#if DMibEnableSafeCheck > 0
	void CFutureCoroutineContext::f_SetOwner(CActorHolder *_pCoroutineOwner)
	{
#ifndef DMibClangAnalyzerWorkaround
		m_pPromiseData->m_pCoroutineOwner = _pCoroutineOwner;
#endif
	}
#endif

	void CFutureCoroutineContext::fp_GetReturnObjectShared
		(
			NPrivate::CPromiseDataBase *_pPromiseData
			, CPromiseThreadLocal &_ThreadLocal
#if DMibEnableSafeCheck > 0
			, bool _bGenerator
#endif
		)
	{
#if DMibEnableSafeCheck > 0
		bool bSafeCall = _ThreadLocal.m_bExpectCoroutineCall;
		_ThreadLocal.m_bExpectCoroutineCall = false;
#endif
		_pPromiseData->m_Coroutine = TCCoroutineHandle<CFutureCoroutineContext>::from_promise(*this);
		_pPromiseData->m_AfterSuspend.m_bCoroutineValid = true;
		_pPromiseData->m_BeforeSuspend.m_bIsCoroutine = true;

		if (_pPromiseData->m_BeforeSuspend.m_bAllocatedInCoroutineContext)
			_pPromiseData->m_BeforeSuspend.m_bNeedAtomicRead = false;

#if DMibEnableSafeCheck > 0
		_pPromiseData->m_bFutureGotten = true;
		_pPromiseData->m_pHadCoroutine = this;
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_CreateCallstack.f_Capture();
#endif

#if DMibConfig_Concurrency_DebugFutures
		{
			auto &ConcurrencyManager = fg_ConcurrencyManager();
			DMibLock(ConcurrencyManager.m_FutureListLock);
			ConcurrencyManager.m_Coroutines.f_Insert(_pPromiseData);
		}
#endif

#if DMibEnableSafeCheck > 0
		if (_ThreadLocal.m_pCurrentActorCalled)
			_pPromiseData->m_pCoroutineOwner = (CActorHolder *)_ThreadLocal.m_pCurrentActorCalled;
		else
			_pPromiseData->m_pCoroutineOwner = _ThreadLocal.f_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder;

		this->fp_UpdateSafeCall(bSafeCall);
		if (bSafeCall)
		{
			DMibFastCheck
				(
					_ThreadLocal.m_pOnResultSetConsumedBy == _pPromiseData
					|| _ThreadLocal.m_pOnResultSetConsumedBy == this
					|| _ThreadLocal.m_pUsePromiseConsumedBy == this
					|| _ThreadLocal.m_bSafeCall
					|| _bGenerator
				)
			;
			_ThreadLocal.m_pExpectCoroutineCallConsumedBy = _pPromiseData;
		}
#endif
		this->m_pPromiseData = _pPromiseData;

		if (_ThreadLocal.m_pKeepAlive) [[unlikely]]
		{
			DMibFastCheck(!m_pKeepAlive);
			m_pKeepAlive = fg_Explicit(_ThreadLocal.m_pKeepAlive);
			_ThreadLocal.m_pKeepAlive = nullptr;
		}
	}

	bool CFutureCoroutineContext::fp_DestroyShared(NPrivate::CPromiseDataBase *_pPromiseData)
	{
		auto &PromiseData = *_pPromiseData;
		PromiseData.m_Coroutine = nullptr;
		PromiseData.m_AfterSuspend.m_bCoroutineValid = false;

		auto bReturn = PromiseData.m_AfterSuspend.m_bPendingResult;
		PromiseData.m_AfterSuspend.m_bPendingResult = false;
		return bReturn;
	}

#ifdef DMibNeedDebugException
	bool fg_IsDebugException(NException::CExceptionPointer const &_pException)
	{
		return NException::fg_ExceptionIsOfType<NException::CDebugException>(_pException);
	}
#endif

#if DMibConcurrencySupportCoroutineFreeFunctionDebug
	extern "C" mark_nodebug void fg_MalterlibConcurrency_TCFutureFunctionEnter(void *_pThisFunction, void *_pCallSite)
	{
		auto &PromiseThreadLocal = fg_SystemThreadLocal().m_PromiseThreadLocal;
		PromiseThreadLocal.m_bExpectCoroutineCall = false;
	}
#endif

#if DMibEnableSafeCheck > 0

	bool CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck = false;

	inline_never void CFutureCoroutineContext::fp_UpdateSafeCall(bool _bSafeCall)
	{
		m_bIsCalledSafely = _bSafeCall;
	}

	inline_never void CFutureCoroutineContext::fp_CheckUnsafeThisPointer()
	{
		if (m_Flags & ECoroutineFlag_AllowReferences)
			return;

		if (!m_bIsCalledSafely)
		{
			if (ms_bDebugCoroutineSafeCheck)
			{
				DMibSafeCheck(false, "Unsafe call to coroutine with reference this pointer");
			}
			else
			{
				NSys::fg_DebugOutput("Unsafe call to coroutine with reference this pointer:\n");

				NException::CCallstack Callstack;
				Callstack.f_Capture();
				Callstack.f_Trace(4);

#if DMibConfig_Concurrency_DebugActorCallstacks
				NSys::fg_DebugOutput("Create Callstack:\n");
				m_CreateCallstack.f_Trace(4);
#endif

				DMibFastCheck(false);
			}
		}

#if 0
		// If you hit this assert you have called a coroutine (probably a lamdba) in an unsafe way.
		// Instead of calling coroutine directly, dispatch it instead:

		TCFuture<void> f_MyFunction()
		{
			co_await
				(
					self / [=] () -> TCFuture<void>
					{
						co_await f_MyOtherCoroutineFunction();
						co_return {};
					}
				)
			;

			co_return {};
		}

		// Or if you are not in an actor:

		TCFuture<void> fg_MyCoroutineFunction()
		{
			co_await
				(
					fg_CallSafe
					(
						[=] () -> TCFuture<void>
						{
							co_await f_MyOtherCoroutineFunction();
							co_return {};
						}
					)
				)
			;

			co_return {};
		}
#endif
	}
#endif
}
