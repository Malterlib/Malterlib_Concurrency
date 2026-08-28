// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	struct CLogErrorResultFunctorWithUserError;
	struct CLogErrorResultFunctor
	{
		template <typename tf_CResult>
		void operator() (TCAsyncResult<tf_CResult> &&_Result) const;

		friend void operator > (CAsyncResult const &_Result, CLogErrorResultFunctor const &_LogError);

		template <typename tf_CResult>
		friend void operator > (NContainer::TCVector<TCAsyncResult<tf_CResult>> &&_Result, CLogErrorResultFunctor &&_LogError);

		template <typename tf_CKey, typename tf_CResult>
		friend void operator > (NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CResult>> &&_Result, CLogErrorResultFunctor &&_LogError);

		template <typename tf_FResultHandler>
		auto operator / (tf_FResultHandler &&_fResultHandler) &&;

		CLogErrorResultFunctorWithUserError operator % (NStr::CStr &&_UserError);

		void f_LogException(NException::CExceptionPointer const &_pException) const;

		CLogErrorResultFunctor &&f_IgnoreActorDeleted() &&;

		NStr::CStr m_Category;
		NStr::CStr m_Description;
		NLog::ESeverity m_Severity = NLog::ESeverity_Error;
		bool m_bIgnoreActorDeleted = false;
	};

	struct CLogErrorResultFunctorWithUserError : public CLogErrorResultFunctor
	{
		NStr::CStr m_UserError;
	};

	CLogErrorResultFunctor fg_LogWarning(NStr::CStr const &_Category, NStr::CStr const &_Description);
	CLogErrorResultFunctor fg_LogError(NStr::CStr const &_Category, NStr::CStr const &_Description);
	CLogErrorResultFunctor fg_LogCritical(NStr::CStr const &_Category, NStr::CStr const &_Description);

	// Logs unconsumed failures where the call completes; arguments must be string literals.
	// Actor-deleted failures are normal for weak calls. With warning logging disabled, no result callback is installed.
	#if (DMibSysLogSeverities) & DMibLogSeverity_Warning
		#define DMibLogWarningOrDiscardResult(d_Call, d_Category, d_Description) \
			( \
				(d_Call) > ::NMib::NConcurrency::g_DirectResult \
				/ ::NMib::NConcurrency::fg_LogWarning(::NMib::NStr::gc_Str<d_Category>.m_Str, ::NMib::NStr::gc_Str<d_Description>.m_Str).f_IgnoreActorDeleted() \
			)
	#else
		#define DMibLogWarningOrDiscardResult(d_Call, d_Category, d_Description) ((d_Call).f_DiscardResult())
	#endif

	struct CLogError
	{
		CLogError(NStr::CStr const &_Category);

		CLogErrorResultFunctor operator () (NStr::CStr const &_Description) const;
		CLogErrorResultFunctor f_Warning(NStr::CStr const &_Description) const;
		CLogErrorResultFunctor f_Critical(NStr::CStr const &_Description) const;

		void f_Log(NStr::CStr const &_Description, CAsyncResult const &_Result) const;
		void f_Log(NStr::CStr const &_Description, NException::CExceptionPointer const &_pException) const;

		NStr::CStr m_Category;
	};
}

#include "Malterlib_Concurrency_LogError.hpp"
