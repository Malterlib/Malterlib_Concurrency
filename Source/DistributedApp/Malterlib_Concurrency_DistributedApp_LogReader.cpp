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
