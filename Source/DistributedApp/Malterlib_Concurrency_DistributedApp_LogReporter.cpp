// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogReporter.h"

namespace NMib::NConcurrency
{
	CDistributedAppLogReporter::CDistributedAppLogReporter()
	{
		DMibPublishActorFunction(CDistributedAppLogReporter::f_OpenLogReporter);
	}

	CDistributedAppLogReporter::~CDistributedAppLogReporter() = default;

	auto CDistributedAppLogReporter::CLogInfo::f_Key() -> CLogInfoKey
	{
		return {m_HostID, m_Scope, m_Identifier, m_IdentifierScope};
	}

	void CDistributedAppLogReporter::CLogData::f_SetFromLogSeverity(NLog::ESeverity _Severity)
	{
		using namespace NLog;

		m_Severity = CDistributedAppLogReporter::ELogSeverity_Info;

		switch (_Severity)
		{
		case ESeverity_Critical: m_Severity = CDistributedAppLogReporter::ELogSeverity_Critical; break;
		case ESeverity_Error: m_Severity = CDistributedAppLogReporter::ELogSeverity_Error; break;
		case ESeverity_Warning: m_Severity = CDistributedAppLogReporter::ELogSeverity_Warning; break;
		case ESeverity_Info: m_Severity = CDistributedAppLogReporter::ELogSeverity_Info; break;
		case ESeverity_Debug: m_Severity = CDistributedAppLogReporter::ELogSeverity_Debug; break;
		case ESeverity_DebugVerbose1: m_Severity = CDistributedAppLogReporter::ELogSeverity_DebugVerbose1; break;
		case ESeverity_DebugVerbose2: m_Severity = CDistributedAppLogReporter::ELogSeverity_DebugVerbose2; break;
		case ESeverity_Perf_Info:
			{
				m_Severity = CDistributedAppLogReporter::ELogSeverity_Info;
				m_Flags |= CDistributedAppLogReporter::ELogEntryFlag_Performance;
			}
			break;
		case ESeverity_Perf_Warning:
			{
				m_Severity = CDistributedAppLogReporter::ELogSeverity_Warning;
				m_Flags |= CDistributedAppLogReporter::ELogEntryFlag_Performance;
			}
			break;
		case ESeverity_Perf_Error:
			{
				m_Severity = CDistributedAppLogReporter::ELogSeverity_Error;
				m_Flags |= CDistributedAppLogReporter::ELogEntryFlag_Performance;
			}
			break;
		case ESeverity_None:
		case ESeverity_All:
			break;
		}
	}
}
