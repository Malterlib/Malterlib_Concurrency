// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
		auto fSeverity = [&]
			{
				switch (m_Severity)
				{
					case EStatusSeverity_Ok: return "Ok";
					case EStatusSeverity_Info: return "Info";
					case EStatusSeverity_Warning: return "Warning";
					case EStatusSeverity_Error: return "Error";
				}
				return "Unknown";
			}
		;
		o_Str += typename tf_CStr::CFormat("{}: {}") << fSeverity() << m_Description;
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
				"    WarnValue: {}\n"
				"    CriticalValue: {}"
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
			<< m_WarnValue
			<< m_CriticalValue
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
