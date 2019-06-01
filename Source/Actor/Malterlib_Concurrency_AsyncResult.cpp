// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <exception>
#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_AsyncResult.h"

namespace NMib::NConcurrency
{
	CAsyncResult::CAsyncResult(CAsyncResult const &_Other) = default;
	CAsyncResult::CAsyncResult(CAsyncResult &&_Other) = default;
	CAsyncResult::CAsyncResult() = default;
	CAsyncResult &CAsyncResult::operator =(CAsyncResult const &_Other) = default;
	CAsyncResult &CAsyncResult::operator =(CAsyncResult &&_Other) = default;
	
	void CAsyncResult::f_Access() const
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibError("No result specified");
		}
	}

	NException::CExceptionPointer CAsyncResult::f_GetException() const
	{
		if (m_pException != nullptr)
			return m_pException;
		return nullptr;
	}

	NStr::CStr CAsyncResult::f_GetExceptionStr() const
	{
		try
		{
			f_Access();
		}
		catch (NException::CException const& _Exception)
		{
			return _Exception.f_GetErrorStr();
		}
		return NStr::CStr("No error (no exception was set)");
	}
	
	NStr::CStr CAsyncResult::f_GetExceptionCallstackStr(mint _Indent) const
	{
		try
		{
			f_Access();
		}
		catch (NException::CException const& _Exception)
		{
			return _Exception.f_GetCallstackStr(_Indent);
		}
		return NStr::CStr();
	}
	
	TCAsyncResult<void>::TCAsyncResult() = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult const &_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult const &_Other) = default;
	
	CVoidTag TCAsyncResult<void>::f_Get() const
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibError("No result specified");
		}
		return CVoidTag();
	}
	
	CVoidTag TCAsyncResult<void>::f_Get()
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibError("No result specified");
		}
		return CVoidTag();
	}
	
	CVoidTag TCAsyncResult<void>::f_Move()
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibError("No result specified");
		}
		return CVoidTag();
	}

	CVoidTag TCAsyncResult<void>::operator *() const
	{
		return f_Get();
	}
	
	CVoidTag TCAsyncResult<void>::operator *()
	{
		return f_Get();
	}

	void CAsyncResult::f_SetException(NException::CExceptionPointer const &_pException)
	{
		m_pException = _pException;
	}
	
	void CAsyncResult::f_SetException(NException::CExceptionPointer &&_pException)
	{
		m_pException = fg_Move(_pException);
	}

	void CAsyncResult::f_SetException(CAsyncResult &&_AsyncResult)
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_pException = fg_Move(_AsyncResult.m_pException);
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_Callstacks = fg_Move(_AsyncResult.m_Callstacks);
#endif
	}

	void CAsyncResult::f_SetException(CAsyncResult const &_AsyncResult)
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_pException = _AsyncResult.m_pException;
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_Callstacks = _AsyncResult.m_Callstacks;
#endif
	}
	
	void CAsyncResult::f_SetCurrentException()
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_pException = NException::fg_CurrentException();
	}

	void TCAsyncResult<void>::f_SetResult()
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_bHasBeenSet = true;
	}

	CAsyncResult::operator bool () const
	{
		return m_bHasBeenSet;
	}

	bool CAsyncResult::f_IsSet() const
	{
		return m_bHasBeenSet || m_pException != nullptr;
	}
}

namespace NMib::NFunction
{
	template struct TCFunctionMovable<void (NConcurrency::TCAsyncResult<void> &&)>;
}
