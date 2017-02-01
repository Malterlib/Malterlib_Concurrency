// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <exception>

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
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
		bint m_bHasBeenSet = false;
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
		NStr::CStr f_GetExceptionStr() const;
		CExceptionPointer f_GetException() const;
		
		void f_SetCurrentException();
		void f_SetException(NException::CException const &_Exception);
		void f_SetException(NException::CException &&_Exception);
		void f_SetException(CAsyncResult && _AsyncResult);
		void f_SetException(CAsyncResult const &_AsyncResult);
		void f_SetException(CExceptionPointer const &_pException);
		void f_SetException(CExceptionPointer &&_pException);

		explicit operator bool () const;
		bool f_IsSet() const;
	};
	
	template <typename t_CType = void>
	class TCAsyncResult : public CAsyncResult
	{
		template <typename t_CType2>
		friend class TCAsyncResult;

		NAggregate::TCAggregateSimple<t_CType> m_ResultAggregate;
	public:
		TCAsyncResult() = default;
		~TCAsyncResult();
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
		
		template <typename ...tfp_CType>
		void f_SetResult(tfp_CType && ...p_Result);
		
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream);
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
	};

	template <>
	class TCAsyncResult<void> : public CAsyncResult
	{
		template <typename t_CType2>
		friend class TCAsyncResult;

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
		
		void f_SetResult();

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
	};
}

#include "Malterlib_Concurrency_AsyncResult.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
