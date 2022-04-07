// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedAppLogReporter>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogReader_LogFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NStorage::TCOptional<NStr::CStr> m_HostID;
		NStorage::TCOptional<CDistributedAppLogReporter::CLogScope> m_Scope;
		NStorage::TCOptional<NStr::CStr> m_Identifier;
		NStorage::TCOptional<NStr::CStr> m_IdentifierScope;
	};

	struct CDistributedAppLogReader_LogDataFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		bool f_IsFiltered() const;

		NStorage::TCOptional<NContainer::TCSet<NStr::CStr>> m_CategoriesWildcards;
		NStorage::TCOptional<NContainer::TCSet<NStr::CStr>> m_OperationsWildcards;
		NStorage::TCOptional<NContainer::TCSet<NStr::CStr>> m_MessageWildcards;
		NStorage::TCOptional<NContainer::TCSet<NStr::CStr>> m_SourceLocationFileWildcards;
		NStorage::TCOptional<NContainer::TCSet<CDistributedAppLogReporter::ELogSeverity>> m_Severities;
		NStorage::TCOptional<CDistributedAppLogReporter::ELogEntryFlag> m_Flags;
	};

	struct CDistributedAppLogReader_LogEntryFilter
	{
		enum ELogEntriesFlag
		{
			ELogEntriesFlag_None = 0
			, ELogEntriesFlag_ReportNewestFirst = DMibBit(0)
		};

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppLogReader_LogFilter m_LogFilter;
		CDistributedAppLogReader_LogDataFilter m_LogDataFilter;

		NStorage::TCOptional<uint64> m_MinSequence;
		NStorage::TCOptional<uint64> m_MaxSequence;

		NStorage::TCOptional<NTime::CTime> m_MinTimestamp;
		NStorage::TCOptional<NTime::CTime> m_MaxTimestamp;

		ELogEntriesFlag m_Flags = ELogEntriesFlag_ReportNewestFirst;
	};

	struct CDistributedAppLogReader_LogKeyAndEntry
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppLogReporter::CLogInfoKey m_LogInfoKey;
		CDistributedAppLogReporter::CLogEntry m_Entry;
	};

	struct CDistributedAppLogReader : public CActor
	{
		CDistributedAppLogReader();
		~CDistributedAppLogReader();

		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppLogReader";

		enum : uint32
		{
			EProtocolVersion_Min = CDistributedAppLogReporter::EProtocolVersion_Min
			, EProtocolVersion_Current = CDistributedAppLogReporter::EProtocolVersion_Current
		};

		virtual TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>>> f_GetLogs(CDistributedAppLogReader_LogFilter &&_Filter, uint32 _BatchSize) = 0;
		virtual auto f_GetLogEntries(CDistributedAppLogReader_LogEntryFilter &&_Filter, uint32 _BatchSize)
			-> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>>> = 0
		;
	};
}

#include "Malterlib_Concurrency_DistributedApp_LogReader.hpp"
