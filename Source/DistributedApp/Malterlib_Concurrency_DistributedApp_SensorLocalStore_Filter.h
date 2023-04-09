// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency::NSensorStore
{
	struct CFilterSensorKeyContext
	{
		struct CSensorCacheValue
		{
			NSensorStoreLocalDatabase::CSensorValue m_Value;
			CDistributedAppSensorReporter::CSensorInfo const *m_pValue = nullptr;
		};

		CDistributedAppSensorReporter::CSensorInfo const *f_GetSensor(NSensorStoreLocalDatabase::CSensorKey const &_Key);

		NDatabase::CDatabaseSubReadTransaction &m_Transaction;

		TCMap<NSensorStoreLocalDatabase::CSensorKey, CSensorCacheValue> m_SensorValues;
	};

	template <typename tf_CKey>
	bool fg_FilterSensorKey(tf_CKey const &_Key, CDistributedAppSensorReader_SensorFilter const &_Filter);
	template <typename tf_CKey>
	bool fg_FilterSensorKey(tf_CKey const &_Key, NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> const &_Filters);

	template <typename tf_CReading>
	bool fg_FilterSensorValueStatusIsProblem(tf_CReading const &_Reading);

	template <typename tf_CSensor, typename tf_CReading>
	bool fg_FilterSensorValueStatus
		(
			tf_CSensor const &_Sensor
			, tf_CReading const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
			, NTime::CTime const &_Now
		)
	;

	bool fg_FilterSensorReadingValue
		(
			NSensorStoreLocalDatabase::CSensorReadingValue const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
			, NSensorStoreLocalDatabase::CSensorReadingKey const &_ReadingKey
			, CFilterSensorKeyContext &_Context
		)
	;

	template <typename tf_CSensor, typename tf_CReading>
	bool fg_FilterSensorReadingValueWithSensor
		(
			tf_CSensor const &_Sensor
			, tf_CReading const &_Reading
			, CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag _Flags
		)
	;
}

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Filter.hpp"
