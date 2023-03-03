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
		if (m_pException)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibError("No result specified");
		}
	}

	namespace
	{
		NException::CExceptionPointer const &fg_NoResultException() noexcept
		{
			CConcurrencyThreadLocal &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (!ThreadLocal.m_pNoResultException)
				ThreadLocal.m_pNoResultException = fg_MakeException(DMibErrorInstance("No result specified"));
			return ThreadLocal.m_pNoResultException;
		}
	}

	NException::CExceptionPointer CAsyncResult::f_GetException() const & noexcept
	{
		if (m_pException)
			return m_pException;
		else if (!m_bHasBeenSet)
			return fg_NoResultException();
		return nullptr;
	}

	NException::CExceptionPointer CAsyncResult::f_GetException() && noexcept
	{
		if (m_pException)
			return m_pException;
		else if (!m_bHasBeenSet)
			return fg_NoResultException();
		return nullptr;
	}

	NStr::CStr CAsyncResult::f_GetExceptionStr() const noexcept
	{
		if (m_pException)
		{
			NStr::CStr Return;
			if
				(
					!NException::fg_VisitException<NException::CExceptionBase>
					(
						m_pException
						, [&](NException::CExceptionBase const& _Exception)
						{
							Return = _Exception.f_GetErrorStr();
						}
					)
				)
			{
				return NStr::gc_Str<"Unknown exception type">;
			}

			return Return;
		}
		else if (!m_bHasBeenSet)
			return NStr::gc_Str<"No result specified">;

		return NStr::gc_Str<"No error (no exception was set)">;
	}
	
	NStr::CStr CAsyncResult::f_GetExceptionCallstackStr(mint _Indent) const
	{
		if (m_pException)
		{
			NStr::CStr Return;
			NException::fg_VisitException<NException::CExceptionBase>
				(
					m_pException
					, [&](NException::CExceptionBase const& _Exception)
					{
						Return = _Exception.f_GetCallstackStr(_Indent);
					}
				)
			;

			return Return;
		}

		return {};
	}
	
	TCAsyncResult<void>::TCAsyncResult() = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult const &_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult const &_Other) = default;

	TCAsyncResult<void>::~TCAsyncResult()
	{
		m_pException = nullptr;
		m_bHasBeenSet = false;
	}

	void TCAsyncResult<void>::f_Clear()
	{
		m_pException = nullptr;
		m_bHasBeenSet = false;
	}

	CVoidTag TCAsyncResult<void>::f_Get() const
	{
		if (m_pException)
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
		if (m_pException)
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
		if (m_pException)
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


	void TCAsyncResult<void>::f_SetResult(TCAsyncResult const &_Result)
	{
		*this = _Result;
	}

	void TCAsyncResult<void>::f_SetResult(TCAsyncResult &_Result)
	{
		*this = _Result;
	}

	void TCAsyncResult<void>::f_SetResult(TCAsyncResult &&_Result)
	{
		*this = fg_Move(_Result);
	}

	void CAsyncResult::f_SetException(NException::CExceptionPointer const &_pException)
	{
		m_pException = _pException;
	}
	
	void CAsyncResult::f_SetException(NException::CExceptionPointer &&_pException)
	{
		m_pException = fg_Move(_pException);
	}

	void CAsyncResult::f_SetExceptionAppendable(NException::CExceptionPointer &&_pException)
	{
		if (f_IsSet())
		{
			if (*this)
				*this = {};
			else
			{
				NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
				ErrorCollector.f_AddError(fg_Move(*this).f_GetException());
				ErrorCollector.f_AddError(fg_Move(_pException));

				*this = {};
				f_SetException(fg_Move(ErrorCollector).f_GetException());

				return;
			}
		}

		f_SetException(_pException);
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
		return m_bHasBeenSet || m_pException;
	}
}

namespace NMib::NFunction
{
	template struct TCFunctionMovable<void (NConcurrency::TCAsyncResult<void> &&)>;
}
