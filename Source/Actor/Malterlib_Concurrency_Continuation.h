// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CReturnValue>
		struct TCContinuation;
		
		template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
		struct TCActorCall;
		
		namespace NPrivate
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

			template <typename tf_FFunctor>
			struct TCAllAsyncResultsAreVoid;
		}
		
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
			
			struct CData
			{
				TCAsyncResult<t_CReturnValue> m_Result;
				NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> m_OnResult;
				NAtomic::TCAtomic<mint> m_OnResultSet;

				void fp_ReportNothingSet();
				~CData();
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

			explicit operator bool () const;
			bool f_IsSet() const;
			void f_SetResult() const;
			template <typename tf_CResult>
			void f_SetResult(tf_CResult &&_Result) const;
			template <typename tf_CResult>
			void f_SetException(tf_CResult &&_Result) const;
			void f_SetCurrentException() const;
			
			void f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
			
			template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
			auto operator / (tf_FResultHandler &&_fResultHandler) const;

			template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
			auto operator / (tf_FResultHandler &&_fResultHandler) const;

			template <typename tf_CErrorString>
			auto operator % (tf_CErrorString &&_ErrorString) const;

			template <typename tf_FResultHandler>
			auto operator > (tf_FResultHandler &&_fResultHandler) const;

			template <typename tf_CType>
			auto operator + (TCContinuation<tf_CType> const &_Other) const;

			template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
			auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall);
			
		public:
			NPtr::TCSharedPointer<CData> m_pData;
		};
		
		template <typename t_CReturnValue, typename t_CError>
		struct TCContinuationWithError
		{
			TCContinuationWithError(TCContinuation<t_CReturnValue> const &_Continuation, t_CError const &_Error);

			template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
			auto operator / (tf_FResultHandler &&_fResultHandler) const;

			template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
			auto operator / (tf_FResultHandler &&_fResultHandler) const;

			TCContinuation<t_CReturnValue> m_Continuation;
			t_CError m_Error;
		};
	}
}

#include "Malterlib_Concurrency_Continuation.hpp"
