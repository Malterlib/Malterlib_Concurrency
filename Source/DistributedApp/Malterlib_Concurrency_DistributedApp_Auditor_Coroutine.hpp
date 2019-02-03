// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../Actor/Malterlib_Concurrency_Continuation_Coroutine.hpp"

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditor<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditor<t_CReturnValue>::operator co_await() const
	{
		return TCContinuationAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Continuation
			 	, [Auditor = m_Auditor](CExceptionPointer &&_pException)
			 	{
					Auditor.f_Error(NException::fg_ExceptionString(_pException));
					return fg_Move(_pException);
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditor<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	*m_pWrapped
			 	, [Auditor = m_pWrapped->m_Auditor](CExceptionPointer &&_pException)
			 	{
					Auditor.f_Error(NException::fg_ExceptionString(_pException));
					return fg_Move(_pException);
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/* 	TCContinuationWithError<t_CReturnValue> m_Continuation;
	 	CDistributedAppAuditor m_Auditor;
	*/

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::operator co_await() const
	{
		return TCContinuationAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Continuation.m_Continuation
			 	, [Auditor = m_Auditor, Error = this->m_Continuation.m_Error](CExceptionPointer &&_pException)
			 	{
					return Auditor.f_Exception(fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))).f_ExceptionPointer();
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditor<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_pWrapped->m_Continuation.m_Continuation
			 	, [Auditor = m_pWrapped->m_Auditor, Error = m_pWrapped->m_Continuation.m_Error](CExceptionPointer &&_pException)
			 	{
					return Auditor.f_Exception(fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))).f_ExceptionPointer();
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/*  TCContinuation<t_CReturnValue> m_Continuation;
		CDistributedAppAuditorWithError m_Auditor;
	 */

	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditorWithError<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditorWithError<t_CReturnValue>::operator co_await() const
	{
		return TCContinuationAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Continuation
			 	, [Auditor = m_Auditor](CExceptionPointer &&_pException)
			 	{
					Auditor.m_Auditor.f_Error(Auditor.f_InternalError(NException::fg_ExceptionString(_pException)));
					return NException::fg_ExceptionPointer(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithAppAuditorWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	*m_pWrapped
			 	, [Auditor = m_pWrapped->m_Auditor](CExceptionPointer &&_pException)
			 	{
					Auditor.m_Auditor.f_Error(Auditor.f_InternalError(NException::fg_ExceptionString(_pException)));
					return NException::fg_ExceptionPointer(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	////////////////////////////////////////////////////////////////
	/*	TCContinuationWithError<t_CReturnValue> m_Continuation;
		CDistributedAppAuditorWithError m_Auditor;
	 */

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::operator co_await() const
	{
		return TCContinuationAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Continuation.m_Continuation
			 	, [Auditor = m_Auditor, Error = m_Continuation.m_Error](CExceptionPointer &&_pException)
			 	{
					Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))));
					return NException::fg_ExceptionPointer(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithErrorWithAppAuditorWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_pWrapped->m_Continuation.m_Continuation
			 	, [Auditor = m_pWrapped->m_Auditor, Error = m_pWrapped->m_Continuation.m_Error](CExceptionPointer &&_pException)
			 	{
					Auditor.m_Auditor.f_Exception(Auditor.f_InternalError(NStr::fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException))));
					return NException::fg_ExceptionPointer(DMibErrorInstance(Auditor.m_UserError));
				}
			)
		;
	}
}
