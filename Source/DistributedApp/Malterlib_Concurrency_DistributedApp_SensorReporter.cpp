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

	ch8 const *CDistributedAppSensorReporter::fs_SensorDataTypeToString(ESensorDataType _Type)
	{
		switch (_Type)
		{
		case CDistributedAppSensorReporter::ESensorDataType_Unsupported: return "Unsupported";
		case CDistributedAppSensorReporter::ESensorDataType_Integer: return "Integer";
		case CDistributedAppSensorReporter::ESensorDataType_Float: return "Floating Point";
		case CDistributedAppSensorReporter::ESensorDataType_Boolean: return "Boolean";
		case CDistributedAppSensorReporter::ESensorDataType_Status: return "Status";
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
