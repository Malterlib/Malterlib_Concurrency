// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NStr
{
	extern template struct TCStrFormatType_Inline<TCFormat<CStrTraits_CStr>, NConcurrency::CDistributedAppSensorReporter::CVersion, true>;
}
namespace NMib::NConcurrency
{
	extern template void CDistributedAppSensorReporter::CVersion::f_Format<NStr::CStr>(NStr::CStr &o_Str) const;
	extern template TCActorFunctor<TCFuture<void> (NContainer::TCVector<CDistributedAppSensorReporter::CSensorReading> _Readings)>::~TCActorFunctor();
}
