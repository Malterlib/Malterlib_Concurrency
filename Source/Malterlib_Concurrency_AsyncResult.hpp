// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <exception>

#include <Mib/Storage/Variant>

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CType>
		TCAsyncResult<t_CType>::TCAsyncResult()
		{
		}
		template <typename t_CType>
		TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult const &_Other)
			: m_pException(_Other.m_pException)
			, m_Result(_Other.m_Result)
#if DMibConcurrencyDebugActorCallstacks
			, m_Callstacks(_Other.m_Callstacks)
#endif
		{
		}
		template <typename t_CType>
		TCAsyncResult<t_CType> &TCAsyncResult<t_CType>::operator =(TCAsyncResult const &_Other)
		{
			m_pException = _Other.m_pException;
			m_Result = _Other.m_Result;
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = _Other.m_Callstacks;
#endif
			return *this;
		}
		template <typename t_CType>
		TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult &&_Other)
			: m_pException(fg_Move(_Other.m_pException))
			, m_Result(fg_Move(_Other.m_Result))
#if DMibConcurrencyDebugActorCallstacks
			, m_Callstacks(fg_Move(_Other.m_Callstacks))
#endif
		{
		}

		template <typename t_CType>
		TCAsyncResult<t_CType> &TCAsyncResult<t_CType>::operator =(TCAsyncResult &&_Other)
		{
			m_pException = fg_Move(_Other.m_pException);
			m_Result = fg_Move(_Other.m_Result);
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = fg_Move(_Other.m_Callstacks);
#endif
			return *this;
		}

		template <typename t_CType>
		t_CType const &TCAsyncResult<t_CType>::f_Get() const
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (m_Result.template f_IsOfType<void>())
				DMibError("No result specified");

			return m_Result.template f_GetAsType<t_CType>();
		}
		template <typename t_CType>
		t_CType &TCAsyncResult<t_CType>::f_Get()
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (m_Result.template f_IsOfType<void>())
				DMibError("No result specified");

			return m_Result.template f_GetAsType<t_CType>();
		}
		template <typename t_CType>
		t_CType TCAsyncResult<t_CType>::f_Move()
		{
			if (m_pException != nullptr)
				std::rethrow_exception(m_pException);
			else if (m_Result.template f_IsOfType<void>())
				DMibError("No result specified");

			return fg_Move(m_Result.template f_GetAsType<t_CType>());
		}

		template <typename t_CType>
		t_CType const &TCAsyncResult<t_CType>::operator *() const
		{
			return f_Get();;
		}
		template <typename t_CType>
		t_CType &TCAsyncResult<t_CType>::operator *()
		{
			return f_Get();;
		}

		template <typename t_CType>
		t_CType const *TCAsyncResult<t_CType>::operator ->() const
		{
			return &f_Get();;
		}
		template <typename t_CType>
		t_CType *TCAsyncResult<t_CType>::operator ->()
		{
			return &f_Get();;
		}

		template <typename t_CType>
		template <typename tf_CType>
		void TCAsyncResult<t_CType>::f_SetException(tf_CType &&_Exception)
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
#if defined(DCompiler_MSVC) && DMibCompilerVersion < 1700
			try
			{
				throw _Exception;
			}
			catch (...)
			{
				m_pException = std::current_exception();
			}
#else
			m_pException = std::make_exception_ptr(fg_Forward<tf_CType>(_Exception));
#endif
		}

		template <typename t_CType>
		template <typename tf_CType>
		void TCAsyncResult<t_CType>::f_SetException(TCAsyncResult<tf_CType> &&_AsyncResult)
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
			m_pException = fg_Move(_AsyncResult.m_pException);
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = fg_Move(_AsyncResult.m_Callstacks);
#endif

		}

		template <typename t_CType>
		template <typename tf_CType>
		void TCAsyncResult<t_CType>::f_SetException(TCAsyncResult<tf_CType> &_AsyncResult)
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
			m_pException = _AsyncResult.m_pException;
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = _AsyncResult.m_Callstacks;
#endif
		}

		template <typename t_CType>
		template <typename tf_CType>
		void TCAsyncResult<t_CType>::f_SetException(TCAsyncResult<tf_CType> const&_AsyncResult)
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
			m_pException = _AsyncResult.m_pException;
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = _AsyncResult.m_Callstacks;
#endif
		}
		
		template <typename t_CType>
		void TCAsyncResult<t_CType>::f_SetException(std::exception_ptr const &_pException)
		{
			m_pException = _pException;
		}
		
		template <typename t_CType>
		void TCAsyncResult<t_CType>::f_SetException(std::exception_ptr &&_pException)
		{
			m_pException = fg_Move(_pException);
		}

		template <typename t_CType>
		void TCAsyncResult<t_CType>::f_SetCurrentException()
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
			m_pException = std::current_exception();
		}

		template <typename t_CType>
		template <typename tf_CType>
		void TCAsyncResult<t_CType>::f_SetResult(tf_CType &&_Result)
		{
			DMibRequire(m_Result.template f_IsOfType<void>());
			DMibRequire(!m_pException);
			m_Result = fg_Forward<tf_CType>(_Result);
		}

		template <typename t_CType>
		TCAsyncResult<t_CType>::operator bool () const
		{
			return !m_Result.template f_IsOfType<void>();
		}

		template <typename t_CType>
		bool TCAsyncResult<t_CType>::f_IsSet() const
		{
			return !m_Result.template f_IsOfType<void>() || m_pException != nullptr;
		}

		template <typename t_CType>
		NStr::CStr TCAsyncResult<t_CType>::f_GetExceptionStr() const
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
		
		template <typename tf_CType>
		void TCAsyncResult<void>::f_SetException(tf_CType &&_Exception)
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
#if defined(DCompiler_MSVC) && DMibCompilerVersion < 1700
			try
			{
				throw _Exception;
			}
			catch (...)
			{
				m_pException = std::current_exception();
			}
#else
			m_pException = std::make_exception_ptr(fg_Forward<tf_CType>(_Exception));
#endif
		}

		template <typename tf_CType>
		void TCAsyncResult<void>::f_SetException(TCAsyncResult<tf_CType> && _AsyncResult)
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
			m_pException = fg_Move(_AsyncResult.m_pException);
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = fg_Move(_AsyncResult.m_Callstacks);
#endif
		}

		template <typename tf_CType>
		void TCAsyncResult<void>::f_SetException(TCAsyncResult<tf_CType> & _AsyncResult)
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
			m_pException = _AsyncResult.m_pException;
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = _AsyncResult.m_Callstacks;
#endif
		}

		template <typename tf_CType>
		void TCAsyncResult<void>::f_SetException(TCAsyncResult<tf_CType> const& _AsyncResult)
		{
			DMibRequire(!m_bHasBeenSet);
			DMibRequire(!m_pException);
			m_pException = _AsyncResult.m_pException;
#if DMibConcurrencyDebugActorCallstacks
			m_Callstacks = _AsyncResult.m_Callstacks;
#endif
		}
		
	}
}
