// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib::NConcurrency::NSensorStore
{
	template <typename tf_CKey>
	bool fg_FilterSensorKey(tf_CKey const &_Key, CDistributedAppSensorReader_SensorFilter const &_Filter)
	{
		if (_Filter.m_HostID && _Key.m_HostID != *_Filter.m_HostID)
			return false;

		if (_Filter.m_Scope)
		{
			switch (_Filter.m_Scope->f_GetTypeID())
			{
			case CDistributedAppSensorReporter::ESensorScope_Unsupported:
				{
					return false;
				}
				break;
			case CDistributedAppSensorReporter::ESensorScope_Host:
				{
					if (!_Key.m_ApplicationName.f_IsEmpty())
						return false;
				}
				break;
			case CDistributedAppSensorReporter::ESensorScope_Application:
				{
					auto &Application = _Filter.m_Scope->f_Get<CDistributedAppSensorReporter::ESensorScope_Application>();
					if (_Key.m_ApplicationName != Application.m_ApplicationName)
						return false;
				}
				break;
			}
		}

		if (_Filter.m_Identifier && _Key.m_Identifier != *_Filter.m_Identifier)
			return false;

		if (_Filter.m_IdentifierScope && _Key.m_IdentifierScope != *_Filter.m_IdentifierScope)
			return false;

		return true;
	}

	template <typename tf_CKey>
	bool fg_FilterSensorKey
		(
			tf_CKey const &_Key
			, NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> const &_Filters
		)
	{
		bool bPassFilter = _Filters.f_IsEmpty();
		for (auto &Filter : _Filters)
		{
			if (fg_FilterSensorKey(_Key, Filter))
			{
				bPassFilter = true;
				break;
			}
		}
		return bPassFilter;
	}

	template <typename tf_CReading>
	bool fg_FilterSensorValueStatusIsProblem(tf_CReading const &_Reading)
	{
		if (_Reading.m_Data.template f_IsOfType<CDistributedAppSensorReporter::CStatus>())
		{
			auto &Status = _Reading.m_Data.template f_GetAsType<CDistributedAppSensorReporter::CStatus>();
			if (Status.m_Severity >= CDistributedAppSensorReporter::EStatusSeverity_Warning)
				return true;
		}

		return false;
	}

	template <typename tf_CSensor, typename tf_CReading>
	bool fg_FilterSensorValueStatus
		(
			tf_CSensor const &_Sensor
			, tf_CReading const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
			, NTime::CTime const &_Now
		)
	{
		if (!(_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems))
			return true;

		if (fg_FilterSensorValueStatusIsProblem(_Reading))
			return true;

		if (_Sensor.m_Info.f_IsValueCritical(_Reading.m_Data))
			return true;
		else if (_Sensor.m_Info.f_IsValueWarning(_Reading.m_Data))
			return true;

		if (_Sensor.m_Info.f_HasExpectedReportInterval())
		{
			fp64 OutdatedSeconds = (_Now - _Reading.m_Timestamp).f_GetSecondsFraction() - _Sensor.m_Info.m_ExpectedReportInterval;

			if (OutdatedSeconds > _Sensor.m_Info.m_ExpectedReportInterval)
				return true;
		}

		return false;
	}

	template <typename tf_CSensor, typename tf_CReading>
	bool fg_FilterSensorReadingValueWithSensor
		(
			tf_CSensor const &_Sensor
			, tf_CReading const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
		)
	{
		if (!(_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems))
			return true;

		if (fg_FilterSensorValueStatusIsProblem(_Reading))
			return true;

		if (_Sensor.m_Info.f_IsValueCritical(_Reading.m_Data))
			return true;
		else if (_Sensor.m_Info.f_IsValueWarning(_Reading.m_Data))
			return true;

		return false;
	}
}
