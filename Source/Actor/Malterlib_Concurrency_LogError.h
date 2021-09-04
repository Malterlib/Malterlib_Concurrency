// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CLogErrorResultFunctor
	{
		CLogErrorResultFunctor(NStr::CStr const &_Category, NStr::CStr const &_Description);

		template <typename tf_CResult>
		void operator() (TCAsyncResult<tf_CResult> &&_Result) const;

		template <typename tf_CResult>
		friend void operator > (TCAsyncResult<tf_CResult> const &_Result, CLogErrorResultFunctor const &_LogError);

		template <typename tf_CResult>
		friend void operator > (NContainer::TCVector<TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError);

		template <typename tf_CKey, typename tf_CResult>
		friend void operator > (NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		NStr::CStr m_Category;
		NStr::CStr m_Description;
	};

	CLogErrorResultFunctor fg_LogError(NStr::CStr const &_Category, NStr::CStr const &_Description);

	struct CLogError
	{
		CLogError(NStr::CStr const &_Category);

		CLogErrorResultFunctor operator () (NStr::CStr const &_Description) const;

		void f_Log(NStr::CStr const &_Description, CAsyncResult const &_Result) const;
		void f_Log(NStr::CStr const &_Description, NException::CExceptionPointer const &_pException) const;

		NStr::CStr m_Category;
	};
}

#include "Malterlib_Concurrency_LogError.hpp"
