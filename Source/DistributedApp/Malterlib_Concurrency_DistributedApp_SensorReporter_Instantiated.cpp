// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorReporter.h"

namespace NMib::NStr
{
	template struct TCStrFormatType_Inline<TCFormat<CStrTraits_CStr>, NConcurrency::CDistributedAppSensorReporter::CVersion, true>;
}
namespace NMib::NConcurrency
{
	template void CDistributedAppSensorReporter::CVersion::f_Format<NStr::CStr>(NStr::CStr &o_Str) const;
	template TCActorFunctor<TCFuture<void> (NContainer::TCVector<CDistributedAppSensorReporter::CSensorReading> _Readings)>::~TCActorFunctor();
}
