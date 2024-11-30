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

	CCoroutineOnResumeScope::CCoroutineOnResumeScope(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume)
		: mp_fOnResume(fg_Move(_fOnResume))
	{
	}

	CCoroutineOnResumeScope::~CCoroutineOnResumeScope()
	{
	}

	void CCoroutineOnResumeScope::f_Suspend() noexcept
	{
	}

	NException::CExceptionPointer CCoroutineOnResumeScope::f_Resume() noexcept
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

	CCoroutineOnResumeScope CFutureCoroutineContext::COnResumeScopeAwaiter::await_resume() noexcept
	{
		return CCoroutineOnResumeScope(fg_Move(mp_fOnResume));
	}

	CFutureCoroutineContext::COnResumeScopeAwaiter &&CFutureCoroutineContext::await_transform(COnResumeScopeAwaiter &&_Awaiter)
	{
		_Awaiter.mp_pThis = this;
#if DMibEnableSafeCheck > 0
		_Awaiter.mp_bAwaited = true;
#endif
		return fg_Move(_Awaiter);
	}

	CFutureCoroutineContext::COnResumeScopeAwaiter::~COnResumeScopeAwaiter()
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

	CFutureCoroutineContext::COnResumeScopeAwaiter::COnResumeScopeAwaiter(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume)
		: mp_fOnResume(fg_Move(_fOnResume))
	{
	}

	CFutureCoroutineContext::COnResumeScopeAwaiter fg_OnResume(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume)
	{
		return CFutureCoroutineContext::COnResumeScopeAwaiter(fg_Move(_fOnResume));
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

	void CFutureCoroutineContext::f_HandleAwaitedException(CConcurrencyThreadLocal &_ThreadLocal, FKeepaliveSetException *_fSetException, NException::CExceptionPointer &&_pException)
	{
#if DMibEnableSafeCheck > 0
		try
		{
			fp_CheckCaptureExceptions();
		}
		catch (NException::CDebugException const &_Exception)
		{
			_pException = _Exception.f_ExceptionPointer();
		}
#endif
		auto fDeliverExceptionResult = f_PrepareExceptionResult(_fSetException, fg_CurrentActor(_ThreadLocal));

		DMibFastCheck(_ThreadLocal.m_AsyncDestructors.f_IsEmpty());
		DMibFastCheck(!_ThreadLocal.m_bCaptureAsyncDestructors);
		_ThreadLocal.m_AsyncDestructors.f_Clear();
		{
			auto Cleanup = g_OnScopeExit / [&, bOld = fg_Exchange(_ThreadLocal.m_bCaptureAsyncDestructors, true)]
				{
					_ThreadLocal.m_bCaptureAsyncDestructors = bOld;
				}
			;

			f_Abort();
		}

		if (!_ThreadLocal.m_AsyncDestructors.f_IsEmpty())
		{
			TCFutureVector<void> DestroyResults;

			for (auto &fAsyncDestroy : _ThreadLocal.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults;

			_ThreadLocal.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromise<void> Promise
					(
						[fDeliverExceptionResult = fg_Move(fDeliverExceptionResult), pException = fg_Move(_pException)](TCAsyncResult<void> &&_DestroyResults) mutable
						{
							if (!_DestroyResults)
							{
								NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
								ErrorCollector.f_AddError(fg_Move(pException));
								ErrorCollector.f_AddError(fg_Move(_DestroyResults).f_GetException());
								pException = fg_Move(ErrorCollector).f_GetException();
							}

							fDeliverExceptionResult(fg_Move(pException));
						}
					)
				;

				fg_AllDoneWrapped(DestroyResults).f_OnResultSet(Promise.f_ReceiveAnyUnwrap());

				return;
			}
		}

		fDeliverExceptionResult(fg_Move(_pException));
	}

	void *CFutureCoroutineContext::fs_New(mint _Size, mint _Alignment, mint _PromiseDataSize, mint _PromiseDataAlignment)
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		if (!PromiseThreadLocal.m_pUsePromise)
		{
			mint Alignment = fg_Max(_PromiseDataAlignment, mint(_Alignment));
			mint PromiseSize = fg_AlignUp(_PromiseDataSize, mint(_Alignment));
			mint Size = PromiseSize + fg_AlignUp(_Size, mint(_Alignment));

			uint8 *pMemory = (uint8 *)NMib::NMemory::fg_AllocAligned(Size, Alignment);
			uint8 *pReturn = pMemory + PromiseSize;

			PromiseThreadLocal.f_PushAllocation(pMemory);

			return pReturn;
		}

		return NMib::NMemory::fg_AllocAligned(_Size, (mint)_Alignment);
	}

	void CFutureCoroutineContext::fs_Delete(void *_pMemory, mint _Size, mint _Alignment)
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
						+ fg_AlignUp(_Size, mint(_Alignment))
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
}

namespace NMib::NConcurrency::NPrivate
{
	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception, NStr::CStr const &_CallStack)
	{
		DMibDTrace2("Unobserved exception in future: {}{}\n", NException::fg_ExceptionString(_Exception), _CallStack);
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
		)
		: m_OnResultSet(_OnResultSet)
		, m_RefCount(_RefCount)
		, m_BeforeSuspend{.m_bOnResultSetAtInit = _bOnResultSetAtInit, .m_OverAlignment = _OverAlignment}
#if DMibConfig_Concurrency_DebugFutures
		, m_FutureTypeName(_Name)
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
			m_ResultSetCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace(m_ResultSetCallstack.m_Callstack, 128);
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
		constexpr static mint gc_DefaultAlignment = fg_GetHighestBitSet(sizeof(void *));
		uint8 *pNextClass = (uint8 *)(void *)(this + 1) + sizeof(TCFutureOnResult<void>);
		uint8 *pAlignedStart = fg_AlignUp(pNextClass, mint(1) << (m_BeforeSuspend.m_OverAlignment + gc_DefaultAlignment));

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
		DMibFastCheck(!(m_OnResultSet.f_Load(NAtomic::EMemoryOrder_Relaxed) & EFutureResultFlag_DataSet));
		m_AfterSuspend.m_bPendingResult = true;
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_PendingResultSetCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace(m_PendingResultSetCallstack.m_Callstack, 128);
#endif
	}

	inline_always_lto void CPromiseDataBase::f_OnResultPendingAppendable()
	{
		DMibFastCheck(!(m_OnResultSet.f_Load(NAtomic::EMemoryOrder_Relaxed) & EFutureResultFlag_DataSet));
		m_AfterSuspend.m_bPendingResult = true;
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_PendingResultSetCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace(m_PendingResultSetCallstack.m_Callstack, 128);
#endif
	}
}
