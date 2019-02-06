// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCPromise<tf_CReturnValue> const &_Promise, CDistributedAppAuditor const &_Auditor)
	{
		return TCPromiseWithAppAuditor<tf_CReturnValue>(_Promise, _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCPromiseWithError<tf_CReturnValue> const &_Promise, CDistributedAppAuditor const &_Auditor)
	{
		return TCPromiseWithErrorWithAppAuditor<tf_CReturnValue>(_Promise, _Auditor);
	}

	template <typename t_CReturnValue>
	TCPromiseWithAppAuditor<t_CReturnValue>::TCPromiseWithAppAuditor(TCPromise<t_CReturnValue> const &_Promise, CDistributedAppAuditor const &_Auditor)
		: m_Promise(_Promise)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Auditor.f_Error(p_Results.f_GetExceptionStr());
								Promise.f_SetException(fg_Move(p_Results));
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
				fResultHandler();
			}
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Auditor.f_Error(p_Results.f_GetExceptionStr());
								Promise.f_SetException(fg_Move(p_Results));
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

	template <typename t_CReturnValue>
	TCPromiseWithErrorWithAppAuditor<t_CReturnValue>::TCPromiseWithErrorWithAppAuditor
		(
			TCPromiseWithError<t_CReturnValue> const &_Promise
			, CDistributedAppAuditor const &_Auditor
		)
		: m_Promise(_Promise)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithErrorWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise.m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor, Error = m_Promise.m_Error]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(Auditor.f_Exception(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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
				fResultHandler();
			}
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithErrorWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise.m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor, Error = m_Promise.m_Error]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(Auditor.f_Exception(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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

#include "Malterlib_Concurrency_DistributedApp_Auditor_Coroutine.hpp"
