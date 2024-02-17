// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Auditor.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NNetwork;
	using namespace NCryptography;

	CDistributedAppAuditorWithError::CDistributedAppAuditorWithError
		(
			CDistributedAppAuditor const &_Auditor
			, NStr::CStr const &_UserError
			, NStr::CStr const &_InternalError
			, NStr::CStr const &_Category
		)
		: m_Auditor(_Auditor)
		, m_UserError(_UserError)
		, m_InternalError(_InternalError)
		, m_Category(_Category)
	{
	}

	NStr::CStr CDistributedAppAuditorWithError::f_InternalError(NStr::CStr const &_Error) const
	{
		if (m_InternalError.f_IsEmpty())
			return _Error;
		return NStr::CStr::CFormat("{}: {}") << m_InternalError << _Error;
	}

	CDistributedAppAuditor::CDistributedAppAuditor() = default;
	CDistributedAppAuditor::~CDistributedAppAuditor() = default;

	CDistributedAppAuditor::CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo, NStr::CStr const &_Category)
		: mp_AppActor(_AppActor)
		, mp_CallingHostInfo(_CallingHostInfo)
		, mp_Category(_Category)
	{
	}

	CDistributedAppAuditorWithError CDistributedAppAuditor::operator ()(NStr::CStr const &_UserError, NStr::CStr const &_Category)
	{
		return CDistributedAppAuditorWithError{*this, _UserError, {}, _Category};
	}

	CDistributedAppAuditorWithError CDistributedAppAuditor::operator ()(TCVector<NStr::CStr> const &_Errors, NStr::CStr const &_Category)
	{
		CStr FullMessage;
		CStr FirstMessage;
		for (CStr const &Message : _Errors)
		{
			if (FirstMessage.f_IsEmpty())
				FirstMessage = Message;
			fg_AddStrSep(FullMessage, Message, " ");
		}
		return CDistributedAppAuditorWithError{*this, FirstMessage, FullMessage, _Category};
	}

	CStr CDistributedAppAuditor::f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CDistributedAppAuditExtraData &&_ExtraData) const
	{
		CDistributedAppAuditParams AuditParams
			{
				.m_Severity = _Severity
				, .m_Message = _Message
				, .m_Category = _Category ? _Category : mp_Category
				, .m_CallingHostInfo = mp_CallingHostInfo
				, .m_ExtraData = fg_Move(_ExtraData)
			}
		;

		if (!mp_AppActor)
		{
			CDistributedAppActor::fs_LogAudit(AuditParams, mp_Category);
			return _Message;
		}

		mp_AppActor(&CDistributedAppActor::f_Audit, fg_Move(AuditParams)) > fg_DiscardResult();

		return _Message;
	}

	CStr CDistributedAppAuditor::f_Audit
		(
			NLog::ESeverity _Severity
			, NContainer::TCVector<NStr::CStr> const &_Message
			, NStr::CStr const &_Category
			, CDistributedAppAuditExtraData &&_ExtraData
		) const
	{
		CStr FullMessage;
		CStr FirstMessage;
		for (CStr const &Message : _Message)
		{
			if (FirstMessage.f_IsEmpty())
				FirstMessage = Message;
			fg_AddStrSep(FullMessage, Message, " ");
		}

		CDistributedAppAuditParams AuditParams
			{
				.m_Severity = _Severity
				, .m_Message = FullMessage
				, .m_Category = _Category ? _Category : mp_Category
				, .m_CallingHostInfo = mp_CallingHostInfo
				, .m_ExtraData = fg_Move(_ExtraData)
			}
		;

		if (!mp_AppActor)
		{
			CDistributedAppActor::fs_LogAudit(AuditParams, mp_Category);
			return FirstMessage;
		}

		mp_AppActor(&CDistributedAppActor::f_Audit, fg_Move(AuditParams)) > fg_DiscardResult();

		return FirstMessage;
	}

	CStr CDistributedAppAuditor::f_Critical(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Critical, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Critical(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Critical, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Error(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Error, _Message, _Category);
	}

	CStr CDistributedAppAuditor::f_Error(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Error, _Message, _Category);
	}
	
	NException::CException CDistributedAppAuditor::f_Exception(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_Exception(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_CriticalException(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Critical, _Message, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_CriticalException(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Critical, _Message, _Category));
	}
	
	CStr CDistributedAppAuditor::f_Warning(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Warning, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Warning(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Warning, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Info(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Info, _Message, _Category);
	}

	CStr CDistributedAppAuditor::f_Info(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Info, _Message, _Category);
	}
	
	CCallingHostInfo const &CDistributedAppAuditor::f_HostInfo() const
	{
		return mp_CallingHostInfo;
	}

	CDistributedAppAuditor CDistributedAppActor::f_Auditor(NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo) const
	{
		return {fg_ThisActor(this), _CallingHostInfo, _Category};
	}
	
	CDistributedAppAuditor CDistributedAppState::f_Auditor(NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo) const
	{
		return {m_AppActor, _CallingHostInfo, _Category};
	}

	auto CDistributedAppState::f_AuditorFactory(NStr::CStr const &_Category) const
		-> NFunction::TCFunctionMovable<CDistributedAppAuditor (CCallingHostInfo const &_CallingHostInfo, NStr::CStr const &_Category)>
	{
		return [AppActor = m_AppActor, Category = _Category](CCallingHostInfo const &_CallingHostInfo, NStr::CStr const &_Category) mutable -> CDistributedAppAuditor
			{
				return {AppActor, _CallingHostInfo, _Category ? _Category : Category};
			}
		;
	}

	namespace
	{
		NContainer::TCSet<NContainer::TCSet<NStr::CStr>> fg_ConvertPermissions(NContainer::TCVector<NStr::CStr> const &_Permissions)
		{
			NContainer::TCSet<NContainer::TCSet<NStr::CStr>> Permissions;
			NContainer::TCSet<NStr::CStr> Set;
			Set.f_AddContainer(_Permissions);
			Permissions[Set];

			return Permissions;
		}

		NContainer::TCSet<NContainer::TCSet<NStr::CStr>> fg_ConvertPermissions(NContainer::TCVector<CPermissionQuery> const &_Permissions)
		{
			NContainer::TCSet<NContainer::TCSet<NStr::CStr>> Permissions;
			for (auto &Query : _Permissions)
			{
				NContainer::TCSet<NStr::CStr> Set;
				Set.f_AddContainer(Query.m_Permissions);
				Permissions[Set];
			}

			return Permissions;
		}
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied(NStr::CStr const &_Message, NContainer::TCVector<NStr::CStr> const &_Permissions, NStr::CStr const &_Category) const
	{
		auto Permissions = fg_ConvertPermissions(_Permissions);

		if (_Message.f_IsEmpty())
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, "Access denied", _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
		else
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, {"Access denied", _Message}, _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied
		(
			NContainer::TCVector<NStr::CStr> const &_Message
			, NContainer::TCVector<NStr::CStr> const &_Permissions
			, NStr::CStr const &_Category
		) const
	{
		auto Permissions = fg_ConvertPermissions(_Permissions);

		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
	}

	NException::CException CDistributedAppAuditor::f_AccessDenied
		(
			NStr::CStr const &_Message
			, NContainer::TCVector<CPermissionQuery> const &_Permissions
			, NStr::CStr const &_Category
		) const
	{
		auto Permissions = fg_ConvertPermissions(_Permissions);

		if (_Message.f_IsEmpty())
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, "Access denied", _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
		else
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, {"Access denied", _Message}, _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
	}

	NException::CException CDistributedAppAuditor::f_AccessDenied
		(
			NContainer::TCVector<NStr::CStr> const &_Message
			, NContainer::TCVector<CPermissionQuery> const &_Permissions
			, NStr::CStr const &_Category
		) const
	{
		auto Permissions = fg_ConvertPermissions(_Permissions);

		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category, CDistributedAppAuditExtraData_AccessDenied{.m_Permissions = Permissions}));
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CDistributedAppAuditor const &_Auditor
		)
	{
		return [_Auditor, fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
			{
				auto pException = fPreviousTransformer(fg_Move(_pException));
				if (!pException)
					return nullptr;

				_Auditor.f_Error(NException::fg_ExceptionString(pException));
				return fg_Move(pException);
			}
		;
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, CDistributedAppAuditorWithError const &_Auditor
		)
	{
		return [_Auditor, fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
			{
				auto pException = fPreviousTransformer(fg_Move(_pException));
				if (!pException)
					return nullptr;
				
				_Auditor.m_Auditor.f_Error(_Auditor.f_InternalError(NException::fg_ExceptionString(pException)));
				return NException::fg_MakeException(DMibErrorInstance(_Auditor.m_UserError));
			}
		;
	}
}
