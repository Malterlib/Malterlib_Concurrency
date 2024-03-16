// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_LogError.h"

namespace NMib::NConcurrency
{
	CLogErrorResultFunctor fg_LogWarning(NStr::CStr const &_Category, NStr::CStr const &_Description)
	{
		return CLogErrorResultFunctor{.m_Category = _Category, .m_Description = _Description, .m_Severity = NLog::ESeverity_Warning};
	}

	CLogErrorResultFunctor fg_LogError(NStr::CStr const &_Category, NStr::CStr const &_Description)
	{
		return CLogErrorResultFunctor{.m_Category = _Category, .m_Description = _Description};
	}

	CLogErrorResultFunctor fg_LogCritical(NStr::CStr const &_Category, NStr::CStr const &_Description)
	{
		return CLogErrorResultFunctor{.m_Category = _Category, .m_Description = _Description, .m_Severity = NLog::ESeverity_Critical};
	}

	CLogError::CLogError(NStr::CStr const &_Category)
		: m_Category(_Category)
	{
	}

	CLogErrorResultFunctor CLogError::operator () (NStr::CStr const &_Description) const
	{
		return fg_LogError(m_Category, _Description);
	}

	CLogErrorResultFunctor CLogError::f_Warning(NStr::CStr const &_Description) const
	{
		return fg_LogWarning(m_Category, _Description);
	}

	CLogErrorResultFunctor CLogError::f_Critical(NStr::CStr const &_Description) const
	{
		return fg_LogCritical(m_Category, _Description);
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

	CLogErrorResultFunctorWithUserError CLogErrorResultFunctor::operator % (NStr::CStr &&_UserError)
	{
		return CLogErrorResultFunctorWithUserError{*this, _UserError};
	}

	void CLogErrorResultFunctor::f_LogException(NException::CExceptionPointer const &_pException) const
	{
		DMibLogCategoryStr(m_Category.f_GetStr());
		if (m_Severity == NLog::ESeverity_Critical)
			DMibLog(Critical, "{}: {}", m_Description, NException::fg_ExceptionString(_pException));
		else if (m_Severity == NLog::ESeverity_Warning)
			DMibLog(Warning, "{}: {}", m_Description, NException::fg_ExceptionString(_pException));
		else
			DMibLog(Error, "{}: {}", m_Description, NException::fg_ExceptionString(_pException));
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CLogErrorResultFunctor const &_LogError
		)
	{
		return [_LogError, fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
			{
				auto pException = fPreviousTransformer(fg_Move(_pException));
				if (!pException)
					return nullptr;

				_LogError.f_LogException(pException);

				return pException;
			}
		;
	}


	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CLogErrorResultFunctorWithUserError const &_LogError
		)
	{
		return [_LogError, fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
			{
				auto pException = fPreviousTransformer(fg_Move(_pException));
				if (!pException)
					return nullptr;

				_LogError.f_LogException(pException);

				return NException::fg_MakeException(DMibErrorInstance(_LogError.m_UserError));
			}
		;
	}

	void operator > (CAsyncResult const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		if (!_Result)
			_LogError.f_LogException(_Result.f_GetException());
	}
}
