// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
{
	DMibImpErrorClassDefine(CExceptionActorResultWasNotSet, NMib::NException::CException);
#		define DMibErrorActorResultWasNotSet(_Description) DMibImpError(NMib::NConcurrency::CExceptionActorResultWasNotSet, _Description, false)

#		ifndef DMibPNoShortCuts
#			define DErrorActorResultWasNotSet DMibErrorActorResultWasNotSet
#		endif

	/// Common functionality for TCAsyncResult
	struct CAsyncResult
	{
	private:
		template <typename t_CType2>
		friend struct TCAsyncResult;

		enum EDataType
		{
			EDataType_Exception = 0
			, EDataType_HasBeenSet = 1
			, EDataType_None = 2
		};

		union CDataUnion
		{
			CDataUnion();
			~CDataUnion();

			inline_always EDataType f_DataType() const;

			CDataUnion(CDataUnion const &_Other);
			CDataUnion(CDataUnion &&_Other);
			CDataUnion &operator = (CDataUnion const &_Other);
			CDataUnion &operator = (CDataUnion &&_Other);

			NException::CExceptionPointer m_pException;
			mint m_DataType;
		};

		CDataUnion mp_Data;

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

		NException::CExceptionPointer const &f_ExceptionPointer() const;

		static NException::CExceptionPointer const &fs_ResultWasNotSetException();
		static NException::CExceptionPointer const &fs_ActorCalledDeletedException();

		void f_Access() const; ///< Try to access the contained value. Useful to throw the contained exception in case you want to catch and handle it.
		NStr::CStr f_GetExceptionStr() const noexcept; ///< Returns a string for the contained exception.
		NStr::CStr f_GetExceptionCallstackStr(mint _Indent) const; ///< Returns a string for the contained exception.
		NException::CExceptionPointer f_GetException() const & noexcept; ///< Returns the contained exception as an exception pointer
		NException::CExceptionPointer f_GetException() && noexcept; ///< Returns the contained exception as an exception pointer
		template <typename tf_CException>
		bool f_HasExceptionType() const; ///< Returns true if the contained exception is of the specified exception type

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
		void f_SetException(NException::CExceptionPointer const &_pException); ///< Set exception from exception pointer. Usually only used from TCPromise implementation
		void f_SetException(NException::CExceptionPointer &&_pException); ///< Set exception from exception pointer. Usually only used from TCPromise implementation
		void f_SetExceptionAppendable(NException::CExceptionPointer &&_pException); ///< Set exception from exception pointer and append if one already exists

		explicit operator bool () const; ///< Check if async result is has a valid value set: `if (_Result)`
		bool f_IsSet() const; ///< Check if async result is has a valid value set: `if (_Result.f_IsSet())`
	private:
		[[noreturn]] void fp_AccessSlowPath() const;
	};

	/** \brief Used to represent a result of an async operation

	 Either contains an exception or the result value. If the result is an exception the exception will be thrown if you try to access the value.
	*/
	template <typename t_CType = void>
	struct [[nodiscard]] TCAsyncResult : public CAsyncResult
	{
		template <typename t_CType2>
		friend struct TCAsyncResult;

		NStorage::TCAggregateSimple<t_CType> m_ResultAggregate;
	public:
		TCAsyncResult() = default;
		~TCAsyncResult();
		TCAsyncResult(TCAsyncResult const &_Other);
		TCAsyncResult &operator =(TCAsyncResult const &_Other);
		TCAsyncResult(TCAsyncResult &&_Other);
		TCAsyncResult &operator =(TCAsyncResult &&_Other);

		void f_Clear();

		inline_always t_CType const &f_Get() const;
		inline_always t_CType &f_Get();
		inline_always t_CType f_Move();
		inline_always t_CType const &operator *() const;
		inline_always t_CType &operator *();
		inline_always t_CType const *operator ->() const;
		inline_always t_CType *operator ->();
		
		template <typename ...tfp_CType>
		void f_SetResult(tfp_CType && ...p_Result);
		void f_SetResult(TCAsyncResult const &_Result);
		void f_SetResult(TCAsyncResult &_Result);
		void f_SetResult(TCAsyncResult &&_Result);
		t_CType &f_PrepareResult();

		template <typename tf_CStream>
		void f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const;
		template <typename tf_CStream>
		void f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion);
		template <typename tf_CStream>
		void f_ConsumeWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion);
	};

	template <>
	struct [[nodiscard]] TCAsyncResult<void> : public CAsyncResult
	{
		template <typename t_CType2>
		friend struct TCAsyncResult;

	public:
		TCAsyncResult();
		~TCAsyncResult();
		TCAsyncResult(TCAsyncResult &&_Other);
		TCAsyncResult(TCAsyncResult const &_Other);
		TCAsyncResult &operator =(TCAsyncResult &&_Other);
		TCAsyncResult &operator =(TCAsyncResult const &_Other);

		void f_Clear();

		CVoidTag f_Get() const;
		CVoidTag f_Get();
		CVoidTag f_Move();
		CVoidTag operator *() const;
		CVoidTag operator *();
		
		void f_SetResult();
		void f_SetResult(TCAsyncResult const &_Result);
		void f_SetResult(TCAsyncResult &_Result);
		void f_SetResult(TCAsyncResult &&_Result);

		template <typename tf_CStream>
		void f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const;
		template <typename tf_CStream>
		void f_ConsumeWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion);
	};
#if !DMibConfig_Concurrency_DebugActorCallstacks

#ifdef DPlatformFamily_Windows
	static_assert(sizeof(TCAsyncResult<void>) == sizeof(void *) * 2);
#else
	static_assert(sizeof(TCAsyncResult<void>) == sizeof(void *));
#endif
	
#endif

}

namespace NMib::NFunction
{
	extern template struct TCFunctionMovable<void (NConcurrency::TCAsyncResult<void> &&)>;
}

#include "Malterlib_Concurrency_AsyncResult.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
