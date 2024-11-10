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
	auto CFutureCoroutineContext::initial_suspend() noexcept -> CInitialSuspend
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

	auto CFutureCoroutineContext::final_suspend() noexcept -> CFinalAwaiter
	{
#if DMibEnableSafeCheck > 0
		try
		{
			fp_CheckCaptureExceptions();
		}
		catch (NException::CDebugException const &_Exception)
		{
			return TCPromise<void>() <<= _Exception;
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
			{
				TCPromise<void> Promise{CPromiseConstructNoConsume()};
				fg_AllDoneWrapped(DestroyResults) > Promise.f_ReceiveAnyUnwrap();
				return fg_Move(Promise.f_MoveFuture());
			}
		}

		m_State.~CRuntimeState();
		m_bRuntimeStateConstructed = false;

		return TCFuture<void>();
	}

	TCFuture<void> CActor::fp_AbortSuspendedCoroutinesWithAsyncDestroy()
	{
		TCPromise<void> Promise;

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
				DMibFastCheck(pRoutine->m_bLinkConstructed);
				pRoutine->m_State.m_Link.f_UnsafeUnlink();
				pRoutine->m_bLinkConstructed = false;

				pRoutine->f_SetExceptionResult(fg_TempCopy(CAsyncResult::fs_ActorDeletedException()));
				pRoutine->f_Abort();
			}
		}

		if (!ConcurrencyThreadLocal.m_AsyncDestructors.f_IsEmpty())
		{
			TCActorResultVector<void> DestroyResults;

			for (auto &fAsyncDestroy : ConcurrencyThreadLocal.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults.f_AddResult();

			ConcurrencyThreadLocal.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				o_bNeedWait = true;
				DestroyResults.f_GetUnwrappedResults() > Promise;
			}
		}

		return Promise.f_MoveFuture();
	}

	void CActor::fp_AbortSuspendedCoroutines()
	{
		while (!mp_SuspendedCoroutines.f_IsEmpty())
		{
			auto pRoutine = mp_SuspendedCoroutines.f_GetFirst();
			DMibFastCheck(pRoutine->m_bLinkConstructed);
			pRoutine->m_State.m_Link.f_UnsafeUnlink();
			pRoutine->m_bLinkConstructed = false;

			pRoutine->f_SetExceptionResult(fg_TempCopy(CAsyncResult::fs_ActorDeletedException()));
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
		m_pPromiseData->m_AfterSuspend.m_bPendingResult = true;
	}

	auto CFutureCoroutineContext::f_PrepareExceptionResult(FGetVirtualKeepalive *_fGetKeepAlive, TCActor<CActor> &&_Actor) -> FDeliverExceptionResult
	{
		return [fKeepAlive = _fGetKeepAlive(*this, fg_Move(_Actor))](NException::CExceptionPointer &&_pException) mutable -> void
			{
				auto pPromiseData = fKeepAlive();
				pPromiseData->f_GetAsyncResult().f_SetExceptionAppendable(fg_Move(_pException));
				pPromiseData->m_AfterSuspend.m_bPendingResult = true;
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

	NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> CFutureCoroutineContext::f_ResumeInitialSuspend(CSystemThreadLocal &_ThreadLocal)
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

		auto RestoreScopes = fg_Move(m_RestoreScopes);

		for (auto &fRestoreScope : RestoreScopes)
			fRestoreScope();

		return RestoreScopes;
	}

	NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> CFutureCoroutineContext::f_Resume(CSystemThreadLocal &_ThreadLocal, FGetVirtualKeepalive *_fGetKeepAlive, bool &o_bAborted)
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
			auto pException = fg_Move(m_pSuspendDebugException);
			f_HandleAwaitedException(ConcurrencyThreadLocal, _fGetKeepAlive, fg_Move(pException));
			o_bAborted = true;
			return {};
		}
#endif
		auto iHandler = m_CoroutineHandlers.m_ThreadLocalHandlers.f_GetIterator();
		for (; iHandler; ++iHandler)
		{
			auto pException = iHandler->f_Resume();
			if (pException)
			{
				if (iHandler)
					--iHandler;
				for (; iHandler; --iHandler)
					iHandler->f_Suspend();

				f_HandleAwaitedException(ConcurrencyThreadLocal, _fGetKeepAlive, fg_Move(pException));

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
		m_CreateCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace(m_CreateCallstack.m_Callstack, 128);
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

		if (PromiseData.m_BeforeSuspend.m_bNeedToRefcountPromise)
		{
			if (!PromiseData.f_PromiseRefCountDecrease(DIfRefCountDebugging(m_Reference)))
				return false;
		}
		else
		{
#if DMibEnableSafeCheck > 0 || DMibConfig_RefCountDebugging
			[[maybe_unused]] bool bRemoved = PromiseData.f_PromiseRefCountDecrease(DIfRefCountDebugging(m_Reference));
			DMibFastCheck(bRemoved);
#endif
		}

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
	extern "C" void fg_MalterlibConcurrency_TCFutureFunctionEnter(void *_pThisFunction, void *_pCallSite)
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
				Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(Callstack.m_Callstack, sizeof(Callstack.m_Callstack) / sizeof(Callstack.m_Callstack[0]));
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

		TCFuture<void> f_MyCoroutineFunction()
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
