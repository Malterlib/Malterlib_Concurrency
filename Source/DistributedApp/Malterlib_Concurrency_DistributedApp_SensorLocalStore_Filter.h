// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency::NSensorStore
{
	template <typename tf_CKey>
	bool fg_FilterSensorKey(tf_CKey const &_Key, CDistributedAppSensorReader_SensorFilter const &_Filter);
}

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Filter.hpp"
