// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;
	struct CTrustedPermissionSubscription;
	struct CTrustedActorInfo;
}

namespace NMib::NConcurrency::NPrivate
{
	struct CTrustedActorSubscriptionState : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
		TCWeakActor<CActor> m_DispatchActor;
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;
		uint32 m_TypeHash = 0;
		NStr::CStr m_NamespaceName;
	
		CTrustedActorSubscriptionState();
		virtual ~CTrustedActorSubscriptionState(); 
		virtual void f_AddDistributedActors(NContainer::TCMap<TCDistributedActor<CActor>, CTrustedActorInfo> const &_Actors) = 0;
		virtual void f_RemoveDistributedActors(NContainer::TCSet<TCWeakDistributedActor<CActor>> const &_Actors) = 0;
	};
	
	struct CTrustedPermissionSubscriptionState : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
	
		CTrustedPermissionSubscriptionState();
		~CTrustedPermissionSubscriptionState();

		// Valid on dispatched actor
		CTrustedPermissionSubscription *m_pSubscription = nullptr;
		NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded)> m_fOnPermissionsAdded;
		NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> m_fOnPermissionsRemoved;
		
		// Valid on trust manager
		TCWeakActor<CActor> m_DispatchActor;
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;
		NContainer::TCVector<NStr::CStr> m_Wildcards;

		void f_AddPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded);
		void f_RemovePermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved);
	};
}
