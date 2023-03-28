// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorReporter.h"

namespace NMib::NConcurrency
{
	CDistributedAppSensorReporter::CDistributedAppSensorReporter()
	{
		DMibPublishActorFunction(CDistributedAppSensorReporter::f_OpenSensorReporter);
	}

	CDistributedAppSensorReporter::~CDistributedAppSensorReporter() = default;

	auto CDistributedAppSensorReporter::CSensorInfo::f_Key() -> CSensorInfoKey
	{
		return {m_HostID, m_Scope, m_Identifier, m_IdentifierScope};
	}

	bool CDistributedAppSensorReporter::CValueComparison::f_IsTrue(CSensorData const &_Value) const
	{
		switch (m_Operator)
		{
		case EValueComparisonOperator_Equal:
			return _Value == m_CompareToValue;
		case EValueComparisonOperator_LessThan:
			{
				if (_Value.f_GetTypeID() != m_CompareToValue.f_GetTypeID())
				{
					if (m_CompareToValue.f_IsOfType<fp64>())
					{
						if (_Value.f_IsOfType<int64>())
							return fp64(_Value.f_GetAsType<int64>()) < m_CompareToValue.f_GetAsType<fp64>();
					}
					else if (m_CompareToValue.f_IsOfType<int64>())
					{
						if (_Value.f_IsOfType<fp64>())
							return _Value.f_GetAsType<fp64>() < fp64(m_CompareToValue.f_GetAsType<int64>());
					}

					return false;
				}

				return _Value < m_CompareToValue;
			}
			break;
		case EValueComparisonOperator_GreaterThan:
			{
				if (_Value.f_GetTypeID() != m_CompareToValue.f_GetTypeID())
				{
					if (m_CompareToValue.f_IsOfType<fp64>())
					{
						if (_Value.f_IsOfType<int64>())
							return fp64(_Value.f_GetAsType<int64>()) > m_CompareToValue.f_GetAsType<fp64>();
					}
					else if (m_CompareToValue.f_IsOfType<int64>())
					{
						if (_Value.f_IsOfType<fp64>())
							return _Value.f_GetAsType<fp64>() > fp64(m_CompareToValue.f_GetAsType<int64>());
					}

					return false;
				}

				return _Value > m_CompareToValue;
			}
			break;
		}

		return false;
	}

	bool CDistributedAppSensorReporter::CSensorInfo::f_IsValueWarning(CSensorData const &_Value) const
	{
		if (!m_WarnValue)
			return false;

		return m_WarnValue->f_IsTrue(_Value);
	}

	bool CDistributedAppSensorReporter::CSensorInfo::f_IsValueCritical(CSensorData const &_Value) const
	{
		if (!m_CriticalValue)
			return false;

		return m_CriticalValue->f_IsTrue(_Value);
	}

	bool CDistributedAppSensorReporter::CSensorInfo::f_HasValueWarnings() const
	{
		return m_WarnValue || m_CriticalValue;
	}

	bool CDistributedAppSensorReporter::CSensorInfo::f_HasExpectedReportInterval() const
	{
		return m_ExpectedReportInterval != fp32::fs_Inf();
	}

	auto CDistributedAppSensorReporter::CSensorReading::f_Status() const -> EStatusSeverity
	{
		EStatusSeverity Status = EStatusSeverity_Info;
		if (m_Data.f_IsOfType<CDistributedAppSensorReporter::CStatus>())
			Status = m_Data.f_GetAsType<CDistributedAppSensorReporter::CStatus>().m_Severity;
		return Status;
	}

	auto CDistributedAppSensorReporter::CSensorReading::f_CriticalityStatus(CSensorInfo const &_SensorInfo) const -> EStatusSeverity
	{
		if (_SensorInfo.f_IsValueCritical(m_Data))
			return EStatusSeverity_Error;
		else if (_SensorInfo.f_IsValueWarning(m_Data))
			return EStatusSeverity_Warning;
		else if (_SensorInfo.f_HasValueWarnings())
			return EStatusSeverity_Ok;

		return EStatusSeverity_Info;
	}

	auto CDistributedAppSensorReporter::CSensorReading::f_OutdatedStatus(CSensorInfo const &_SensorInfo, NTime::CTime const &_Now, fp64 &o_OutdatedSeconds) const -> EStatusSeverity
	{
		if (!_SensorInfo.f_HasExpectedReportInterval())
			return EStatusSeverity_Ok;

		o_OutdatedSeconds = (_Now - m_Timestamp).f_GetSecondsFraction() - _SensorInfo.m_ExpectedReportInterval;

		if (o_OutdatedSeconds > _SensorInfo.m_ExpectedReportInterval * 2)
			return EStatusSeverity_Error;

		if (o_OutdatedSeconds > _SensorInfo.m_ExpectedReportInterval)
			return EStatusSeverity_Warning;

		return EStatusSeverity_Ok;
	}

	auto CDistributedAppSensorReporter::CSensorReading::f_GetCombinedStatus(CSensorInfo const *_pSensorInfo, NTime::CTime const &_Now) const -> EStatusSeverity
	{
		EStatusSeverity Status = f_Status();

		if (_pSensorInfo)
		{
			fp64 OutdatedSeconds;
			Status = fs_CombineStatusSeverity(Status, f_OutdatedStatus(*_pSensorInfo, _Now, OutdatedSeconds));
			Status = fs_CombineStatusSeverity(Status, f_CriticalityStatus(*_pSensorInfo));
		}

		return Status;
	}

	auto CDistributedAppSensorReporter::fs_CombineStatusSeverity(EStatusSeverity _Severity0, EStatusSeverity _Severity1) -> EStatusSeverity
	{
		if (_Severity0 >= EStatusSeverity_Warning)
		{
			if (_Severity1 >= EStatusSeverity_Warning)
				return fg_Max(_Severity0, _Severity1);
			else
				return _Severity0;
		} else if (_Severity1 >= EStatusSeverity_Warning)
			return _Severity1;

		if (_Severity0 == EStatusSeverity_Ok || _Severity1 == EStatusSeverity_Ok)
			return EStatusSeverity_Ok;

		return EStatusSeverity_Info;
	}

	NStr::CStr CDistributedAppSensorReporter::fs_StatusSeverityToColor(EStatusSeverity _Severity, NCommandLine::CAnsiEncoding const &_AnsiEncoding)
	{
		switch (_Severity)
		{
		case CDistributedAppSensorReporter::EStatusSeverity_Ok: return _AnsiEncoding.f_StatusNormal();
		case CDistributedAppSensorReporter::EStatusSeverity_Info: break;
		case CDistributedAppSensorReporter::EStatusSeverity_Warning: return _AnsiEncoding.f_StatusWarning();
		case CDistributedAppSensorReporter::EStatusSeverity_Error: return _AnsiEncoding.f_StatusError();
		}

		return {};
	}

	NStr::CStr CDistributedAppSensorReporter::CSensorReading::f_GetFormattedData(NCommandLine::CAnsiEncoding const &_AnsiEncoding, CSensorInfo const *_pSensorInfo) const
	{
		using namespace NStr;

		EStatusSeverity Severity = EStatusSeverity_Info;

		if (_pSensorInfo)
			Severity = f_CriticalityStatus(*_pSensorInfo);

		CStr Value;
		m_Data.f_Visit
			(
				[&](auto const &_Value)
				{
					using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
					if constexpr (NTraits::TCIsSame<CValueType, CVoidTag>::mc_Value)
						Value = "Unsupported";
					else if constexpr (NTraits::TCIsSame<CValueType, bool>::mc_Value)
						Value = _Value ? "true" : "false";
					else if constexpr (NTraits::TCIsSame<CValueType, CDistributedAppSensorReporter::CStatus>::mc_Value)
					{
						Value = _Value.m_Description;
						Severity = fs_CombineStatusSeverity(Severity, _Value.m_Severity);
					}
					else
					{
						if (_pSensorInfo)
						{
							auto *pDivisorUnit = _pSensorInfo->m_UnitDivisors.f_FindLargestLessThanEqual(CDistributedAppSensorReporter::CSensorData(fg_Abs(_Value)));
							if (!pDivisorUnit)
								pDivisorUnit = _pSensorInfo->m_UnitDivisors.f_FindLargestLessThanEqual(CDistributedAppSensorReporter::CSensorData(fp64(fg_Abs(_Value))));

							if (pDivisorUnit)
							{
								auto &Divisor = _pSensorInfo->m_UnitDivisors.fs_GetKey(pDivisorUnit);
								if (Divisor.template f_IsOfType<CValueType>())
								{
									if (Divisor.template f_GetAsType<CValueType>())
										Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << (_Value / Divisor.template f_GetAsType<CValueType>());
									else
										Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << _Value;
									return;
								}
								else if (Divisor.template f_IsOfType<fp64>())
								{
									// Allow floating point division for integer data

									if (Divisor.template f_GetAsType<fp64>())
										Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << (fp64(_Value) / Divisor.template f_GetAsType<fp64>());
									else
										Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << fp64(_Value);
									return;
								}
							}
						}
						Value = "{}"_f << _Value;
					}
				}
			)
		;

		auto ValueColor = fs_StatusSeverityToColor(Severity, _AnsiEncoding);
		if (ValueColor)
			Value = ("{}{}{}"_f << ValueColor << Value << _AnsiEncoding.f_Default()).f_GetStr();

		return Value;
	}

	NStr::CStr CDistributedAppSensorReporter::CSensorReading::f_GetFormattedOutdatedStatus
		(
			NCommandLine::CAnsiEncoding const &_AnsiEncoding
			, CSensorInfo const *_pSensorInfo
			, NTime::CTime const &_Now
			, EOutdatedStatusOutputFlag _Flags
		) const
	{
		using namespace NStr;

		fp64 OutdatedSeconds = fp64::fs_Inf();
		CStr OutdatedString;

		if (!_pSensorInfo)
			OutdatedString = "Unknown";
		else if (_pSensorInfo->f_HasExpectedReportInterval())
		{
			auto OutdatedStatus = f_OutdatedStatus(*_pSensorInfo, _Now, OutdatedSeconds);

			if (_Flags & EOutdatedStatusOutputFlag_IncludeStatus)
			{
				OutdatedString = CDistributedAppSensorReporter::fs_StatusSeverityToColor(OutdatedStatus, _AnsiEncoding);
				OutdatedString += CDistributedAppSensorReporter::fs_StatusSeverityToString(OutdatedStatus);
				OutdatedString += ": ";
				OutdatedString += _AnsiEncoding.f_Default();
			}

			if (OutdatedStatus >= CDistributedAppSensorReporter::EStatusSeverity_Warning)
			{
				auto MissedUpdates = (OutdatedSeconds / _pSensorInfo->m_ExpectedReportInterval).f_ToInt();
				if (MissedUpdates > 10)
					OutdatedString += "Sensor missed more than 10 updates"_f << MissedUpdates; // Make the message constant so notification system won't get overloaded
				else
					OutdatedString += "Sensor missed {} updates"_f << MissedUpdates;
			}
		}

		return OutdatedString;
	}

	ch8 const *CDistributedAppSensorReporter::fs_SensorDataTypeToString(ESensorDataType _Type)
	{
		switch (_Type)
		{
		case ESensorDataType_Unsupported: return "Unsupported";
		case ESensorDataType_Integer: return "Integer";
		case ESensorDataType_Float: return "Floating Point";
		case ESensorDataType_Boolean: return "Boolean";
		case ESensorDataType_Status: return "Status";
		}
		return "Unknown";
	}

	ch8 const *CDistributedAppSensorReporter::fs_StatusSeverityToString(EStatusSeverity _Severity)
	{
		switch (_Severity)
		{
		case EStatusSeverity_Ok: return "Ok";
		case EStatusSeverity_Info: return "Info";
		case EStatusSeverity_Warning: return "Warning";
		case EStatusSeverity_Error: return "Error";
		}
		return "Unknown";
	}

	NStr::CStr CDistributedAppSensorReporter::fs_FormatSensorDivisors(NContainer::TCMap<CSensorData, CUnit> const &_Divisors, ch8 const *_pSeparator)
	{
		using namespace NStr;

		CStr UnitDivisors;
		for (auto &Divisor : _Divisors)
		{
			CStr KeyStr = _Divisors.fs_GetKey(Divisor).f_VisitRet<CStr>
				(
					[](auto const &_Value) -> NStr::CStr
					{
						using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
						if constexpr (NTraits::TCIsSame<CValueType, CVoidTag>::mc_Value)
							return "Unsupported";
						else
							return "{}"_f << _Value;
					}
				)
			;
			fg_AddStrSep
				(
					UnitDivisors
					, "{}: Decimals: {} Formatter: {}"_f
					<< KeyStr
					<< Divisor.m_nDecimals
					<< Divisor.m_UnitFormatter
					, _pSeparator
				)
			;
		}
		return UnitDivisors;
	}

	auto CDistributedAppSensorReporter::fs_DiskSpaceDivisors() -> NContainer::TCMap<CSensorData, CUnit>
	{
		NContainer::TCMap<CSensorData, CUnit> Return;

		{
			auto &UnitDivisor = Return[NConcurrency::CDistributedAppSensorReporter::CSensorData(fp64(0.0))];
			UnitDivisor.m_nDecimals = 1;
			UnitDivisor.m_UnitFormatter = "{fn0} B";
		}
		{
			auto &UnitDivisor = Return[NConcurrency::CDistributedAppSensorReporter::CSensorData(fp64(1024.0))];
			UnitDivisor.m_nDecimals = 1;
			UnitDivisor.m_UnitFormatter = "{fn1} KiB";
		}
		{
			auto &UnitDivisor = Return[NConcurrency::CDistributedAppSensorReporter::CSensorData(fp64(1024.0*1024.0))];
			UnitDivisor.m_nDecimals = 1;
			UnitDivisor.m_UnitFormatter = "{fn1} MiB";
		}
		{
			auto &UnitDivisor = Return[NConcurrency::CDistributedAppSensorReporter::CSensorData(fp64(1024.0*1024.0*1024.0))];
			UnitDivisor.m_nDecimals = 1;
			UnitDivisor.m_UnitFormatter = "{fn1} GiB";
		}
		{
			auto &UnitDivisor = Return[NConcurrency::CDistributedAppSensorReporter::CSensorData(fp64(1024.0*1024.0*1024.0*1024.0))];
			UnitDivisor.m_nDecimals = 1;
			UnitDivisor.m_UnitFormatter = "{fn1} TiB";
		}

		return Return;
	}
}
