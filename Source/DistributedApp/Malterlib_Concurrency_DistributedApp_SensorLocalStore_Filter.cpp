// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Filter.h"

namespace NMib::NConcurrency::NSensorStore
{
	using namespace NSensorStoreLocalDatabase;

	CDistributedAppSensorReporter::CSensorInfo const *CFilterSensorKeyContext::f_GetSensor(CSensorKey const &_Key)
	{
		auto Mapping = m_SensorValues(_Key);
		auto &CachedValue = *Mapping;
		if (!Mapping.f_WasCreated())
			return CachedValue.m_pValue;

		if (m_Transaction.f_Get(_Key, CachedValue.m_Value))
			CachedValue.m_pValue = &CachedValue.m_Value.m_Info;

		return CachedValue.m_pValue;
	}

	bool fg_FilterSensorReadingValue
		(
			CSensorReadingValue const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
			, CSensorReadingKey const &_ReadingKey
			, CFilterSensorKeyContext &_Context
		)
	{
		if (!(_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems))
			return true;

		if (fg_FilterSensorValueStatusIsProblem(_Reading))
			return true;

		auto *pSensorInfo = _Context.f_GetSensor(_ReadingKey.f_SensorKey());
		if (pSensorInfo)
		{
			if (pSensorInfo->f_IsValueCritical(_Reading.m_Data))
				return true;
			else if (pSensorInfo->f_IsValueWarning(_Reading.m_Data))
				return true;
		}

		return false;
	}
}
