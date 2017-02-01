// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJSONDatabase>
#include <Mib/Concurrency/ActorCallOnce>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLineClient.h"

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCContinuation<tf_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor)
	{
		return TCContinuationWithAppAuditor<tf_CReturnValue>(_Continuation, _Auditor);
	}
	
	template <typename t_CReturnValue>
	TCContinuationWithAppAuditor<t_CReturnValue>::TCContinuationWithAppAuditor(TCContinuation<t_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor)
		: m_Continuation(_Continuation)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCContinuationWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.f_Error(p_Results.f_GetExceptionStr());
								Continuation.f_SetException(fg_Move(p_Results));
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
	auto TCContinuationWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Auditor.f_Error(p_Results.f_GetExceptionStr());
								Continuation.f_SetException(fg_Move(p_Results));
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

	template <typename tf_CReturnValue>
	auto operator % (TCContinuationWithError<tf_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor)
	{
		return TCContinuationWithErrorWithAppAuditor<tf_CReturnValue>(_Continuation, _Auditor);
	}
	
	template <typename t_CReturnValue>
	TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::TCContinuationWithErrorWithAppAuditor
		(
			TCContinuationWithError<t_CReturnValue> const &_Continuation
			, CDistributedAppAuditor const &_Auditor
		)
		: m_Continuation(_Continuation)
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Continuation.f_SetException(Auditor.f_Exception(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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
	auto TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
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
								Continuation.f_SetException(Auditor.f_Exception(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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
