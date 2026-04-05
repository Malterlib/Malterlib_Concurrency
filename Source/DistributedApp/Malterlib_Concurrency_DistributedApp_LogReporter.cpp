// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		case ESeverity_DebugVerbose3: m_Severity = CDistributedAppLogReporter::ELogSeverity_DebugVerbose3; break;
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

	CDistributedAppLogReporter::ELogSeverity CDistributedAppLogReporter::fs_LogSeverityFromStr(NStr::CStr const &_String)
	{
		if (_String == "Critical")
			return ELogSeverity_Critical;
		else if (_String == "Error")
			return ELogSeverity_Error;
		else if (_String == "Warning")
			return ELogSeverity_Warning;
		else if (_String == "Info")
			return ELogSeverity_Info;
		else if (_String == "Debug")
			return ELogSeverity_Debug;
		else if (_String == "DebugVerbose1")
			return ELogSeverity_DebugVerbose1;
		else if (_String == "DebugVerbose2")
			return ELogSeverity_DebugVerbose2;
		else if (_String == "DebugVerbose3")
			return ELogSeverity_DebugVerbose3;

		return ELogSeverity_Unsupported;
	}

	NStr::CStr CDistributedAppLogReporter::fs_LogSeverityToStr(ELogSeverity _Severity)
	{
		switch (_Severity)
		{
		case ELogSeverity_Critical: return "Critical";
		case ELogSeverity_Error: return "Error";
		case ELogSeverity_Warning: return "Warning";
		case ELogSeverity_Info: return "Info";
		case ELogSeverity_Debug: return "Debug";
		case ELogSeverity_DebugVerbose1: return "DebugVerbose1";
		case ELogSeverity_DebugVerbose2: return "DebugVerbose2";
		case ELogSeverity_DebugVerbose3: return "DebugVerbose3";
		case ELogSeverity_Unsupported: break;
		}

		return "Unsupported";
	}

	NEncoding::CEJsonSorted CDistributedAppLogReporter::CLogData::f_ToJson() const
	{
		NEncoding::CEJsonSorted Return;

		Return["Message"] = m_Message;
		Return["Categories"] = m_Categories;
		Return["Operations"] = m_Operations;
		if (m_SourceLocation)
		{
			auto &SourceLocation = *m_SourceLocation;
			auto &Location = Return["SourceLocation"];
			Location["File"] = SourceLocation.m_File;
			Location["Line"] = SourceLocation.m_Line;
		}
		if (m_Metadata.f_IsValid())
			Return["Metadata"] = m_Metadata;

		Return["Severity"] = fs_LogSeverityToStr(m_Severity);

		auto &Flags = Return["Flags"].f_Array();

		if (m_Flags & ELogEntryFlag_Performance)
			Flags.f_Insert("Performance");
		if (m_Flags & ELogEntryFlag_Audit)
			Flags.f_Insert("Audit");

		return Return;
	}
}
