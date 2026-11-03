// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CResult>
	void CLogErrorResultFunctor::operator () (TCAsyncResult<tf_CResult> &&_Result) const
	{
		if (!_Result)
			f_LogException(_Result.f_GetException());
	}

	template <typename tf_CResult>
	void operator > (NContainer::TCVector<TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_fLogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
				_fLogError.f_LogException(Result.f_GetException());
		}
	}

	template <typename tf_CKey, typename tf_CResult>
	void operator > (NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_fLogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
				_fLogError.f_LogException(Result.f_GetException());
		}
	}

	template <typename tf_FResultHandler>
	auto CLogErrorResultFunctor::operator / (tf_FResultHandler &&_fResultHandler) &&
	{
		return [fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), fLogError = *this](auto && ...p_Results) mutable
			{
				bool bFailed = false;
				(
					[&]
					{
						if (!p_Results)
						{
							fLogError.f_LogException(p_Results.f_GetException());
							bFailed = true;
						}
					}
					()
					, ...
				);

				if (bFailed)
					return;

				if constexpr (NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value)
					fResultHandler();
				else
					fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	FExceptionTransformer fg_ExceptionTransformer(FExceptionTransformer &&_fPreviousTransformer, CLogErrorResultFunctor &&_fLogError);

	FExceptionTransformer fg_ExceptionTransformer(FExceptionTransformer &&_fPreviousTransformer, CLogErrorResultFunctorWithUserError &&_fLogError);
}
