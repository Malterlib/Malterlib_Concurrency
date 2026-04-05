// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorData : public ICDistributedActorData
	{
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStorage::TCWeakPointer<NPrivate::ICHost> m_pHost;
		CDistributedActorHostInfo f_GetHostInfo() const override
		{
			auto pHost = m_pHost.f_Lock();
			if (!pHost)
				return {};

			return pHost->m_HostInfo;
		}
	};
}
