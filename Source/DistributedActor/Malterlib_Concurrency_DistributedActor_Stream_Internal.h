// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorSubscriptionReferenceState
	{
		CDistributedActorSubscriptionReferenceState();
		~CDistributedActorSubscriptionReferenceState();

		TCFuture<void> f_Destroy();

		NStorage::CIntrusiveRefCount m_RefCount;
		NStorage::TCSharedPointerSupportWeak<ICHost> m_pHost;
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStr::CStr m_SubscriptionID;
		NStr::CStr m_LastExecutionID;
	};
	
	struct CDistributedActorSubscriptionReference : public CActorSubscriptionReference
	{
		CDistributedActorSubscriptionReference();
		~CDistributedActorSubscriptionReference();

		TCFuture<void> f_Destroy() override;

		NStorage::TCSharedPointer<CDistributedActorSubscriptionReferenceState> m_pState;
	};
	
	struct CDistributedActorStreamContextState
	{
		struct CSubscriptionInfo
		{
			NStr::CStr m_SubscriptionID; 
			CActorSubscription m_Subscription;
			NContainer::TCVector<TCActor<CActor>> m_ActorReferences;
		};
		
		struct CActorFunctorInfo
		{
			struct CActorFunctors
			{
				NStr::CStr m_ActorID;
				TCWeakActor<CActor> m_Actor;
				NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointer<CStreamingFunction>> m_Functions;
				
				CActorFunctors();
				~CActorFunctors();
			};
			
			struct CActorInterface
			{
				NStr::CStr m_ActorID;
				TCWeakDistributedActor<CActor> m_Actor;
				NContainer::TCVector<uint32> m_Hierarchy;
				CDistributedActorProtocolVersions m_ProtocolVersions;
			};
			
			NContainer::TCVector<CActorFunctors> m_ActorFunctors;
			NContainer::TCVector<CActorInterface> m_Interfaces;
			NContainer::TCLinkedList<NStorage::TCSharedPointer<CDistributedActorSubscriptionReferenceState>> m_PendingSubscriptions;
			
			NContainer::TCSet<NStr::CStr> m_ImplicitlyPublishedFunctions;
			NContainer::TCSet<NStr::CStr> m_ImplicitlyPublishedActors;
			NContainer::TCSet<NStr::CStr> m_ImplicitlyPublishedInterfaces;
		};
		
		CDistributedActorStreamContextState(uint32 _ActorProtocolVersion, bool _bCallInitiator);
		virtual ~CDistributedActorStreamContextState();

		bool f_IsValid(NStr::CStr &o_Error) const;

		NStorage::CIntrusiveRefCount m_RefCount;

		NContainer::TCMap<uint32, CSubscriptionInfo> m_Subscriptions;
		NContainer::TCMap<uint32, CActorFunctorInfo> m_ActorFunctors;
		
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStr::CStr m_LastExecutionID;
		NStorage::TCSharedPointerSupportWeak<NActorDistributionManagerInternal::CHost> m_pHost;

		uint32 m_CurrentAutomaticNotRequiredSubscriptionID = gc_SubscriptionNotRequired + 128;
		uint32 m_ActorProtocolVersion;
		bool m_bCallInitiator;
	};
}
