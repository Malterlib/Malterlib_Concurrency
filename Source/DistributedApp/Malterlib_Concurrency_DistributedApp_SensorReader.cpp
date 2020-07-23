// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
}
