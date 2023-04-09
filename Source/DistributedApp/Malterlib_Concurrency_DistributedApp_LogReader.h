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
		enum ELogFlag
		{
			ELogFlag_None = 0
			, ELogFlag_IgnoreRemoved = DMibBit(0)
		};

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NStorage::TCOptional<NStr::CStr> m_HostID;
		NStorage::TCOptional<CDistributedAppLogReporter::CLogScope> m_Scope;
		NStorage::TCOptional<NStr::CStr> m_Identifier;
		NStorage::TCOptional<NStr::CStr> m_IdentifierScope;
		ELogFlag m_Flags = ELogFlag_None;
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

	struct CDistributedAppLogReader_LogEntrySubscriptionFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
		CDistributedAppLogReader_LogEntryFilter f_ToEntryFilter() const;

		CDistributedAppLogReader_LogFilter m_LogFilter;
		CDistributedAppLogReader_LogDataFilter m_LogDataFilter;

		NStorage::TCOptional<uint64> m_MinSequence; ///< If specified historic entries will also be reported, but will not be applied to not yet generated entries
		NStorage::TCOptional<NTime::CTime> m_MinTimestamp; ///< If specified historic entries will also be reported, but will not be applied to not yet generated entries
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

		enum ELogChange : uint32
		{
			ELogChange_AddedOrUpdated
			, ELogChange_Removed
		};

		using CLogChange = NStorage::TCStreamableVariant
			<
				ELogChange
				, NStorage::TCMember<CDistributedAppLogReporter::CLogInfo, ELogChange_AddedOrUpdated>
				, NStorage::TCMember<CDistributedAppLogReporter::CLogInfoKey, ELogChange_Removed>
			>
		;

		struct CGetLogs
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDistributedAppLogReader_LogFilter> m_Filters;
			uint32 m_BatchSize = 1024;
		};

		struct CGetLogEntries
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDistributedAppLogReader_LogEntryFilter> m_Filters;
			uint32 m_BatchSize = 1024;
		};

		template <typename tf_CStream, typename tf_CFilter>
		static void fs_StreamFilterVector(tf_CStream &_Stream, NContainer::TCVector<tf_CFilter> &o_Filters);

		virtual TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>>> f_GetLogs(CGetLogs &&_Params) = 0;
		virtual auto f_GetLogEntries(CGetLogEntries &&_Params) -> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>>> = 0;
		virtual auto f_SubscribeLogs(NContainer::TCVector<CDistributedAppLogReader_LogFilter> &&_Filters, TCActorFunctorWithID<TCFuture<void> (CLogChange &&_Change)> &&_fOnChange)
			-> TCFuture<TCActorSubscriptionWithID<>> = 0
		;
		virtual auto f_SubscribeLogEntries
			(
				NContainer::TCVector<CDistributedAppLogReader_LogEntrySubscriptionFilter> &&_Filters
				, TCActorFunctorWithID<TCFuture<void> (CDistributedAppLogReader_LogKeyAndEntry &&_Entry)> &&_fOnEntry
			)
			-> TCFuture<TCActorSubscriptionWithID<>> = 0
		;
	};
}

#include "Malterlib_Concurrency_DistributedApp_LogReader.hpp"
