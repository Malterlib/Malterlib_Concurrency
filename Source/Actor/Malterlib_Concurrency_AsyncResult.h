// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
{
	/// Common functionality for TCAsyncResult
	class CAsyncResult
	{
		template <typename t_CType2>
		friend class TCAsyncResult;
		
		CExceptionPointer m_pException;
		bint m_bHasBeenSet = false;
	public:
#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks m_Callstacks;
#endif
	public:
		CAsyncResult();
		CAsyncResult(CAsyncResult const &_Other);
		CAsyncResult(CAsyncResult &&_Other);
		CAsyncResult &operator =(CAsyncResult const &_Other);
		CAsyncResult &operator =(CAsyncResult &&_Other);
		
		void f_Access() const; ///< Try to access the contained value. Useful to throw the contained exception in case you want to catch and handle it.
		NStr::CStr f_GetExceptionStr() const; ///< Returns a string for the contained exception.
		NStr::CStr f_GetExceptionCallstackStr(mint _Indent) const; ///< Returns a string for the contained exception.
		CExceptionPointer f_GetException() const; ///< Returns the contained exception as a exception pointer
		
		void f_SetCurrentException(); ///< Sets the result to the current exception. Usually only used from TCPromise implementation
		template 
		<
			typename tf_CException
			, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CException>::CType, NException::CExceptionBase>::mc_Value> * = nullptr
		>
		void f_SetException(tf_CException &&_Exception); ///< Set exception instance directly. Usually used with DMibErrorInstance("Error string") or equivialent. 
														 ///< Usually only used from TCPromise implementation
		void f_SetException(CAsyncResult && _AsyncResult); ///< Set exception from another async result. Usually only used from TCPromise implementation
		void f_SetException(CAsyncResult const &_AsyncResult); ///< Set exception from another async result. Usually only used from TCPromise implementation
		void f_SetException(CExceptionPointer const &_pException); ///< Set exception from exception pointer. Usually only used from TCPromise implementation
		void f_SetException(CExceptionPointer &&_pException); ///< Set exception from exception pointer. Usually only used from TCPromise implementation

		explicit operator bool () const; ///< Check if async result is has a valid value set: `if (_Result)`
		bool f_IsSet() const; ///< Check if async result is has a valid value set: `if (_Result.f_IsSet())`
	};
	
	/** \brief Used to represent a result of an async operation

	 Either contains an exception or the result value. If the result is an exception the exception will be thrown if you try to access the value.
	*/
	template <typename t_CType = void>
	class TCAsyncResult : public CAsyncResult
	{
		template <typename t_CType2>
		friend class TCAsyncResult;

		NStorage::TCAggregateSimple<t_CType> m_ResultAggregate;
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
		void f_SetResult(TCAsyncResult const &_Result);
		void f_SetResult(TCAsyncResult &_Result);
		void f_SetResult(TCAsyncResult &&_Result);

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
