// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorData : public ICDistributedActorData
	{
		bool f_IsValidForCall() const override;
		
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStr::CStr m_ActorID;
		NStorage::TCWeakPointer<NPrivate::ICHost> m_pHost;		
		uint32 m_ProtocolVersion = TCLimitsInt<uint32>::mc_Max;
		bool m_bWasDestroyed = false;
		bool m_bRemote = false;
	};
}
