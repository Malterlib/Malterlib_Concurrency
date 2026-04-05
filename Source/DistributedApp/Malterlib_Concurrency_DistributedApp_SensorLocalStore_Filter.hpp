// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib::NConcurrency::NSensorStore
{
	template <typename tf_CKey>
	bool fg_FilterSensorKey
		(
			tf_CKey const &_Key
			, CDistributedAppSensorReader_SensorFilter const &_Filter
			, CFilterSensorKeyContext &_Context
			, CDistributedAppSensorReporter::CSensorInfo const *_pSensorInfo
			, NSensorStoreLocalDatabase::CKnownHostValue const *_pKnownHost
		)
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
					if (NStr::fg_StrMatchWildcard(_Key.m_ApplicationName.f_GetStr(), Application.m_ApplicationName.f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						return false;
				}
				break;
			}
		}

		if (_Filter.m_Identifier && NStr::fg_StrMatchWildcard(_Key.m_Identifier.f_GetStr(), _Filter.m_Identifier->f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
			return false;

		if
			(
				_Filter.m_IdentifierScope
				&& NStr::fg_StrMatchWildcard(_Key.m_IdentifierScope.f_GetStr(), _Filter.m_IdentifierScope->f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted
			)
		{
			return false;
		}

		if (_Filter.m_Flags & CDistributedAppSensorReader_SensorFilter::ESensorFlag_IgnoreRemoved)
		{
			if (_pKnownHost)
			{
				if (_pKnownHost->m_bRemoved)
					return false;
			}
			else if (!_Key.m_HostID.f_IsEmpty() && _Key.m_HostID != _Context.m_ThisHostID && _Context.m_pTransaction)
			{
				NSensorStoreLocalDatabase::CKnownHostKey Key{.m_DbPrefix = _Context.m_Prefix, .m_HostID = _Key.m_HostID};
				NSensorStoreLocalDatabase::CKnownHostValue Value;
				if (_Context.m_pTransaction->f_Get(Key, Value))
				{
					if (Value.m_bRemoved)
						return false;
				}
			}

			if (!_pSensorInfo)
				_pSensorInfo = _Context.f_GetSensor(_Key.f_SensorKey());

			if (_pSensorInfo && _pSensorInfo->m_bRemoved)
				return false;
		}

		return true;
	}

	template <typename tf_CKey>
	bool fg_FilterSensorKey
		(
			tf_CKey const &_Key
			, NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> const &_Filters
			, CFilterSensorKeyContext &_Context
			, CDistributedAppSensorReporter::CSensorInfo const *_pSensorInfo
			, NSensorStoreLocalDatabase::CKnownHostValue const *_pKnownHost
		)
	{
		bool bPassFilter = _Filters.f_IsEmpty();
		for (auto &Filter : _Filters)
		{
			if (fg_FilterSensorKey(_Key, Filter, _Context, _pSensorInfo, _pKnownHost))
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
			, fp32 _PauseReportingFor
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
			, NTime::CTime const &_Now
		)
	{
		if (!(_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems))
			return true;

		auto fCheckSnoozed = [&]()
			{
				if (_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_IgnoreSnoozed)
				{
					if (_Sensor.m_Info.m_SnoozeUntil.f_IsValid() && _Now < _Sensor.m_Info.m_SnoozeUntil)
						return false;
				}

				return true;
			}
		;

		if (fg_FilterSensorValueStatusIsProblem(_Reading))
			return fCheckSnoozed();

		if (_Sensor.m_Info.f_IsValueCritical(_Reading.m_Data))
			return fCheckSnoozed();
		else if (_Sensor.m_Info.f_IsValueWarning(_Reading.m_Data))
			return fCheckSnoozed();

		if (_Sensor.m_Info.f_HasExpectedReportInterval() && _PauseReportingFor != fp32::fs_Inf())
		{
			fp64 OutdatedSeconds = (_Now - _Reading.m_Timestamp).f_GetSecondsFraction() - _Sensor.m_Info.m_ExpectedReportInterval;
			if (!_PauseReportingFor.f_IsNan())
				OutdatedSeconds -= _PauseReportingFor;

			if (OutdatedSeconds > _Sensor.m_Info.m_ExpectedReportInterval)
				return fCheckSnoozed();
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
