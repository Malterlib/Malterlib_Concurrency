// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Storage/Variant>

namespace NMib::NConcurrency
{
	struct CDistributedAppSensorReporter : public CActor
	{
		CDistributedAppSensorReporter();
		~CDistributedAppSensorReporter();

		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppSensorReporter";

		enum : uint32
		{
			EProtocolVersion_Min = 0x101
			, EProtocolVersion_Current = 0x101
		};

		enum ESensorDataType
		{
			ESensorDataType_Unsupported
			, ESensorDataType_Integer
			, ESensorDataType_Float
			, ESensorDataType_Boolean
			, ESensorDataType_Status
		};

		enum ESensorScope
		{
			ESensorScope_Unsupported
			, ESensorScope_Host
			, ESensorScope_Application
		};

		enum EStatusSeverity
		{
			EStatusSeverity_Ok
			, EStatusSeverity_Info
			, EStatusSeverity_Warning
			, EStatusSeverity_Error
		};

		struct CStatus
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			auto operator <=> (CStatus const &_Right) const = default;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			EStatusSeverity m_Severity = EStatusSeverity_Info;
			NStr::CStr m_Description;
		};

		using CSensorData = NStorage::TCStreamableVariant
			<
				ESensorDataType
				, NStorage::TCMember<void, ESensorDataType_Unsupported>
				, NStorage::TCMember<int64, ESensorDataType_Integer>
				, NStorage::TCMember<fp64, ESensorDataType_Float>
				, NStorage::TCMember<bool, ESensorDataType_Boolean>
				, NStorage::TCMember<CStatus, ESensorDataType_Status>
			>
		;

		struct CSensorScope_Unsupported
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSensorScope_Unsupported const &_Right) const = default;
		};

		struct CSensorScope_Host
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSensorScope_Host const &_Right) const = default;
		};

		struct CSensorScope_Application
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSensorScope_Application const &_Right) const = default;

			NStr::CStr m_ApplicationName;
		};

		using CSensorScope = NStorage::TCStreamableVariant
			<
				ESensorScope
				, NStorage::TCMember<CSensorScope_Host, ESensorScope_Host>
				, NStorage::TCMember<CSensorScope_Unsupported, ESensorScope_Unsupported>
				, NStorage::TCMember<CSensorScope_Application, ESensorScope_Application>
			>
		;

		struct CSensorReading
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			uint64 m_UniqueSequence = TCLimitsInt<uint64>::mc_Max; /// This will be filled in automatically when using CDistributedAppSensorStoreLocal
			NTime::CTime m_Timestamp = NTime::CTime::fs_NowUTC();
			CSensorData m_Data;
		};

		struct CUnit
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			auto operator <=> (CUnit const &_Right) const = default;

			NStr::CStr m_UnitFormatter;
			uint32 m_nDecimals = 2;
		};

		enum EValueComparisonOperator
		{
			EValueComparisonOperator_LessThan
			, EValueComparisonOperator_GreaterThan
			, EValueComparisonOperator_Equal
		};

		struct CValueComparison
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CValueComparison const &_Right) const = default;

			bool f_IsTrue(CSensorData const &_Value) const;

			CSensorData m_CompareToValue;
			EValueComparisonOperator m_Operator = EValueComparisonOperator_LessThan;
		};

		struct CSensorInfoAutomatic
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			auto operator <=> (CSensorInfoAutomatic const &_Right) const = default;

			/// This will be filled in automatically when using CDistributedAppSensorStoreLocal
			NStr::CStr m_HostID;
			NStr::CStr m_HostName;
			CSensorScope m_Scope;
		};

		struct CSensorInfoKey
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSensorInfoKey const &_Right) const = default;

			NStr::CStr m_HostID;
			CSensorScope m_Scope;
			NStr::CStr m_Identifier;
			NStr::CStr m_IdentifierScope;
		};

		struct CSensorInfo : public CSensorInfoAutomatic
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			CSensorInfoKey f_Key();

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			auto operator <=> (CSensorInfo const &_Right) const = default;

			bool f_IsValueWarning(CSensorData const &_Value) const;
			bool f_IsValueCritical(CSensorData const &_Value) const;
			bool f_HasValueWarnings() const;

			NStr::CStr m_Identifier;
			NStr::CStr m_IdentifierScope;

			NStr::CStr m_Name;
			ESensorDataType m_Type = ESensorDataType_Integer;
			NContainer::TCMap<CSensorData, CUnit> m_UnitDivisors;
			fp32 m_ExpectedReportInterval = fp32::fs_Inf();
			NStorage::TCOptional<CValueComparison> m_WarnValue;
			NStorage::TCOptional<CValueComparison> m_CriticalValue;
		};

		struct CSensorReporter
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			TCActorFunctorWithID<TCFuture<void> (NContainer::TCVector<CSensorReading> &&_Readings)> m_fReportReadings;
			uint64 m_LastSeenUniqueSequence = 0;
		};

		static ch8 const *fs_SensorDataTypeToString(ESensorDataType _Type);
		static NStr::CStr fs_FormatSensorDivisors(NContainer::TCMap<CSensorData, CUnit> const &_Divisors, ch8 const *_pSeparator = "\n");
		static NContainer::TCMap<CSensorData, CUnit> fs_DiskSpaceDivisors();

		virtual TCFuture<CSensorReporter> f_OpenSensorReporter(CSensorInfo &&_SensorInfo) = 0;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorReporter.hpp"
