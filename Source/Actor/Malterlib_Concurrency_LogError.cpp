// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_LogError.h"

namespace NMib::NConcurrency
{
	CLogErrorResultFunctor::CLogErrorResultFunctor(NStr::CStr const &_Category, NStr::CStr const &_Description)
		: m_Category(_Category)
		, m_Description(_Description)
	{
	}

	CLogErrorResultFunctor fg_LogError(NStr::CStr const &_Category, NStr::CStr const &_Description)
	{
		return CLogErrorResultFunctor(_Category, _Description);
	}

	CLogError::CLogError(NStr::CStr const &_Category)
		: m_Category(_Category)
	{
	}

	CLogErrorResultFunctor CLogError::operator () (NStr::CStr const &_Description) const
	{
		return fg_LogError(m_Category, _Description);
	}

	void CLogError::f_Log(NStr::CStr const &_Description, CAsyncResult const &_Result) const
	{
		DMibLogCategoryStr(m_Category.f_GetStr());
		DMibLog(Error, "{}: {}", _Description, _Result.f_GetExceptionStr());
	}

	void CLogError::f_Log(NStr::CStr const &_Description, NException::CExceptionPointer const &_pException) const
	{
		DMibLogCategoryStr(m_Category.f_GetStr());
		DMibLog(Error, "{}: {}", _Description, NException::fg_ExceptionString(_pException));
	}
}
