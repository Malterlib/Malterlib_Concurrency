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

		// The call finding its actor already deleted is ordinary teardown for a job queued
		// against a weak actor, not an error to report
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

	// For an actor call whose value nobody consumes: a discarded result takes any exception
	// with it unreported, so where warning logging is compiled in the call's failure is logged
	// at that severity under the category, and where it is compiled out the result is discarded
	// outright and the call pays for no result callback. The log runs where the call completes
	// rather than on a calling actor, so the call may be made from any thread, an io loop's
	// included. A call that found its actor deleted is not logged: the callers are jobs queued
	// against weak actors, and the actor going away under one of them is ordinary teardown.
	// Both arguments are string literals
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
