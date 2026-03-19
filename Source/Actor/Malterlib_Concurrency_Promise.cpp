// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"
#include <Mib/Log/Log>

namespace NMib::NFunction
{
	extern template struct TCFunctionMutable<NConcurrency::TCFuture<void> ()>;
}

namespace NMib::NConcurrency
{
	constinit CCoroutineOnSuspendHelper g_OnSuspend;
	constinit CCaptureExceptionsHelper g_CaptureExceptions;

	CCaptureExceptionsHelperWithTransformer CCaptureExceptionsHelperWithTransformer::ms_EmptyTransformer
		{
			[](NException::CExceptionPointer &&_pException)
			{
				return fg_Move(_pException);
			}
		}
	;

	CCoroutineOnSuspendScope::CCoroutineOnSuspendScope(NFunction::TCFunctionMovable<void ()> &&_fOnSuspend)
		: m_fOnSuspend(fg_Move(_fOnSuspend))
	{
	}

	CCoroutineOnSuspendScope::~CCoroutineOnSuspendScope()
	{
	}

	void CCoroutineOnSuspendScope::f_Suspend() noexcept
	{
		m_fOnSuspend();
	}

	void CCoroutineOnSuspendScope::f_ResumeNoExcept() noexcept
	{
	}

	void CCoroutineOnSuspendScope::operator ()()
	{
		m_fOnSuspend();
	}

	CCoroutineOnResumeScope::CCoroutineOnResumeScope(NFunction::TCFunctionMovable<NStorage::TCUniquePointer<ICOnResumeResult> ()> &&_fOnResume)
		: mp_fOnResume(fg_Move(_fOnResume))
	{
	}

	CCoroutineOnResumeScope::~CCoroutineOnResumeScope()
	{
	}

	void CCoroutineOnResumeScope::f_Suspend() noexcept
	{
	}

	NStorage::TCUniquePointer<ICOnResumeResult> CCoroutineOnResumeScope::f_Resume() noexcept
	{
		return mp_fOnResume();
	}

	CCoroutineOnSuspendScope CCoroutineOnSuspendHelper::operator / (NFunction::TCFunctionMovable<void ()> &&_fOnSuspend) const
	{
		return CCoroutineOnSuspendScope(fg_Move(_fOnSuspend));
	}

	CFutureCoroutineContext::CCaptureExceptionScope::CCaptureExceptionScope(CFutureCoroutineContext *_pThis)
		: mp_pThis(_pThis)
		, mp_OriginalExceptions(NException::fg_UncaughtExceptions())
	{
	}

	CFutureCoroutineContext::CCaptureExceptionScope::CCaptureExceptionScope(CCaptureExceptionScope &&_Other)
		: mp_pThis(fg_Exchange(_Other.mp_pThis, nullptr))
		, mp_OriginalExceptions(_Other.mp_OriginalExceptions)
	{
	}

	CFutureCoroutineContext::CCaptureExceptionScope::~CCaptureExceptionScope()
	{
		if (mp_pThis && NException::fg_UncaughtExceptions() != mp_OriginalExceptions)
			fg_ConcurrencyThreadLocal().m_PendingCaptureExceptions.f_Insert();
	}

	void CFutureCoroutineContext::CCaptureExceptionScope::f_Suspend() noexcept
	{
	}

	void CFutureCoroutineContext::CCaptureExceptionScope::f_ResumeNoExcept() noexcept
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		// Clean up any leaked pending transformers
		DMibFastCheck(ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_IsEmpty());
		ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_Clear();
		mp_OriginalExceptions = NException::fg_UncaughtExceptions();
	}

	CFutureCoroutineContext::CCaptureExceptionScope CFutureCoroutineContext::CCaptureExceptionScopeAwaiter::await_resume() const noexcept
	{
		return CCaptureExceptionScope(m_pThis);
	}

	CFutureCoroutineContext::CCaptureExceptionWithTransformerScope::CCaptureExceptionWithTransformerScope
		(
			CFutureCoroutineContext *_pThis
			, FExceptionTransformer &&_fTransformer
		)
		: mp_pThis(_pThis)
		, mp_fTransformer(fg_Move(_fTransformer))
		, mp_OriginalExceptions(NException::fg_UncaughtExceptions())
	{
	}

	CFutureCoroutineContext::CCaptureExceptionWithTransformerScope::CCaptureExceptionWithTransformerScope(CCaptureExceptionWithTransformerScope &&_Other)
		: mp_pThis(fg_Exchange(_Other.mp_pThis, nullptr))
		, mp_fTransformer(fg_Move(_Other.mp_fTransformer))
		, mp_OriginalExceptions(_Other.mp_OriginalExceptions)
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		// Clean up any leaked pending transformers
		DMibFastCheck(ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_IsEmpty());
		ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_Clear();
	}

	CFutureCoroutineContext::CCaptureExceptionWithTransformerScope::~CCaptureExceptionWithTransformerScope()
	{
		if (mp_pThis && NException::fg_UncaughtExceptions() != mp_OriginalExceptions)
			fg_ConcurrencyThreadLocal().m_PendingCaptureExceptions.f_Insert().m_fTransformer = fg_Move(mp_fTransformer);
	}

	void CFutureCoroutineContext::CCaptureExceptionWithTransformerScope::f_Suspend() noexcept
	{
	}

	void CFutureCoroutineContext::CCaptureExceptionWithTransformerScope::f_ResumeNoExcept() noexcept
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		// Clean up any leaked pending transformers
		DMibFastCheck(ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_IsEmpty());
		ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_Clear();
		mp_OriginalExceptions = NException::fg_UncaughtExceptions();
	}

	auto CFutureCoroutineContext::CCaptureExceptionScopeWithTransformerAwaiter::await_resume() noexcept -> CCaptureExceptionWithTransformerScope
	{
		return CCaptureExceptionWithTransformerScope(m_pThis, fg_Move(m_fTransformer));
	}

	CCoroutineOnResumeScope CFutureCoroutineContextOnResumeScopeAwaiter::await_resume() noexcept
	{
		return CCoroutineOnResumeScope(fg_Move(mp_fOnResume));
	}

	CFutureCoroutineContextOnResumeScopeAwaiter &&CFutureCoroutineContext::await_transform(CFutureCoroutineContextOnResumeScopeAwaiter &&_Awaiter)
	{
		_Awaiter.mp_pThis = this;
#if DMibEnableSafeCheck > 0
		_Awaiter.mp_bAwaited = true;
#endif
		return fg_Move(_Awaiter);
	}

	CFutureCoroutineContextOnResumeScopeAwaiter::~CFutureCoroutineContextOnResumeScopeAwaiter()
#if DMibEnableSafeCheck > 0
		noexcept(false)
#endif
	{
		DMibSafeCheck(fg_Exchange(mp_bAwaited, true), "You forgot to co_await the on resume");
		// Correct usage:
		/*
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> CExceptionPointer
					{
						if (f_IsDestroyed())
							return DMibErrorInstance("Shutting down");
						return {};
					}
				)
			;
		*/
	}

	CFutureCoroutineContextOnResumeScopeAwaiter::CFutureCoroutineContextOnResumeScopeAwaiter(NFunction::TCFunctionMovable<NStorage::TCUniquePointer<ICOnResumeResult> ()> &&_fOnResume)
		: mp_fOnResume(fg_Move(_fOnResume))
	{
	}

	CFutureCoroutineContext::CCaptureExceptionScopeAwaiter CFutureCoroutineContext::await_transform(CCaptureExceptionsHelper)
	{
		return CCaptureExceptionScopeAwaiter{{}, this};
	}

	auto CFutureCoroutineContext::await_transform(CCaptureExceptionsHelperWithTransformer &&_Transformer) -> CCaptureExceptionScopeWithTransformerAwaiter
	{
		return CCaptureExceptionScopeWithTransformerAwaiter{{}, this, fg_Move(_Transformer.m_fTransformer)};
	}

	void CFutureCoroutineContext::unhandled_exception()
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto PendingCaptureExceptions = fg_Move(ConcurrencyThreadLocal.m_PendingCaptureExceptions);
		auto pException = NException::fg_CurrentException();

		if (!PendingCaptureExceptions.f_IsEmpty())
		{
			for (auto &CaptureExceptions : PendingCaptureExceptions)
			{
				if (CaptureExceptions.m_fTransformer)
				{
					auto pNewException = CaptureExceptions.m_fTransformer(fg_TempCopy(pException));
					if (pNewException)
						return f_SetExceptionResult(fg_Move(pNewException));
				}
				else if (NException::fg_ExceptionIsOfType<NException::CException>(pException))
					return f_SetExceptionResult(fg_Move(pException));
			}
		}

		if (this->m_Flags & ECoroutineFlag_CaptureMalterlibExceptions)
		{
			if (NException::fg_ExceptionIsOfType<NException::CException>(pException))
				return f_SetExceptionResult(fg_Move(pException));
		}
		else if (this->m_Flags & ECoroutineFlag_CaptureExceptions)
			return f_SetExceptionResult(fg_Move(pException));

#ifdef DMibNeedDebugException
		if (fg_IsDebugException(pException))
			return f_SetExceptionResult(fg_Move(pException));
#endif

		// All other exceptions falls through and crashes the application
		std::rethrow_exception(fg_Move(pException));
	}

#if DMibEnableSafeCheck > 0
	void CFutureCoroutineContext::fp_CheckCaptureExceptions() const
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();

		if (ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_IsEmpty())
			return;

		ConcurrencyThreadLocal.m_PendingCaptureExceptions.f_Clear();

		if (ms_bDebugCoroutineSafeCheck)
			DMibSafeCheck(false, "It's not supported to capture exceptions inside a try/catch statement");

		DMibFastCheck(false);
	}
#endif

	NContainer::TCVector<TCFuture<void>> CFutureCoroutineContext::fp_AbortAndCaptureAsyncDestructors(CConcurrencyThreadLocal &_ThreadLocal)
	{
		if (_ThreadLocal.m_bCaptureAsyncDestructors) [[unlikely]]
		{
			// Nested case: save outer scope's destructors, abort, capture ours, restore outer
			auto SavedDestructors = fg_Move(_ThreadLocal.m_AsyncDestructors);
			auto RestoreOnException = g_OnScopeExit / [&]
				{
					_ThreadLocal.m_AsyncDestructors = fg_Move(SavedDestructors);
				}
			;

			f_Abort();

			auto OurDestructors = fg_Move(_ThreadLocal.m_AsyncDestructors);

			return OurDestructors;
		}

		// Normal case
		DMibFastCheck(_ThreadLocal.m_AsyncDestructors.f_IsEmpty());
		_ThreadLocal.m_AsyncDestructors.f_Clear();
		auto Cleanup = g_OnScopeExit / [&, bOld = fg_Exchange(_ThreadLocal.m_bCaptureAsyncDestructors, true)]
			{
				_ThreadLocal.m_bCaptureAsyncDestructors = bOld;
				_ThreadLocal.m_AsyncDestructors.f_Clear();
			}
		;

		f_Abort();

		return fg_Move(_ThreadLocal.m_AsyncDestructors);
	}

	TCFuture<void> CFutureCoroutineContext::f_AsyncDestroy(CConcurrencyThreadLocal &_ThreadLocal)
	{
		auto AsyncDestructors = fp_AbortAndCaptureAsyncDestructors(_ThreadLocal);

		if (!AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &Future : AsyncDestructors)
				fg_Move(Future) > DestroyResults;

			AsyncDestructors.f_Clear(); // Important to clear here to keep destruction order correct

			if (!DestroyResults.f_IsEmpty())
				return fg_AllDone(DestroyResults);
		}

		return {};
	}

	void CFutureCoroutineContext::f_HandleAwaitedResult(CConcurrencyThreadLocal &_ThreadLocal, FKeepaliveSetResult *_fSetResult, NStorage::TCUniquePointer<ICOnResumeResult> &&_pResult)
	{
		DMibFastCheck(_pResult);
#if DMibEnableSafeCheck > 0
		NException::CExceptionPointer pCaptureCheckException;
		try
		{
			fp_CheckCaptureExceptions();
		}
		catch (NException::CDebugException const &_Exception)
		{
			pCaptureCheckException = _Exception.f_ExceptionPointer();
		}

		if (pCaptureCheckException)
		{
			if (auto pOrigException = fg_Move(*_pResult).f_TryGetException())
			{
				NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
				ErrorCollector.f_AddError(fg_Move(pOrigException));
				ErrorCollector.f_AddError(fg_Move(pCaptureCheckException));
				_pResult->f_SetException(fg_Move(ErrorCollector).f_GetException());
			}
			else
				_pResult->f_SetException(fg_Move(pCaptureCheckException));
		}
#endif
		auto fDeliverResult = f_PrepareResult(_fSetResult, fg_CurrentActor(_ThreadLocal));

		auto AsyncDestructors = fp_AbortAndCaptureAsyncDestructors(_ThreadLocal);

		if (!AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &Future : AsyncDestructors)
				fg_Move(Future) > DestroyResults;

			AsyncDestructors.f_Clear(); // Important to clear here to keep destruction order correct

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromise<void> Promise
					(
						[fDeliverResult = fg_Move(fDeliverResult), pResult = fg_Move(_pResult)](TCAsyncResult<void> &&_DestroyResults) mutable
						{
							if (!_DestroyResults)
							{
								if (auto pOrigException = fg_Move(*pResult).f_TryGetException())
								{
									NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
									ErrorCollector.f_AddError(fg_Move(pOrigException));
									ErrorCollector.f_AddError(fg_Move(_DestroyResults).f_GetException());
									pResult->f_SetException(fg_Move(ErrorCollector).f_GetException());
								}
								else
									pResult->f_SetException(fg_Move(_DestroyResults).f_GetException());
							}

							fDeliverResult(fg_Move(pResult));
						}
					)
				;

				fg_AllDoneWrapped(DestroyResults).f_OnResultSet(Promise.f_ReceiveAnyUnwrap());

				return;
			}
		}

		fDeliverResult(fg_Move(_pResult));
	}

	void *CFutureCoroutineContext::fs_New(umint _Size, umint _Alignment, umint _PromiseDataSize, umint _PromiseDataAlignment)
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		if (!PromiseThreadLocal.m_pUsePromise)
		{
			umint Alignment = fg_Max(_PromiseDataAlignment, umint(_Alignment));
			umint PromiseSize = fg_AlignUp(_PromiseDataSize, umint(_Alignment));
			umint Size = PromiseSize + fg_AlignUp(_Size, umint(_Alignment));

			uint8 *pMemory = (uint8 *)NMib::NMemory::fg_AllocAligned(Size, Alignment);
			uint8 *pReturn = pMemory + PromiseSize;

			PromiseThreadLocal.f_PushAllocation(pMemory);

			return pReturn;
		}

		return NMib::NMemory::fg_AllocAligned(_Size, (umint)_Alignment);
	}

	void CFutureCoroutineContext::fs_Delete(void *_pMemory, umint _Size, umint _Alignment)
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		if (auto *pAllocation = PromiseThreadLocal.f_PopAllocation())
		{
			auto *pPromiseData = static_cast<NPrivate::CPromiseDataBase *>(pAllocation);
			DMibFastCheck(pPromiseData->m_BeforeSuspend.m_bAllocatedInCoroutineContext);
			if (pPromiseData->m_BeforeSuspend.m_bAllocatedInCoroutineContext)
			{
				// Store end in coroutine pointer
				pPromiseData->m_Coroutine = std::coroutine_handle<CFutureCoroutineContext>::from_address
					(
						(uint8 *)_pMemory
						+ fg_AlignUp(_Size, umint(_Alignment))
					)
				;

				return; // Will be deallocated when the promise is deleted
			}
		}

		return NMib::NMemory::fg_Free(_pMemory, _Size);
	}

	TCFutureOnResultOverSized<void> CBlockingActorCheckout::f_MoveResultHandler(NStr::CStr const &_Category, NStr::CStr const &_Error)
	{
		return [ThisReference = fg_Move(*this), _Category, _Error](auto &&_Result)
			{
				if (!_Result)
					CBlockingActorCheckout::fsp_LogError(_Category, _Error, _Result);
			}
		;
	}

	COnResumeException::COnResumeException(NException::CExceptionPointer &&_pException)
		: m_pException(fg_Move(_pException))
	{
	}

	NException::CExceptionPointer COnResumeException::f_TryGetException() &&
	{
		return fg_Move(m_pException);
	}

	void COnResumeException::f_SetException(NException::CExceptionPointer &&_pException)
	{
		m_pException = fg_Move(_pException);
	}

	void COnResumeException::f_ApplyValue(NPrivate::CPromiseDataBase *)
	{
		DMibFastCheck(false);  // Should never be called - this type only holds exceptions
	}

	TCOnResumeResult<void>::TCOnResumeResult(TCAsyncResult<void> &&_Result)
		: m_Result(fg_Move(_Result))
	{
	}

	NException::CExceptionPointer TCOnResumeResult<void>::f_TryGetException() &&
	{
		if (m_Result)
			return {};  // Has value, not exception

		return fg_Move(m_Result).f_GetException();
	}

	void TCOnResumeResult<void>::f_SetException(NException::CExceptionPointer &&_pException)
	{
		m_Result.f_Clear();
		m_Result.f_SetException(fg_Move(_pException));
	}

	void TCOnResumeResult<void>::f_ApplyValue(NPrivate::CPromiseDataBase *_pPromiseData)
	{
		auto *pTyped = static_cast<NPrivate::TCPromiseData<void> *>(_pPromiseData);
		pTyped->f_SetResultNoReport();
	}
}

namespace NMib::NConcurrency::NPrivate
{
	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception, NStr::CStr const &_CallStack)
	{
		DMibDTrace("Unobserved exception in future: {}{}\n", NException::fg_ExceptionString(_Exception), _CallStack);
		DMibLog(Error, "Unobserved exception in future: {}{}", NException::fg_ExceptionString(_Exception), _CallStack);
	}

	CPromiseDataBase::CPromiseDataBase
		(
			int32 _RefCount
			, uint8 _OnResultSet
			, bool _bOnResultSetAtInit
			, uint8 _OverAlignment
#if DMibConfig_Concurrency_DebugFutures
			, NStr::CStr const &_Name
#endif
#if DMibEnableSafeCheck > 0
			, void const *_pDebugTypeId
#endif
		)
		: m_OnResultSet(_OnResultSet)
		, m_RefCount(_RefCount)
		, m_BeforeSuspend{.m_bOnResultSetAtInit = _bOnResultSetAtInit, .m_OverAlignment = _OverAlignment}
#if DMibConfig_Concurrency_DebugFutures
		, m_FutureTypeName(_Name)
#endif
#if DMibEnableSafeCheck > 0
		, m_pDebugTypeId(_pDebugTypeId)
#endif
	{
		DMibFastCheck(_OverAlignment <= 7);
#if DMibConfig_Concurrency_DebugFutures
		auto &ConcurrencyManager = fg_ConcurrencyManager();
		DMibLock(ConcurrencyManager.m_FutureListLock);
		ConcurrencyManager.m_Futures.f_Insert(this);
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		if (_OnResultSet & EFutureResultFlag_DataSet)
			m_ResultSetCallstack.f_Capture();
#endif
	}

#if DMibConfig_Concurrency_DebugFutures
	CPromiseDataBase::~CPromiseDataBase()
	{
		auto &ConcurrencyManager = fg_ConcurrencyManager();
		DMibLock(ConcurrencyManager.m_FutureListLock);
		m_Link.f_Unlink();
		m_LinkCoro.f_Unlink();
	}
#endif

	inline_always_lto CAsyncResult &CPromiseDataBase::f_GetAsyncResult()
	{
		constexpr static umint gc_DefaultAlignment = fg_GetHighestBitSet(sizeof(void *));
		uint8 *pNextClass = (uint8 *)(void *)(this + 1) + sizeof(TCFutureOnResult<void>);
		uint8 *pAlignedStart = fg_AlignUp(pNextClass, umint(1) << (m_BeforeSuspend.m_OverAlignment + gc_DefaultAlignment));

		return *((CAsyncResult *)pAlignedStart);
	}

	inline_always_lto void CPromiseDataBase::CCanOnlyBeSetBeforeSuspend::f_NeedAtomicRead()
	{
		if (!m_bNeedAtomicRead)
			m_bNeedAtomicRead = true;
	}

	inline_always_lto void CPromiseDataBase::f_OnResultPending()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
		DMibFastCheck(!(m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet));
		m_AfterSuspend.m_bPendingResult = true;
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_PendingResultSetCallstack.f_Capture();
#endif
	}

	inline_always_lto void CPromiseDataBase::f_OnResultPendingAppendable()
	{
		DMibFastCheck(!(m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet));
		m_AfterSuspend.m_bPendingResult = true;
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_PendingResultSetCallstack.f_Capture();
#endif
	}
}
