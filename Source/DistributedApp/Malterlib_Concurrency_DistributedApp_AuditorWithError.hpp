// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCPromise<tf_CReturnValue> const &_Promise, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCPromiseWithAppAuditorWithError<tf_CReturnValue>(_Promise, _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCPromiseWithError<tf_CReturnValue> const &_Promise, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCPromiseWithErrorWithAppAuditorWithError<tf_CReturnValue>(_Promise, _Auditor);
	}

	template <typename t_CReturnValue>
	TCPromiseWithAppAuditorWithError<t_CReturnValue>::TCPromiseWithAppAuditorWithError
		(
			TCPromise<t_CReturnValue> const &_Promise
			, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Promise(_Promise)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.m_Auditor.f_Error(Auditor.f_InternalError(p_Results.f_GetExceptionStr()));
								Promise.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
	auto TCPromiseWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.m_Auditor.f_Error(Auditor.f_InternalError(p_Results.f_GetExceptionStr()));
								Promise.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
	TCPromiseWithErrorWithAppAuditorWithError<t_CReturnValue>::TCPromiseWithErrorWithAppAuditorWithError
		(
			TCPromiseWithError<t_CReturnValue> const &_Promise
			, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Promise(_Promise)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithErrorWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
								Promise.f_SetException(DMibErrorInstance(Auditor.m_UserError));

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
	auto TCPromiseWithErrorWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
								Promise.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
