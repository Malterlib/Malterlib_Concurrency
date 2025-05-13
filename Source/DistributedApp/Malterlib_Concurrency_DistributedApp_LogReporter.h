// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Encoding/EJson>
#include <Mib/Storage/Variant>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogReporter : public CActor
	{
		CDistributedAppLogReporter();
		~CDistributedAppLogReporter();

		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppLogReporter";

		enum : uint32
		{
			EProtocolVersion_Min = 0x101

			, EProtocolVersion_UniqueSequenceAtLastCleanup = 0x102
			, EProtocolVersion_ReportEntriesDepth = 0x103
			, EProtocolVersion_MultipleFilters = 0x104
			, EProtocolVersion_IgnoreRemoved = 0x105
			, EProtocolVersion_LastSeenEntrySentinel = 0x106
			, EProtocolVersion_InfoMetadata = 0x107

			, EProtocolVersion_Current = 0x107
		};

		enum ELogSeverity : uint32
		{
			ELogSeverity_Unsupported = 0
			, ELogSeverity_Critical
			, ELogSeverity_Error
			, ELogSeverity_Warning
			, ELogSeverity_Info
			, ELogSeverity_Debug
			, ELogSeverity_DebugVerbose1
			, ELogSeverity_DebugVerbose2
			, ELogSeverity_DebugVerbose3
		};

		enum ELogEntryFlag : uint32
		{
			ELogEntryFlag_None = 0
			, ELogEntryFlag_Audit = DMibBit(0)
			, ELogEntryFlag_Performance = DMibBit(1)
		};

		enum ELogScope : uint32
		{
			ELogScope_Unsupported
			, ELogScope_Host
			, ELogScope_Application
		};

		struct CSourceLocation
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSourceLocation const &_Right) const = default;

			NStr::CStr m_File;
			int32 m_Line = 0;
		};

		struct CLogScope_Unsupported
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CLogScope_Unsupported const &_Right) const = default;
		};

		struct CLogScope_Host
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CLogScope_Host const &_Right) const = default;
		};

		struct CLogScope_Application
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CLogScope_Application const &_Right) const = default;

			NStr::CStr m_ApplicationName; // Supports wildcards
		};

		using CLogScope = NStorage::TCStreamableVariant
			<
				ELogScope
				, NStorage::TCMember<CLogScope_Host, ELogScope_Host>
				, NStorage::TCMember<CLogScope_Unsupported, ELogScope_Unsupported>
				, NStorage::TCMember<CLogScope_Application, ELogScope_Application>
			>
		;

		struct CLogData
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			void f_SetFromLogSeverity(NLog::ESeverity _Severity);
			NEncoding::CEJsonSorted f_ToJson() const;

			auto operator <=> (CLogData const &_Right) const = default;

			NContainer::TCVector<NStr::CStr> m_Categories;
			NContainer::TCVector<NStr::CStr> m_Operations;
			NStr::CStr m_Message;
			NStorage::TCOptional<CSourceLocation> m_SourceLocation;
			NEncoding::CEJsonSorted m_Metadata;
			ELogSeverity m_Severity = ELogSeverity_Info;
			ELogEntryFlag m_Flags = ELogEntryFlag_None;
		};

		struct CLogEntry
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			uint64 m_UniqueSequence = TCLimitsInt<uint64>::mc_Max; /// This will be filled in automatically when using CDistributedAppLogStoreLocal
			NTime::CTime m_Timestamp = NTime::CTime::fs_NowUTC();
			CLogData m_Data;
		};

		struct CLogInfoAutomatic
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			auto operator <=> (CLogInfoAutomatic const &_Right) const = default;

			/// This will be filled in automatically when using CDistributedAppLogStoreLocal
			NStr::CStr m_HostID;
			NStr::CStr m_HostName;
			CLogScope m_Scope;
		};

		struct CLogInfoKey
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CLogInfoKey const &_Right) const = default;

			NStr::CStr m_HostID;
			CLogScope m_Scope;
			NStr::CStr m_Identifier;
			NStr::CStr m_IdentifierScope;
		};

		struct CLogInfo : public CLogInfoAutomatic
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			CLogInfoKey f_Key();

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CLogInfo const &_Right) const = default;

			NStr::CStr m_Identifier;
			NStr::CStr m_IdentifierScope;

			NStr::CStr m_Name;

			NTime::CTime m_LastSeen = NTime::CTime::fs_NowUTC();

			NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> m_Metadata;

			bool m_bRemoved = false;
		};

		struct CReportEntriesResult
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			uint32 m_ReportDepth = 0;
		};

		struct CLogReporter
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			TCActorFunctorWithID<TCFuture<CReportEntriesResult> (NContainer::TCVector<CLogEntry> _Entries)> m_fReportEntries;
			uint64 m_LastSeenUniqueSequence = 0;
		};

		virtual TCFuture<CLogReporter> f_OpenLogReporter(CLogInfo _LogInfo) = 0;

		static NStr::CStr fs_LogSeverityToStr(ELogSeverity _Severity);
		static ELogSeverity fs_LogSeverityFromStr(NStr::CStr const &_String);
	};
}

#include "Malterlib_Concurrency_DistributedApp_LogReporter.hpp"
