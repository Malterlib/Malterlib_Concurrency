// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogForwarder : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		// _bColorLogs decides whether monitored processes are asked to write colored log files;
		// pass the effective color setting of the caller so --no-color reaches the forwarded logs
		CDistributedAppLogForwarder(NStr::CStr const &_RootPath, bool _bColorLogs);
		~CDistributedAppLogForwarder();

		struct CInternal;

		TCFuture<void> f_StartMonitoring();

	private:
		TCFuture<void> fp_Destroy() override;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
