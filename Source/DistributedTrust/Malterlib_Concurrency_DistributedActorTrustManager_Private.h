// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;
	struct CTrustedPermissionSubscription;
	struct CTrustedActorInfo;
	template <typename t_CActor>
	struct TCTrustedActor;
}

namespace NMib::NConcurrency::NPrivate
{
	struct CTrustedActorSubscriptionState : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
		TCWeakActor<CActor> m_DispatchActor;
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;
		uint32 m_TypeHash = 0;
		CDistributedActorProtocolVersions m_ProtocolVersions;
		NStr::CStr m_NamespaceName;
	
		CTrustedActorSubscriptionState();
		virtual ~CTrustedActorSubscriptionState(); 
		virtual TCDispatchedWeakActorCall<void> f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors) = 0;
		virtual TCDispatchedWeakActorCall<void> f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> const &_Actors) = 0;
	};
	
	struct CTrustedPermissionSubscriptionState : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
	
		CTrustedPermissionSubscriptionState();
		~CTrustedPermissionSubscriptionState();

		// Valid on dispatched actor
		CTrustedPermissionSubscription *m_pSubscription = nullptr;
		NFunction::TCFunctionMovable<void (NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded)> m_fOnPermissionsAdded;
		NFunction::TCFunctionMovable<void (NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> m_fOnPermissionsRemoved;
		
		// Valid on trust manager
		TCWeakActor<CActor> m_DispatchActor;
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;
		NContainer::TCVector<NStr::CStr> m_Wildcards;

		TCDispatchedWeakActorCall<void> f_AddPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded);
		TCDispatchedWeakActorCall<void> f_RemovePermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved);
	};
}
