// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CResult>
	void CLogErrorResultFunctor::operator() (TCAsyncResult<tf_CResult> &&_Result) const
	{
		if (!_Result)
		{
			DMibLogCategoryStr(m_Category.f_GetStr());
			DMibLog(Error, "{}: {}", m_Description, _Result.f_GetExceptionStr());
		}
	}

	template <typename tf_CResult>
	void operator > (TCAsyncResult<tf_CResult> const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		if (!_Result)
		{
			DMibLogCategoryStr(_LogError.m_Category.f_GetStr());
			DMibLog(Error, "{}: {}", _LogError.m_Description, _Result.f_GetExceptionStr());
		}
	}

	template <typename tf_CResult>
	void operator > (NContainer::TCVector<TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
			{
				DMibLogCategoryStr(_LogError.m_Category.f_GetStr());
				DMibLog(Error, "{}: {}", _LogError.m_Description, Result.f_GetExceptionStr());
			}
		}
	}

	template <typename tf_CKey, typename tf_CResult>
	void operator > (NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CResult>> const &_Result, CLogErrorResultFunctor const &_LogError)
	{
		for (auto &Result : _Result)
		{
			if (!Result)
			{
				DMibLogCategoryStr(_LogError.m_Category.f_GetStr());
				DMibLog(Error, "{}: {}", _LogError.m_Description, Result.f_GetExceptionStr());
			}
		}
	}

	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto CLogErrorResultFunctor::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Category = m_Category, Description = m_Description]
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
								DMibLogCategoryStr(Category.f_GetStr());
								DMibLog(Error, "{}: {}", Description, p_Results.f_GetExceptionStr());
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
		return [Promise = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Category = m_Category, Description = m_Description]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!p_Results)
							{
								DMibLogCategoryStr(Category.f_GetStr());
								DMibLog(Error, "{}: {}", Description, p_Results.f_GetExceptionStr());
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
}
