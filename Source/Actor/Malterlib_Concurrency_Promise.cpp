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
	DMibImpErrorClassImplement(CExceptionCoroutineWrapper);
	
	CFutureMakeHelper g_Future;
	CCoroutineOnSuspendHelper g_OnSuspend;
	CCoroutineOnResumeHelper g_OnResume;

	CCoroutineOnSuspendScope::CCoroutineOnSuspendScope(NFunction::TCFunctionMovable<void ()> &&_fOnSuspend)
		: m_fOnSuspend(fg_Move(_fOnSuspend))
	{
	}

	CCoroutineOnSuspendScope::~CCoroutineOnSuspendScope()
	{
	}

	void CCoroutineOnSuspendScope::f_Suspend()
	{
		m_fOnSuspend();
	}

	void CCoroutineOnSuspendScope::f_Resume()
	{
	}

	void CCoroutineOnSuspendScope::operator ()()
	{
		m_fOnSuspend();
	}

	CCoroutineOnResumeScope::CCoroutineOnResumeScope(NFunction::TCFunctionMovable<void ()> &&_fOnResume)
		: m_fOnResume(fg_Move(_fOnResume))
	{
	}

	CCoroutineOnResumeScope::~CCoroutineOnResumeScope()
	{
	}

	void CCoroutineOnResumeScope::f_Suspend()
	{
	}

	void CCoroutineOnResumeScope::f_Resume()
	{
		m_fOnResume();
	}

	void CCoroutineOnResumeScope::operator ()()
	{
		m_fOnResume();
	}

	CCoroutineOnSuspendScope CCoroutineOnSuspendHelper::operator / (NFunction::TCFunctionMovable<void ()> &&_fOnSuspend) const
	{
		return CCoroutineOnSuspendScope(fg_Move(_fOnSuspend));
	}

	CCoroutineOnResumeScope CCoroutineOnResumeHelper::operator / (NFunction::TCFunctionMovable<void ()> &&_fOnResume) const
	{
		try
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			_fOnResume();
		}
		catch (...)
		{
			DMibErrorCoroutineWrapper("Exception on initial 'on resume'", NException::fg_CurrentException());
		}
		return CCoroutineOnResumeScope(fg_Move(_fOnResume));
	}
}

namespace NMib::NConcurrency::NPrivate
{
	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception)
	{
		DMibDTrace("Unobserved exception in future: {}\n", NException::fg_ExceptionString(_Exception));
		DMibLog(Error, "Unobserved exception in future: {}", NException::fg_ExceptionString(_Exception));
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
