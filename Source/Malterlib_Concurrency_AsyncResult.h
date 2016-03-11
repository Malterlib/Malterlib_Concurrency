// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <exception>

#include <Mib/Storage/Variant>
#include "Malterlib_Concurrency_Configuration.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CType>
		class TCAsyncResult
		{
			template <typename t_CType2>
			friend class TCAsyncResult;

			std::exception_ptr m_pException;
			NContainer::TCVariant<void, t_CType> m_Result;
		public:
#if DMibConcurrencyDebugActorCallstacks
			CAsyncCallstacks m_Callstacks;
#endif
		public:
			TCAsyncResult();
			TCAsyncResult(TCAsyncResult const &_Other);
			TCAsyncResult &operator =(TCAsyncResult const &_Other);
			TCAsyncResult(TCAsyncResult &&_Other);
			TCAsyncResult &operator =(TCAsyncResult &&_Other);

			t_CType const &f_Get() const;
			t_CType &f_Get();
			t_CType f_Move();
			t_CType const &operator *() const;
			t_CType &operator *();
			t_CType const *operator ->() const;
			t_CType *operator ->();
			
			template <typename tf_CType>
			void f_SetException(tf_CType &&_Exception);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> && _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> & _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> const& _AsyncResult);
			void f_SetException(std::exception_ptr const &_pException);
			void f_SetException(std::exception_ptr &&_pException);
			
			void f_SetCurrentException();
			template <typename tf_CType>
			void f_SetResult(tf_CType &&_Result);
			
			explicit operator bool () const;
			bool f_IsSet() const;
			
			NStr::CStr f_GetExceptionStr() const;
		};

		template <>
		class TCAsyncResult<void>
		{
			template <typename t_CType2>
			friend class TCAsyncResult;

			std::exception_ptr m_pException;
			bint m_bHasBeenSet;
		public:
#if DMibConcurrencyDebugActorCallstacks
			CAsyncCallstacks m_Callstacks;
#endif
		public:
			TCAsyncResult();
			TCAsyncResult(TCAsyncResult &&_Other);
			TCAsyncResult(TCAsyncResult const &_Other);
			TCAsyncResult &operator =(TCAsyncResult &&_Other);
			TCAsyncResult &operator =(TCAsyncResult const &_Other);
			
			void f_Get() const;
			void f_Get();
			void f_Move();
			void operator *() const;
			void operator *();
			
			template <typename tf_CType>
			void f_SetException(tf_CType &&_Exception);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> && _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> & _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> const& _AsyncResult);
			void f_SetException(std::exception_ptr const &_pException);
			void f_SetException(std::exception_ptr &&_pException);
			
			void f_SetCurrentException();
			void f_SetResult();
			
			explicit operator bool () const;
			bool f_IsSet() const;

			NStr::CStr f_GetExceptionStr() const;
		};
	}
}

#include "Malterlib_Concurrency_AsyncResult.hpp"
