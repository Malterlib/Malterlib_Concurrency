// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	struct TCContinuation;
	
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
	struct TCActorCall;
	
	class CActor;

	template <typename t_CActor>
	class TCActor;

	template <typename t_CReturnValue>
	struct TCContinuation;
		
	template <typename t_CReturn>
	using TCDispatchedActorCall = 
		TCActorCall
		<
			TCActor<CActor>
			, TCContinuation<t_CReturn> (CActor::*)(NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()> &&)
			, NContainer::TCTuple<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>>
			, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>> 
		>
	;
}
		
namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnValue, typename t_CException = void>
	struct TCRunProtectedHelper
	{
		template <typename tf_FToRun>
		TCContinuation<t_CReturnValue> operator > (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue>
	struct TCRunProtectedHelper<t_CReturnValue, void>
	{
		template <typename tf_FToRun>
		TCContinuation<t_CReturnValue> operator > (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CException>
	struct TCRunProtectedHelper<void, t_CException>
	{
		template <typename tf_FToRun>
		TCContinuation<void> operator > (tf_FToRun &&_ToRun) const;
	};
	
	template <>
	struct TCRunProtectedHelper<void, void>
	{
		template <typename tf_FToRun>
		TCContinuation<void> operator > (tf_FToRun &&_ToRun) const;
	};

	template <typename t_FFunctor>
	struct TCAllAsyncResultsAreVoid;
	
	template <typename t_CContinuation>
	struct TCContinuationReceiveAnyFunctor;

	template <typename t_CReturnValue>
	struct TCContinuationData : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		TCAsyncResult<t_CReturnValue> m_Result;
		NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> m_OnResult;
		NAtomic::TCAtomic<mint> m_OnResultSet;

		void fp_ReportNothingSet();
		~TCContinuationData();
		void fp_OnResult();
		void f_SetResult();
		void f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result);
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> const &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &&_Result);
		void f_SetCurrentException();
	};
}

template <typename t_CReturnValue>
struct NMib::NPtr::TCHasIntrusiveRefcount<NMib::NConcurrency::NPrivate::TCContinuationData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CReturnValue>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::NPrivate::TCContinuationData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{ 
};

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	struct TCContinuationWithError;

	/// \brief Used to defer the return of a value to allow async programming
	template <typename t_CReturnValue = void>
	struct TCContinuation
	{
	public:
		TCContinuation(TCContinuation const &_Other);
		TCContinuation &operator =(TCContinuation const &_Other);
		TCContinuation &operator =(TCContinuation &&_Other);
		TCContinuation(TCContinuation &&_Other);
		TCContinuation();

		template <typename tf_CType>
		TCContinuation(TCExplicit<tf_CType> &&_Data);
		TCContinuation(TCExplicit<void> &&_Data);

		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		TCContinuation(tf_CType &&_Exception);

		TCContinuation(CExceptionPointer const &_pException);
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		TCContinuation(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall);
		
		TCContinuation(TCAsyncResult<t_CReturnValue> const &_Result);
		TCContinuation(TCAsyncResult<t_CReturnValue> &&_Result);

		template <typename tf_CResult>
		static TCContinuation fs_Finished(tf_CResult &&_Result);
		static TCContinuation fs_Finished();

		static NPrivate::TCRunProtectedHelper<t_CReturnValue> fs_RunProtected();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> fs_RunProtected();
		
		explicit operator bool () const;
		bool f_IsSet() const;
		void f_SetResult() const;
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result) const;
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result) const;
		template <typename tf_CException, typename tf_CResult>
		void f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const;
		void f_SetCurrentException() const;
		auto f_ReceiveAny() const -> NPrivate::TCContinuationReceiveAnyFunctor<TCContinuation>;
		
		void f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
		
		TCDispatchedActorCall<t_CReturnValue> f_Dispatch() const;
		auto f_CallSync(fp64 _Timeout) const;
		
		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		auto operator % (NStr::CStr const &_ErrorString) const -> TCContinuationWithError<t_CReturnValue>;

		template <typename tf_FResultHandler>
		void operator > (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_CReturnValue>
		void operator > (TCContinuation<tf_CReturnValue> const &_Continuation) const;

		template <typename tf_CType>
		auto operator + (TCContinuation<tf_CType> const &_Other) const;

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall);
		
	public:
		NPtr::TCSharedPointer<NPrivate::TCContinuationData<t_CReturnValue>> m_pData;
	};
	
	template <typename t_CReturnValue>
	struct TCContinuationWithError
	{
		TCContinuationWithError(TCContinuation<t_CReturnValue> const &_Continuation, NStr::CStr const &_Error);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuation<t_CReturnValue> m_Continuation;
		NStr::CStr m_Error;
	};
}

#include "Malterlib_Concurrency_Continuation.hpp"
