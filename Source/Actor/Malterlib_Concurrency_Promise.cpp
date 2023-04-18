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
	constinit CFutureMakeHelper g_Future;
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
			, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fTransformer
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

	bool CFutureCoroutineContext::COnResumeScopeAwaiter::await_suspend(TCCoroutineHandle<> &&_Handle)
	{
		DMibFastCheck(fg_CurrentActor());
		DMibFastCheck(fg_CurrentActorProcessingOrOverridden());

		auto pException = mp_fOnResume();
		if (pException)
		{
			mp_pThis->f_HandleAwaitedException(fg_Move(pException));
			return true;
		}

		return false;
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

	void CFutureCoroutineContext::f_HandleAwaitedException(NException::CExceptionPointer &&_pException)
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
		auto fDeliverExceptionResult = f_PrepareExceptionResult(fg_CurrentActor());
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

			f_Abort();
		}

		if (!ConcurrencyThreadLocal.m_AsyncDestructors.f_IsEmpty())
		{
			TCActorResultVector<void> DestroyResults;

			for (auto &fAsyncDestroy : ConcurrencyThreadLocal.m_AsyncDestructors)
				fg_Move(fAsyncDestroy) > DestroyResults.f_AddResult();

			ConcurrencyThreadLocal.m_AsyncDestructors.f_Clear();

			if (!DestroyResults.f_IsEmpty())
			{
				TCPromise<void> Promise;
				DestroyResults.f_GetResults() > Promise.f_ReceiveAnyUnwrap();
				Promise.f_MoveFuture() > NConcurrency::NPrivate::fg_DirectResultActor()
					/ [fDeliverExceptionResult = fg_Move(fDeliverExceptionResult), pException = fg_Move(_pException)](TCAsyncResult<void> &&_DestroyResults) mutable
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
				;
				return;
			}
		}

		fDeliverExceptionResult(fg_Move(_pException));
	}
}

namespace NMib::NConcurrency::NPrivate
{
	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception, NStr::CStr const &_CallStack)
	{
		DMibDTrace2("Unobserved exception in future: {}{}\n", NException::fg_ExceptionString(_Exception), _CallStack);
		DMibLog(Error, "Unobserved exception in future: {}{}", NException::fg_ExceptionString(_Exception), _CallStack);
	}

#if DMibConfig_Concurrency_DebugFutures
	CPromiseDataBase::CPromiseDataBase(NStr::CStr const &_Name)
		: m_FutureTypeName(_Name)
	{
		auto &ConcurrencyManager = fg_ConcurrencyManager();
		DMibLock(ConcurrencyManager.m_FutureListLock);
		ConcurrencyManager.m_Futures.f_Insert(this);
	}

	CPromiseDataBase::~CPromiseDataBase()
	{
		auto &ConcurrencyManager = fg_ConcurrencyManager();
		DMibLock(ConcurrencyManager.m_FutureListLock);
		m_Link.f_Unlink();
		m_LinkCoro.f_Unlink();
	}
#endif
}
