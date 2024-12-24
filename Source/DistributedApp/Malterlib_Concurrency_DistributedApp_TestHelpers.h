// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogForwarder : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		CDistributedAppLogForwarder(NStr::CStr const &_RootPath);
		~CDistributedAppLogForwarder();

		struct CInternal;

		TCFuture<void> f_StartMonitoring();

	private:
		TCFuture<void> fp_Destroy() override;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
