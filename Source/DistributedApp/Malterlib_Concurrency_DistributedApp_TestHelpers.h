// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogForwarder : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		struct CInternal;

		CDistributedAppLogForwarder(NStr::CStr const &_RootPath, bool _bColorLogs);
		~CDistributedAppLogForwarder();

		TCFuture<void> f_StartMonitoring();

	private:
		TCFuture<void> fp_Destroy() override;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
