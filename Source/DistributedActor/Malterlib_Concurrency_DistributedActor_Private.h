// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorData : public ICDistributedActorData
	{
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStorage::TCWeakPointer<NPrivate::ICHost> m_pHost;
	};
}
