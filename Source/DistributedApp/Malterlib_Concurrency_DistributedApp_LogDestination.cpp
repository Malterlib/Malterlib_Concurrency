// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogDestination.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NLog;

	struct CDistributedAppLogDestination_Internal
	{
		struct CDeferredMessage
		{
			NTime::CTime m_Time;
			NLog::ESeverity m_Severity;
			NLog::CLogStr m_Message;
			NContainer::TCVector<NStr::CStr> m_Categories;
			NContainer::TCVector<NStr::CStr> m_Operations;
			NLog::CLogLocationTag m_Location;
		};

		CDistributedAppLogDestination_Internal(TCActor<CActor> const &_LogActor)
			: m_LogActor(_LogActor ? _LogActor : TCActor<CActor>(fg_ConcurrentActor()))
		{
		}

		~CDistributedAppLogDestination_Internal()
		{
			if (m_LogReporter && m_LogReporter->m_fReportEntries)
				fg_Move(m_LogReporter->m_fReportEntries).f_Destroy() > fg_DiscardResult();
		}

		void f_ProcessDeferredMessages()
		{
			DMibFastCheck(m_LogReporter);
			NContainer::TCVector<CDeferredMessage> Messages;
			{
				DMibLock(m_Lock);
				m_bScheduledProcess = false;
				Messages = fg_Move(m_DeferredMessages);
			}
			if (!Messages.f_GetLen())
				return;

			if (!m_LogReporter->m_fReportEntries)
				return;

			NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> LogEntries;

			for (auto &Message : Messages)
			{
				auto &LogEntry = LogEntries.f_Insert();
				LogEntry.m_Timestamp = Message.m_Time;

				auto &Data = LogEntry.m_Data;
				Data.f_SetFromLogSeverity(Message.m_Severity);
				Data.m_Categories = fg_Move(Message.m_Categories);
				Data.m_Operations = fg_Move(Message.m_Operations);
				Data.m_Message = fg_Move(Message.m_Message);
				if (Message.m_Location.m_pFile)
					Data.m_SourceLocation = {Message.m_Location.m_pFile, Message.m_Location.m_Line};
			}

			m_LogReporter->m_fReportEntries(fg_Move(LogEntries)) > NPrivate::fg_DirectResultActor() / [](TCAsyncResult<CDistributedAppLogReporter::CReportEntriesResult> &&_Result)
				{
					if (!_Result.f_HasExceptionType<CExceptionActorDeleted>() && !_Result)
						DMibConErrOut2("Error reporting log entries: {}\n", _Result.f_GetExceptionStr());
				}
			;
		}

		TCActor<CActor> m_LogActor;
		NStorage::TCOptional<CDistributedAppLogReporter::CLogReporter> m_LogReporter;
		NThread::CMutual m_Lock;
		NContainer::TCVector<CDeferredMessage> m_DeferredMessages;
		bool m_bScheduledProcess = false;
	};

	CDistributedAppLogDestination::CDistributedAppLogDestination(TCActor<CDistributedAppActor> const &_DistributedLoggingApp, TCActor<CActor> const &_LogActor)
		: mp_pInternal(fg_Construct(_LogActor))
	{
		fp_InitLogging(_DistributedLoggingApp);
	}

	CDistributedAppLogDestination::CDistributedAppLogDestination(CDistributedAppLogDestination &&_Other) = default;
	CDistributedAppLogDestination::CDistributedAppLogDestination(CDistributedAppLogDestination const &_Other) = default;

	CDistributedAppLogDestination::~CDistributedAppLogDestination() = default;

	void CDistributedAppLogDestination::fp_InitLogging(TCActor<CDistributedAppActor> const &_DistributedLoggingApp)
	{
		_DistributedLoggingApp(&CDistributedAppActor::f_OpenDefaultLogReporter)
			> mp_pInternal->m_LogActor / [pInternal = mp_pInternal](TCAsyncResult<CDistributedAppLogReporter::CLogReporter> &&_LogReporter)
			{
				if (!_LogReporter)
				{
					DMibConErrOut2("Failed to open log reporter in distributed app: {}\n", _LogReporter.f_GetExceptionStr());
					return;
				}

				pInternal->m_LogReporter = fg_Move(*_LogReporter);
				pInternal->f_ProcessDeferredMessages();
			}
		;
	}

	void CDistributedAppLogDestination::operator()
		(
			mint _ThreadID
			, NTime::CTime const &_Time
			, NLog::ESeverity _Severity
			, NLog::CLogStr const &_Message
			, NContainer::TCVector<NStr::CStr> const &_Categories
			, NContainer::TCVector<NStr::CStr> const &_Operations
			, NLog::CLogLocationTag const &_Location
		)
	{
		for (auto &Operation : _Operations)
		{
			if (Operation == "DisableDistributedLogReporter")
				return;
		}

		CDistributedAppLogDestination_Internal::CDeferredMessage Message{_Time, _Severity, _Message, _Categories, _Operations, _Location};
		
		auto &Internal = *mp_pInternal;
		{
			DMibLock(Internal.m_Lock);
			Internal.m_DeferredMessages.f_Insert(fg_Move(Message));

			if (Internal.m_LogReporter && !Internal.m_bScheduledProcess)
			{
				Internal.m_bScheduledProcess = true;
				fg_Timeout(0.01)(Internal.m_LogActor) > [pInternal = mp_pInternal]
					{
						pInternal->f_ProcessDeferredMessages();
					}
				;
			}
		}
	}
}
