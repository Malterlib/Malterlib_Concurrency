// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCContinuation<tf_CReturnValue> const &_Continuation, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCContinuationWithAppAuditorWithError<tf_CReturnValue>(_Continuation, _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCContinuationWithError<tf_CReturnValue> const &_Continuation, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCContinuationWithErrorWithAppAuditorWithError<tf_CReturnValue>(_Continuation, _Auditor);
	}

	template <typename t_CReturnValue>
	TCContinuationWithAppAuditorWithError<t_CReturnValue>::TCContinuationWithAppAuditorWithError
		(
		 	TCContinuation<t_CReturnValue> const &_Continuation
		 	, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Continuation(_Continuation)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCContinuationWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Continuation = m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor]
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
								Continuation.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
	auto TCContinuationWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Continuation = m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor]
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
								Continuation.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
	TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::TCContinuationWithErrorWithAppAuditorWithError
		(
			TCContinuationWithError<t_CReturnValue> const &_Continuation
			, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Continuation(_Continuation)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Continuation = m_Continuation.m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor, Error = m_Continuation.m_Error]
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
								Continuation.f_SetException(DMibErrorInstance(Auditor.m_UserError));

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
	auto TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Continuation = m_Continuation.m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Auditor = m_Auditor, Error = m_Continuation.m_Error]
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
								Continuation.f_SetException(DMibErrorInstance(Auditor.m_UserError));
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
