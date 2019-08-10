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
	
	CMakeFutureHelper g_Future;
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
