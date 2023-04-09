// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogReader.h"

namespace NMib::NConcurrency
{
	CDistributedAppLogReader::CDistributedAppLogReader()
	{
		DMibPublishActorFunction(CDistributedAppLogReader::f_GetLogs);
		DMibPublishActorFunction(CDistributedAppLogReader::f_GetLogEntries);
	}

	CDistributedAppLogReader::~CDistributedAppLogReader() = default;

	CDistributedAppLogReader_LogEntryFilter CDistributedAppLogReader_LogEntrySubscriptionFilter::f_ToEntryFilter() const
	{
		CDistributedAppLogReader_LogEntryFilter Return;

		Return.m_LogFilter = m_LogFilter;
		Return.m_LogDataFilter = m_LogDataFilter;
		Return.m_MinSequence = m_MinSequence;
		Return.m_MinTimestamp = m_MinTimestamp;
		Return.m_Flags = CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_None;

		return Return;
	}

	bool CDistributedAppLogReader_LogDataFilter::f_IsFiltered() const
	{
		return m_CategoriesWildcards
			|| m_OperationsWildcards
			|| m_MessageWildcards
			|| m_SourceLocationFileWildcards
			|| m_Severities
			|| m_Flags
		;
	}

}
