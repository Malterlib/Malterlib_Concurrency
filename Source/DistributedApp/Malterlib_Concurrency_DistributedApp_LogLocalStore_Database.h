// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Database/DatabaseValue>

#pragma once
namespace NMib::NConcurrency::NLogStoreLocalDatabase
{
	static constexpr uint32 gc_Version = CDistributedAppLogReporter::EProtocolVersion_Current;

	struct CLogGlobalStateKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		auto operator <=>(CLogGlobalStateKey const &_Right) const noexcept = default;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix = mc_Prefix;
	};

	struct CLogGlobalStateValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		bool m_bConvertedKnownHosts = false;
	};

	struct CKnownHostKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		auto operator <=>(CKnownHostKey const &_Right) const noexcept = default;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix = mc_Prefix;
		NStr::CStr m_HostID;
	};

	struct CKnownHostValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NTime::CTime m_LastSeen;
		bool m_bRemoved = false;
	};

	struct CLogKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		auto operator <=> (CLogKey const &_Right) const noexcept = default;

		CLogKey const &f_LogKey() const &;

		CDistributedAppLogReporter::CLogInfoKey f_LogInfoKey() const &;
		CDistributedAppLogReporter::CLogInfoKey f_LogInfoKey() &&;

		static const NStr::CStr mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix = mc_Prefix;
		NStr::CStr m_HostID;
		NStr::CStr m_ApplicationName;
		NStr::CStr m_Identifier;
		NStr::CStr m_IdentifierScope;
	};

	struct CLogValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppLogReporter::CLogInfo m_Info;
		uint64 m_UniqueSequenceAtLastCleanup = 0;
	};

	struct CLogEntryKey : public CLogKey
	{
		CLogEntryKey();

		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		static const NStr::CStr mc_Prefix;

		uint64 m_UniqueSequence = 0;
	};

	struct CLogEntryByTime
	{
		CLogEntryKey f_Key() const;
		CLogKey f_LogKey() const &;

		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		static const NStr::CStr mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix = mc_Prefix;

		NTime::CTime m_Timestamp;
		uint64 m_UniqueSequence = 0;

		NStr::CStr m_HostID;
		NStr::CStr m_ApplicationName;
		NStr::CStr m_Identifier;
		NStr::CStr m_IdentifierScope;
	};

	struct CLogEntryByTimeValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
	};

	struct CLogEntryValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppLogReporter::CLogEntry f_Entry(CLogEntryKey const &_Key) const &;
		CDistributedAppLogReporter::CLogEntry f_Entry(CLogEntryKey const &_Key) &&;

		NTime::CTime m_Timestamp = NTime::CTime::fs_NowUTC();
		CDistributedAppLogReporter::CLogData m_Data;
	};
}

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.hpp"
