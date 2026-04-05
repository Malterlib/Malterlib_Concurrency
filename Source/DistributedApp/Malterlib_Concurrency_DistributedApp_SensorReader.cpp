// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorReader.h"

namespace NMib::NConcurrency
{
	CDistributedAppSensorReader::CDistributedAppSensorReader()
	{
		DMibPublishActorFunction(CDistributedAppSensorReader::f_GetSensors);
		DMibPublishActorFunction(CDistributedAppSensorReader::f_GetSensorReadings);
		DMibPublishActorFunction(CDistributedAppSensorReader::f_GetSensorStatus);
	}

	CDistributedAppSensorReader::~CDistributedAppSensorReader() = default;

	CDistributedAppSensorReader_SensorReadingFilter CDistributedAppSensorReader_SensorReadingSubscriptionFilter::f_ToReadingFilter() const
	{
		CDistributedAppSensorReader_SensorReadingFilter Return;

		Return.m_SensorFilter = m_SensorFilter;
		Return.m_MinSequence = m_MinSequence;
		Return.m_MinTimestamp = m_MinTimestamp;
		Return.m_Flags = m_Flags;

		return Return;
	}
}
