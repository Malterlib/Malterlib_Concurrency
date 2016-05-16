// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <exception>
#include "Malterlib_Concurrency_AsyncResult.h"

namespace NMib
{
	namespace NConcurrency
	{
		CAsyncResult::CAsyncResult(CAsyncResult const &_Other) = default;
		CAsyncResult::CAsyncResult(CAsyncResult &&_Other) = default;
		
		void CAsyncResult::f_Access() const
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
		}

		CAsyncResult::CAsyncResult() = default;
		CAsyncResult &CAsyncResult::operator =(CAsyncResult const &_Other) = default;
		CAsyncResult &CAsyncResult::operator =(CAsyncResult &&_Other) = default;
		
		TCAsyncResult<void>::TCAsyncResult() = default;
		TCAsyncResult<void>::TCAsyncResult(TCAsyncResult &&_Other) = default;
		TCAsyncResult<void>::TCAsyncResult(TCAsyncResult const &_Other) = default;
		TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult &&_Other) = default;
		TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult const &_Other) = default;
		
		void TCAsyncResult<void>::f_Get() const
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (!m_bHasBeenSet)
				DMibError("No result specified");
		}
		void TCAsyncResult<void>::f_Get()
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (!m_bHasBeenSet)
				DMibError("No result specified");
		}
		void TCAsyncResult<void>::f_Move()
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (!m_bHasBeenSet)
				DMibError("No result specified");
		}

		void TCAsyncResult<void>::operator *() const
		{
			return f_Get();
		}
		void TCAsyncResult<void>::operator *()
		{
			return f_Get();
		}

		void TCAsyncResult<void>::f_SetException(CExceptionPointer const &_pException)
		{
			m_pException = _pException;
		}
		
		void TCAsyncResult<void>::f_SetException(CExceptionPointer &&_pException)
		{
			m_pException = fg_Move(_pException);
		}
		
		void TCAsyncResult<void>::f_SetCurrentException()
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
			m_pException = fg_CurrentException();
		}

		void TCAsyncResult<void>::f_SetResult()
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
			m_bHasBeenSet = true;
		}

		TCAsyncResult<void>::operator bool () const
		{
			return m_bHasBeenSet;
		}

		bool TCAsyncResult<void>::f_IsSet() const
		{
			return m_bHasBeenSet || m_pException != nullptr;
		}
		
		CExceptionPointer TCAsyncResult<void>::f_GetException() const
		{
			if (m_pException != nullptr)
				return m_pException;
			return nullptr;
		}

		NStr::CStr TCAsyncResult<void>::f_GetExceptionStr() const
		{
			try
			{
				f_Get();
			}
			catch (NException::CException const& _Exception)
			{
				return _Exception.f_GetErrorStr();
			}
			return NStr::CStr();
		}

	}
}
