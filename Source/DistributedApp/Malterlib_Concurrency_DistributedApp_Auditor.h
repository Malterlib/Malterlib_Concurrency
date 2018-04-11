// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	struct CDistributedAppActor;
	struct CDistributedAppAuditorWithError;

	struct CDistributedAppAuditor
	{
		CDistributedAppAuditor();
		~CDistributedAppAuditor();
		CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo);
		NStr::CStr f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Audit(NLog::ESeverity _Severity, NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Critical(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Critical(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NStr::CStr const &_Message = {}, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		
		NStr::CStr f_Warning(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Warning(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		
		CCallingHostInfo const &f_HostInfo() const;

		CDistributedAppAuditorWithError operator ()(NStr::CStr const &_UserError, NStr::CStr const &_Category = {});
		CDistributedAppAuditorWithError operator ()(NContainer::TCVector<NStr::CStr> const &_Errors, NStr::CStr const &_Category = {});

	private:
		CCallingHostInfo mp_CallingHostInfo;
		TCWeakActor<CDistributedAppActor> mp_AppActor;
	};

	struct CDistributedAppAuditorWithError
	{
		CDistributedAppAuditorWithError(CDistributedAppAuditor const &_Auditor, NStr::CStr const &_UserError, NStr::CStr const &_InternalError, NStr::CStr const &_Category);

		NStr::CStr f_InternalError(NStr::CStr const &_Error) const;

		CDistributedAppAuditor m_Auditor;
		NStr::CStr m_UserError;
		NStr::CStr m_InternalError;
		NStr::CStr m_Category;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithAppAuditor
	{
		TCContinuationWithAppAuditor(TCContinuation<t_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuation<t_CReturnValue> m_Continuation;
		CDistributedAppAuditor m_Auditor;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithErrorWithAppAuditor
	{
		TCContinuationWithErrorWithAppAuditor(TCContinuationWithError<t_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuationWithError<t_CReturnValue> m_Continuation;
		CDistributedAppAuditor m_Auditor;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithAppAuditorWithError
	{
		TCContinuationWithAppAuditorWithError(TCContinuation<t_CReturnValue> const &_Continuation, CDistributedAppAuditorWithError const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuation<t_CReturnValue> m_Continuation;
		CDistributedAppAuditorWithError m_Auditor;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithErrorWithAppAuditorWithError
	{
		TCContinuationWithErrorWithAppAuditorWithError(TCContinuationWithError<t_CReturnValue> const &_Continuation, CDistributedAppAuditorWithError const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuationWithError<t_CReturnValue> m_Continuation;
		CDistributedAppAuditorWithError m_Auditor;
	};
}
