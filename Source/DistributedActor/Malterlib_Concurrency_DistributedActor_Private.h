// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorData : public ICDistributedActorData
	{
		CDistributedActorHostInfo f_GetHostInfo() const override;

		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStorage::TCWeakPointer<NPrivate::ICHost> m_pHost;
	};
}
