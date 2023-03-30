// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../Actor/Malterlib_Concurrency_Promise_Coroutine.hpp"

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCFuture<tf_CReturnValue> &&_Future, CDistributedAppAuditor const &_Auditor)
	{
		return TCFutureWithAppAuditor<tf_CReturnValue>(fg_Move(_Future), _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCFutureWithError<tf_CReturnValue> &&_Future, CDistributedAppAuditor const &_Auditor)
	{
		return TCFutureWithErrorWithAppAuditor<tf_CReturnValue>(fg_Move(_Future), _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCFuture<tf_CReturnValue> &&_Future, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCFutureWithAppAuditorWithError<tf_CReturnValue>(fg_Move(_Future), _Auditor);
	}

	template <typename tf_CReturnValue>
	auto operator % (TCFutureWithError<tf_CReturnValue> &&_Future, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCFutureWithErrorWithAppAuditorWithError<tf_CReturnValue>(fg_Move(_Future), _Auditor);
	}

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool t_bDirectCall>
	auto operator % (TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList, t_bDirectCall> &&_Call, CDistributedAppAuditor const &_Auditor)
	{
		return fg_Move(_Call).f_Future() % _Auditor;
	}

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool t_bDirectCall>
	auto operator % (TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList, t_bDirectCall> &&_Call, CDistributedAppAuditorWithError const &_Auditor)
	{
		return fg_Move(_Call).f_Future() % _Auditor;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto operator % (TCActorCallWithError<t_CActorCall, t_CReturnType> &&_Call, CDistributedAppAuditor const &_Auditor)
	{
		return fg_Move(_Call).f_Future() % _Auditor;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto operator % (TCActorCallWithError<t_CActorCall, t_CReturnType> &&_Call, CDistributedAppAuditorWithError const &_Auditor)
	{
		return fg_Move(_Call).f_Future() % _Auditor;
	}

	template <typename t_CReturnValue>
	TCFutureWithAppAuditor<t_CReturnValue>::TCFutureWithAppAuditor(TCFuture<t_CReturnValue> &&_Future, CDistributedAppAuditor const &_Auditor)
		: m_Future(fg_Move(_Future))
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditor<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditor<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_Future)
				, [Auditor = m_Auditor](NException::CExceptionPointer &&_pException)
				{
					Auditor.f_Error(NException::fg_ExceptionString(_pException));
					return fg_Move(_pException);
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditor<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(*m_pWrapped)
				, [Auditor = m_pWrapped->m_Auditor](NException::CExceptionPointer &&_pException)
				{
					Auditor.f_Error(NException::fg_ExceptionString(_pException));
					return fg_Move(_pException);
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/*	TCFutureWithError<t_CReturnValue> m_Future;
		CDistributedAppAuditor m_Auditor;
	*/

	template <typename t_CReturnValue>
	TCFutureWithErrorWithAppAuditor<t_CReturnValue>::TCFutureWithErrorWithAppAuditor
		(
			TCFutureWithError<t_CReturnValue> &&_Future
			, CDistributedAppAuditor const &_Auditor
		)
		: m_Future(fg_Move(_Future))
		, m_Auditor(_Auditor)
	{
	}
	
	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditor<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditor<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_Future.m_Future)
				, [Auditor = m_Auditor, Error = this->m_Future.m_Error](NException::CExceptionPointer &&_pException)
				{
					return Auditor.f_Exception(fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))).f_ExceptionPointer();
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditor<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_pWrapped->m_Future.m_Future)
				, [Auditor = m_pWrapped->m_Auditor, Error = m_pWrapped->m_Future.m_Error](NException::CExceptionPointer &&_pException)
				{
					return Auditor.f_Exception(fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))).f_ExceptionPointer();
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/*  TCFuture<t_CReturnValue> m_Future;
		CDistributedAppAuditorWithError m_Auditor;
	 */

	template <typename t_CReturnValue>
	TCFutureWithAppAuditorWithError<t_CReturnValue>::TCFutureWithAppAuditorWithError
		(
			TCFuture<t_CReturnValue> &&_Future
			, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Future(fg_Move(_Future))
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditorWithError<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditorWithError<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_Future)
				, [Auditor = m_Auditor](NException::CExceptionPointer &&_pException)
				{
					Auditor.m_Auditor.f_Error(Auditor.f_InternalError(NException::fg_ExceptionString(_pException)));
					return NException::fg_MakeException(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithAppAuditorWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(*m_pWrapped)
				, [Auditor = m_pWrapped->m_Auditor](NException::CExceptionPointer &&_pException)
				{
					Auditor.m_Auditor.f_Error(Auditor.f_InternalError(NException::fg_ExceptionString(_pException)));
					return NException::fg_MakeException(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/*	TCFutureWithError<t_CReturnValue> m_Future;
		CDistributedAppAuditorWithError m_Auditor;
	 */

	template <typename t_CReturnValue>
	TCFutureWithErrorWithAppAuditorWithError<t_CReturnValue>::TCFutureWithErrorWithAppAuditorWithError
		(
			TCFutureWithError<t_CReturnValue> &&_Future
			, CDistributedAppAuditorWithError const &_Auditor
		)
		: m_Future(fg_Move(_Future))
		, m_Auditor(_Auditor)
	{
	}

	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditorWithError<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditorWithError<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_Future.m_Future)
				, [Auditor = m_Auditor, Error = m_Future.m_Error](NException::CExceptionPointer &&_pException)
				{
					Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))));
					return NException::fg_MakeException(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithErrorWithAppAuditorWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
				fg_Move(m_pWrapped->m_Future.m_Future)
				, [Auditor = m_pWrapped->m_Auditor, Error = m_pWrapped->m_Future.m_Error](NException::CExceptionPointer &&_pException)
				{
					Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))));
					return NException::fg_MakeException(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CDistributedAppAuditor const &_Auditor
		)
	;

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CDistributedAppAuditorWithError const &_Auditor
		)
	;
}
