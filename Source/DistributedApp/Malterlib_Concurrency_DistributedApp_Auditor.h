// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	struct CDistributedAppActor;
	struct [[nodiscard]] CDistributedAppAuditorWithError;

	enum EDistributedAppAuditType
	{
		EDistributedAppAuditType_Unsupported
		, EDistributedAppAuditType_Normal
		, EDistributedAppAuditType_AccessDenied
	};

	struct CDistributedAppAuditExtraData_Normal
	{
	};

	struct CDistributedAppAuditExtraData_AccessDenied
	{
		NContainer::TCSet<NContainer::TCSet<NStr::CStr>> m_Permissions;
	};

	using CDistributedAppAuditExtraData = NStorage::TCStreamableVariant
		<
			EDistributedAppAuditType
			, NStorage::TCMember<void, EDistributedAppAuditType_Unsupported>
			, NStorage::TCMember<CDistributedAppAuditExtraData_Normal, EDistributedAppAuditType_Normal>
			, NStorage::TCMember<CDistributedAppAuditExtraData_AccessDenied, EDistributedAppAuditType_AccessDenied>
		>
	;

	struct CDistributedAppAuditParams
	{
		NLog::ESeverity m_Severity = NLog::ESeverity_Info;
		NStr::CStr m_Message;
		NStr::CStr m_Category;
		CCallingHostInfo m_CallingHostInfo;
		CDistributedAppAuditExtraData m_ExtraData;
	};

	struct CDistributedAppAuditor
	{
		CDistributedAppAuditor();
		~CDistributedAppAuditor();
		CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo, NStr::CStr const &_Category);
		NStr::CStr f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category = {}, CDistributedAppAuditExtraData &&_ExtraData = {}) const;
		NStr::CStr f_Audit
			(
				NLog::ESeverity _Severity
				, NContainer::TCVector<NStr::CStr> const &_Message
				, NStr::CStr const &_Category = {}
				, CDistributedAppAuditExtraData &&_ExtraData = {}
			) const
		;
		NStr::CStr f_Critical(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Critical(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_CriticalException(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_CriticalException(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NStr::CStr const &_Message, NContainer::TCVector<NStr::CStr> const &_Permissions, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NContainer::TCVector<NStr::CStr> const &_Message, NContainer::TCVector<NStr::CStr> const &_Permissions, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NStr::CStr const &_Message, NContainer::TCVector<CPermissionQuery> const &_Permissions, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied
			(
				NContainer::TCVector<NStr::CStr> const &_Message
				, NContainer::TCVector<CPermissionQuery> const &_Permissions
				, NStr::CStr const &_Category = {}
			) const
		;

		NStr::CStr f_Warning(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Warning(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		
		CCallingHostInfo const &f_HostInfo() const;

		CDistributedAppAuditorWithError operator ()(NStr::CStr const &_UserError, NStr::CStr const &_Category = {});
		CDistributedAppAuditorWithError operator ()(NContainer::TCVector<NStr::CStr> const &_Errors, NStr::CStr const &_Category = {});

	private:
		CCallingHostInfo mp_CallingHostInfo;
		NStr::CStr mp_Category;
		TCWeakActor<CDistributedAppActor> mp_AppActor;
	};

	struct [[nodiscard]] CDistributedAppAuditorWithError
	{
		CDistributedAppAuditorWithError(CDistributedAppAuditor const &_Auditor, NStr::CStr const &_UserError, NStr::CStr const &_InternalError, NStr::CStr const &_Category);

		NStr::CStr f_InternalError(NStr::CStr const &_Error) const;

		CDistributedAppAuditor m_Auditor;
		NStr::CStr m_UserError;
		NStr::CStr m_InternalError;
		NStr::CStr m_Category;
	};

	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, CDistributedAppAuditor const &_Auditor
		)
	;

	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, CDistributedAppAuditorWithError const &_Auditor
		)
	;
}
