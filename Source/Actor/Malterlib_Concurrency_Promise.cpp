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
			mp_pThis->f_SetExceptionResult(fg_Move(pException));
			mp_pThis->f_Abort();
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

	void CFutureCoroutineContext::f_HandleAwaitedException(NException::CExceptionPointer &&_pException)
	{
		f_SetExceptionResult(fg_Move(_pException));
		f_Abort();
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
	}
#endif
}
