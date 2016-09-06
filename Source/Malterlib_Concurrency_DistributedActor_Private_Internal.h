// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorStreamContextSendState : NPtr::TCSharedPointerIntrusiveBase<>
	{
		NPtr::TCSharedPointer<NPrivate::CDistributedActorData> m_pActorData;
		NStr::CStr m_LastExecutionID;
		NContainer::TCMap<NStr::CStr, NPtr::TCSharedPointer<CStreamingFunction>> m_Functions;
		NStr::CStr m_ActorID;
		TCWeakActor<CActor> m_Actor;
		
		struct COwning
		{
			NContainer::TCSet<NStr::CStr> m_Functions;
			NContainer::TCSet<NStr::CStr> m_Actors;
			NPtr::TCSharedPointer<NPrivate::CDistributedActorData> m_pActorData;
		};
		
		NPtr::TCSharedPointer<COwning> m_pOwning;
		
		bool m_bSubscriptionReturned = false;
		bool m_bReceivedValue = false;
		
		bool f_IsValid() const;
		
		~CDistributedActorStreamContextSendState();
	};
	
	struct CDistributedActorStreamContextReceiveState : NPtr::TCSharedPointerIntrusiveBase<>
	{
		NPtr::TCSharedPointer<NActorDistributionManagerInternal::CHost, NPtr::CSupportWeakTag> m_pHost;
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStr::CStr m_SubscriptionID; 
		CActorSubscription m_Subscription;
		NContainer::TCVector<TCActor<CActor>> m_ActorReferences;
	};
}
