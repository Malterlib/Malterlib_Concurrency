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
		using CExceptionPointer = std::exception_ptr;
		
		template <typename tf_CException>
		CExceptionPointer fg_ExceptionPointer(tf_CException _Exception)
		{
			return std::make_exception_ptr(_Exception);
		}

		inline_always CExceptionPointer fg_CurrentException()
		{
			return std::current_exception();
		}
		
		class CAsyncResult
		{
			template <typename t_CType2>
			friend class TCAsyncResult;
			
			CExceptionPointer m_pException;
		public:
#if DMibConcurrencyDebugActorCallstacks
			CAsyncCallstacks m_Callstacks;
#endif
		public:
			CAsyncResult();
			CAsyncResult(CAsyncResult const &_Other);
			CAsyncResult(CAsyncResult &&_Other);
			CAsyncResult &operator =(CAsyncResult const &_Other);
			CAsyncResult &operator =(CAsyncResult &&_Other);
			
			void f_Access() const;
		};
		
		template <typename t_CType>
		class TCAsyncResult : public CAsyncResult
		{
			template <typename t_CType2>
			friend class TCAsyncResult;

			NContainer::TCVariant<void, t_CType> m_Result;
		public:
			TCAsyncResult() = default;
			TCAsyncResult(TCAsyncResult const &_Other) = default;
			TCAsyncResult &operator =(TCAsyncResult const &_Other) = default;
			TCAsyncResult(TCAsyncResult &&_Other) = default;
			TCAsyncResult &operator =(TCAsyncResult &&_Other) = default;

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
			void f_SetException(CExceptionPointer const &_pException);
			void f_SetException(CExceptionPointer &&_pException);
			
			void f_SetCurrentException();
			template <typename tf_CType>
			void f_SetResult(tf_CType &&_Result);
			
			explicit operator bool () const;
			bool f_IsSet() const;
			
			NStr::CStr f_GetExceptionStr() const;
			CExceptionPointer f_GetException() const;
		};

		template <>
		class TCAsyncResult<void> : public CAsyncResult
		{
			template <typename t_CType2>
			friend class TCAsyncResult;

			bint m_bHasBeenSet = false;
		public:
			TCAsyncResult();
			TCAsyncResult(TCAsyncResult &&_Other);
			TCAsyncResult(TCAsyncResult const &_Other);
			TCAsyncResult &operator =(TCAsyncResult &&_Other);
			TCAsyncResult &operator =(TCAsyncResult const &_Other);
			
			CVoidTag f_Get() const;
			CVoidTag f_Get();
			CVoidTag f_Move();
			CVoidTag operator *() const;
			CVoidTag operator *();
			
			template <typename tf_CType>
			void f_SetException(tf_CType &&_Exception);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> && _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> & _AsyncResult);
			template <typename tf_CType>
			void f_SetException(TCAsyncResult<tf_CType> const& _AsyncResult);
			void f_SetException(CExceptionPointer const &_pException);
			void f_SetException(CExceptionPointer &&_pException);
			
			void f_SetCurrentException();
			void f_SetResult();
			
			explicit operator bool () const;
			bool f_IsSet() const;

			NStr::CStr f_GetExceptionStr() const;
			CExceptionPointer f_GetException() const;
		};
	}
}

#include "Malterlib_Concurrency_AsyncResult.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
