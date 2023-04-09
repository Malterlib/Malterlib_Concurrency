// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency::NLogStoreLocalDatabase
{
	constexpr NStr::CStr const CLogGlobalStateKey::mc_Prefix = NStr::gc_Str<"LogGlobal">;
	constexpr NStr::CStr CLogKey::mc_Prefix = NStr::gc_Str<"Log">;
	constexpr NStr::CStr CLogEntryKey::mc_Prefix = NStr::gc_Str<"LogEntry">;
	constexpr NStr::CStr CLogEntryByTime::mc_Prefix = NStr::gc_Str<"LogEntryByDate">;
	constexpr NStr::CStr const CKnownHostKey::mc_Prefix = NStr::gc_Str<"LogKnownHost">;

	CLogEntryKey CLogEntryByTime::f_Key() const
	{
		CLogEntryKey Key;
		Key.m_DbPrefix = m_DbPrefix;
		Key.m_Prefix = CLogEntryKey::mc_Prefix;

		Key.m_UniqueSequence = m_UniqueSequence;

		Key.m_HostID = m_HostID;
		Key.m_ApplicationName = m_ApplicationName;
		Key.m_Identifier = m_Identifier;
		Key.m_IdentifierScope = m_IdentifierScope;

		return Key;
	}

	CDistributedAppLogReporter::CLogInfoKey CLogKey::f_LogInfoKey() const &
	{
		return
			{
				m_HostID
				, m_ApplicationName
				? CDistributedAppLogReporter::CLogScope{CDistributedAppLogReporter::CLogScope_Application{m_ApplicationName}}
				: CDistributedAppLogReporter::CLogScope_Host{}
				, m_Identifier
				, m_IdentifierScope
			}
		;
	}

	CDistributedAppLogReporter::CLogInfoKey CLogKey::f_LogInfoKey() &&
	{
		return
			{
				fg_Move(m_HostID)
				, m_ApplicationName
				? CDistributedAppLogReporter::CLogScope{CDistributedAppLogReporter::CLogScope_Application{fg_Move(m_ApplicationName)}}
				: CDistributedAppLogReporter::CLogScope_Host{}
				, fg_Move(m_Identifier)
				, fg_Move(m_IdentifierScope)
			}
		;
	}

	CDistributedAppLogReporter::CLogEntry CLogEntryValue::f_Entry(CLogEntryKey const &_Key) const &
	{
		return
			{
				_Key.m_UniqueSequence
				, m_Timestamp
				, m_Data
			}
		;
	}

	CDistributedAppLogReporter::CLogEntry CLogEntryValue::f_Entry(CLogEntryKey const &_Key) &&
	{
		return
			{
				_Key.m_UniqueSequence
				, m_Timestamp
				, fg_Move(m_Data)
			}
		;
	}
}
