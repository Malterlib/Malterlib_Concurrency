// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorScope_Unsupported::f_Stream(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorScope_Host::f_Stream(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorScope_Application::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_ApplicationName;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CStatus::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Severity;
		_Stream % m_Description;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CStatus::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}: {}") << fs_StatusSeverityToString(m_Severity) << m_Description;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CVersion::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Identifier;
		_Stream % m_Major;
		_Stream % m_Minor;
		_Stream % m_Revision;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CVersion::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{} {}.{}.{}") << m_Identifier << m_Major << m_Minor << m_Revision;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorData::f_Stream(tf_CStream &_Stream)
	{
		using namespace NStr;

		if (_Stream.f_GetVersion() >= EProtocolVersion_VersionData)
			_Stream % (CSensorDataVariant &)(*this);
		else
		{
			if constexpr (tf_CStream::mc_bConsume)
				_Stream >> (CSensorDataVariant &)(*this);
			else
			{
				if (this->f_IsOfType<CVersion>())
				{
					CSensorDataVariant Temp = CStatus
						{
							.m_Severity = EStatusSeverity_Error
							, .m_Description = "Version sensor reading is not supported. Version: {}"_f << this->f_GetAsType<CVersion>()
						}
					;
					_Stream << Temp;
				}
				else
					_Stream << (CSensorDataVariant &)(*this);
			}
		}
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorReading::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UniqueSequence;
		_Stream % m_Timestamp;
		_Stream % m_Data;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CUnit::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UnitFormatter;
		_Stream % m_nDecimals;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CValueComparison::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_CompareToValue;
		_Stream % m_Operator;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CValueComparison::f_Format(tf_CStr &o_Str) const
	{
		ch8 const *pOperator = "?";
		switch (m_Operator)
		{
			case EValueComparisonOperator_LessThan: pOperator = "<"; break;
			case EValueComparisonOperator_GreaterThan: pOperator = ">"; break;
			case EValueComparisonOperator_Equal: pOperator = "=="; break;
		}

		if (m_CompareToValue.f_IsOfType<void>())
			o_Str += typename tf_CStr::CFormat("{} Unsupported") << pOperator;
		else
			o_Str += typename tf_CStr::CFormat("{} {}") << pOperator << m_CompareToValue;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorInfoAutomatic::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_HostName;
		_Stream % m_Scope;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorInfo::f_Stream(tf_CStream &_Stream)
	{
		CSensorInfoAutomatic::f_Stream(_Stream);

		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
		_Stream % m_Name;
		_Stream % m_Type;
		_Stream % m_UnitDivisors;
		_Stream % m_ExpectedReportInterval;
		_Stream % m_WarnValue;
		_Stream % m_CriticalValue;

		if (_Stream.f_GetVersion() >= EProtocolVersion_IgnoreRemoved)
		{
			_Stream % m_LastSeen;
			_Stream % m_bRemoved;
		}
		else if constexpr (tf_CStream::mc_bConsume)
			m_LastSeen = NTime::CTime();

		if (_Stream.f_GetVersion() >= EProtocolVersion_PauseReporting)
			_Stream % m_PauseReportingFor;

		if (_Stream.f_GetVersion() >= EProtocolVersion_SnoozeSensor)
			_Stream % m_SnoozeUntil;

		if (_Stream.f_GetVersion() >= EProtocolVersion_InfoMetadata)
			_Stream % m_Metadata;

		if (_Stream.f_GetVersion() >= EProtocolVersion_SensorInfoFlags)
			_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorReporter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_fReportReadings);
		_Stream % m_LastSeenUniqueSequence;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReporter::CSensorInfoKey::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_Scope;
		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CSensorInfo::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat
			(
				"    HostID: {}\n"
				"    HostName: {}\n"
				"    Scope: {}\n"
				"    Identifier: {}\n"
				"    IdentifierScope: {}\n"
				"    Name: {}\n"
				"    Type: {}\n"
				"    UnitDivisors: {}\n"
				"    ExpectedReportInterval: {}\n"
				"    Flags: {vs}\n"
				"    WarnValue: {}\n"
				"    CriticalValue: {}\n"
				"    LastSeen: {}\n"
				"    PauseReportingFor: {}\n"
				"    Removed: {}"
			)
			<< m_HostID
			<< m_HostName
			<< m_Scope
			<< m_Identifier
			<< m_IdentifierScope
			<< m_Name
			<< CDistributedAppSensorReporter::fs_SensorDataTypeToString(m_Type)
			<< CDistributedAppSensorReporter::fs_FormatSensorDivisors(m_UnitDivisors, ", ")
			<< m_ExpectedReportInterval
			<< CDistributedAppSensorReporter::fs_FlagsToStringArray(m_Flags)
			<< m_WarnValue
			<< m_CriticalValue
			<< m_LastSeen
			<< m_PauseReportingFor
			<< (m_bRemoved ? "true" : "false")
		;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CSensorInfoKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("HostID: {} Scope: {} Identifier: {} IdentifierScope: {}") << m_HostID << m_Scope << m_Identifier << m_IdentifierScope;
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CSensorScope_Unsupported::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "Unsupported";
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CSensorScope_Host::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "Host";
	}

	template <typename tf_CStr>
	void CDistributedAppSensorReporter::CSensorScope_Application::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Application: {}") << m_ApplicationName;
	}
}
