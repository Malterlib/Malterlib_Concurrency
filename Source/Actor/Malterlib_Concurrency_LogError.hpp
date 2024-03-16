// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CResult>
	void CLogErrorResultFunctor::operator() (TCAsyncResult<tf_CResult> &&_Result) const
	{
		if (!_Result)
			f_LogException(_Result.f_GetException());
	}

	template <typename tf_CResult>
	void operator > (NContainer::TCVector<TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
				_LogError.f_LogException(Result.f_GetException());
		}
	}

	template <typename tf_CKey, typename tf_CResult>
	void operator > (NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
				_LogError.f_LogException(Result.f_GetException());
		}
	}

	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto CLogErrorResultFunctor::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), This = *this]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[
							&
						]
						{
							if (!p_Results)
							{
								This.f_LogException(p_Results.f_GetException());
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				if (bFailed)
					return;
				(void)Dummy;
				fResultHandler();
			}
		;
	}

	template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto CLogErrorResultFunctor::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), This = *this]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!p_Results)
							{
								This.f_LogException(p_Results.f_GetException());
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				if (bFailed)
					return;
				fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CLogErrorResultFunctor const &_LogError
		)
	;

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CLogErrorResultFunctorWithUserError const &_LogError
		)
	;
}
