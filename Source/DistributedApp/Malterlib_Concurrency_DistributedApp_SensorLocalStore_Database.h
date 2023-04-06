// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Database/DatabaseValue>

#pragma once
namespace NMib::NConcurrency::NSensorStoreLocalDatabase
{
	static constexpr uint32 gc_Version = CDistributedAppSensorReporter::EProtocolVersion_Current;

	struct CSensorGlobalStateKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		auto operator <=>(CSensorGlobalStateKey const &_Right) const = default;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix = mc_Prefix;
	};

	struct CSensorGlobalStateValue
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

		auto operator <=>(CKnownHostKey const &_Right) const = default;

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

	struct CSensorKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		auto operator <=>(CSensorKey const &_Right) const = default;

		CDistributedAppSensorReporter::CSensorInfoKey f_SensorInfoKey() const &;
		CDistributedAppSensorReporter::CSensorInfoKey f_SensorInfoKey() &&;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix;
		NStr::CStr m_HostID;
		NStr::CStr m_ApplicationName;
		NStr::CStr m_Identifier;
		NStr::CStr m_IdentifierScope;
	};

	struct CSensorValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppSensorReporter::CSensorInfo m_Info;
		uint64 m_UniqueSequenceAtLastCleanup = 0;
	};

	struct CSensorReadingKey : public CSensorKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		CSensorKey f_SensorKey() const &;
		CSensorKey f_SensorKey() &&;

		static NStr::CStr const mc_Prefix;

		uint64 m_UniqueSequence = 0;
	};

	struct CSensorReadingByTime
	{
		CSensorReadingKey f_Key() const;

		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_DbPrefix;
		NStr::CStr m_Prefix;

		NTime::CTime m_Timestamp;
		uint64 m_UniqueSequence = 0;

		NStr::CStr m_HostID;
		NStr::CStr m_ApplicationName;
		NStr::CStr m_Identifier;
		NStr::CStr m_IdentifierScope;
	};

	struct CSensorReadingByTimeValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
	};

	struct CSensorReadingValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppSensorReporter::CSensorReading f_Reading(CSensorReadingKey const &_Key) const &;
		CDistributedAppSensorReporter::CSensorReading f_Reading(CSensorReadingKey const &_Key) &&;

		NTime::CTime m_Timestamp = NTime::CTime::fs_NowUTC();
		CDistributedAppSensorReporter::CSensorData m_Data;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.hpp"
