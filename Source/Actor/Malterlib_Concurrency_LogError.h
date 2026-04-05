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

		NStr::CStr m_Category;
		NStr::CStr m_Description;
		NLog::ESeverity m_Severity = NLog::ESeverity_Error;
	};

	struct CLogErrorResultFunctorWithUserError : public CLogErrorResultFunctor
	{
		NStr::CStr m_UserError;
	};

	CLogErrorResultFunctor fg_LogWarning(NStr::CStr const &_Category, NStr::CStr const &_Description);
	CLogErrorResultFunctor fg_LogError(NStr::CStr const &_Category, NStr::CStr const &_Description);
	CLogErrorResultFunctor fg_LogCritical(NStr::CStr const &_Category, NStr::CStr const &_Description);

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
