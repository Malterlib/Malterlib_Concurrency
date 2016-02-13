// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CReturnValue>
		struct TCContinuation;
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

		}
		template <typename t_CReturnValue>
		struct TCContinuation
		{
		public:
			TCContinuation(TCContinuation const &_Other);
			TCContinuation &operator =(TCContinuation const &_Other);
			TCContinuation &operator =(TCContinuation &&_Other);
			TCContinuation(TCContinuation &&_Other);
			TCContinuation();
			
			template <typename tf_CResult>
			static TCContinuation fs_Finished(tf_CResult &&_Result);
			static TCContinuation fs_Finished();

			static NPrivate::TCRunProtectedHelper<t_CReturnValue> fs_RunProtected();
			template <typename tf_CException>
			static NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> fs_RunProtected();
			
			struct CData
			{
				TCAsyncResult<t_CReturnValue> m_Result;
				NFunction::TCFunction<void ()> m_OnResult;
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
			
		public:
			NPtr::TCSharedPointer<CData> m_pData;
		};
	}
}

#include "Malterlib_Concurrency_Continuation.hpp"
