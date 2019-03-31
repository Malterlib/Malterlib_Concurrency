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

	CDistributedAppAuditor::CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo)
		: mp_AppActor(_AppActor)
		, mp_CallingHostInfo(_CallingHostInfo)
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

	CStr CDistributedAppAuditor::f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
 		mp_AppActor(&CDistributedAppActor::f_Audit, _Severity, _Message, _Category, mp_CallingHostInfo) > fg_DiscardResult();
		return _Message;
	}
	
	CStr CDistributedAppAuditor::f_Audit(NLog::ESeverity _Severity, NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		CStr FullMessage;
		CStr FirstMessage;
		for (CStr const &Message : _Message)
		{
			if (FirstMessage.f_IsEmpty())
				FirstMessage = Message;
			fg_AddStrSep(FullMessage, Message, " ");
		}
 		mp_AppActor(&CDistributedAppActor::f_Audit, _Severity, FullMessage, _Category, mp_CallingHostInfo) > fg_DiscardResult();
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

	CDistributedAppAuditor CDistributedAppActor::f_Auditor(CCallingHostInfo const &_CallingHostInfo) const
	{
		return {fg_ThisActor(this), _CallingHostInfo};
	}
	
	CDistributedAppAuditor CDistributedAppState::f_Auditor(CCallingHostInfo const &_CallingHostInfo) const
	{
		return {m_AppActor, _CallingHostInfo};
	}

	NFunction::TCFunctionMovable<CDistributedAppAuditor (CCallingHostInfo const &_CallingHostInfo)> CDistributedAppState::f_AuditorFactory() const
	{
		return [AppActor = m_AppActor](CCallingHostInfo const &_CallingHostInfo) mutable -> CDistributedAppAuditor
			{
				return {AppActor, _CallingHostInfo};
			}
		;
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		if (_Message.f_IsEmpty())
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, "Access denied", _Category));
		else
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, {"Access denied", _Message}, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}

	namespace NUnwrap
	{
		CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer const &_Helper, CDistributedAppAuditor const &_Auditor)
		{
			return CUnwrapHelperWithTransformer
				(
					[_Auditor, fPreviousTransformer = _Helper.m_fTransformer](NException::CExceptionPointer &&_pException)
					{
						auto pException = fPreviousTransformer(fg_Move(_pException));
						_Auditor.f_Error(NException::fg_ExceptionString(pException));
						return fg_Move(_pException);
					}
				)
			;
		}

		CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer const &_Helper, CDistributedAppAuditorWithError const &_Auditor)
		{
			return CUnwrapHelperWithTransformer
				(
					[_Auditor, fPreviousTransformer = _Helper.m_fTransformer](NException::CExceptionPointer &&_pException)
					{
						auto pException = fPreviousTransformer(fg_Move(_pException));
						_Auditor.m_Auditor.f_Error(_Auditor.f_InternalError(NException::fg_ExceptionString(pException)));
						return NException::fg_ExceptionPointer(DMibErrorInstance(_Auditor.m_UserError));
					}
				)
			;
		}
	}
}
