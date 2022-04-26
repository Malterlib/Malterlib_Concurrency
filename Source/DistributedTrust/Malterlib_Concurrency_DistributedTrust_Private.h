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
	struct CPermissionIdentifiers;
}

namespace NMib::NConcurrency::NPrivate
{
	struct CTrustedActorSubscriptionState : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
		TCWeakActor<CActor> m_DispatchActor;
		uint32 m_TypeHash = 0;
		CDistributedActorProtocolVersions m_ProtocolVersions;
		NStr::CStr m_NamespaceName;

		// Handled on dipatch actor
		NContainer::TCVector<NStorage::TCVariant<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>, NContainer::TCSet<CDistributedActorIdentifier>>> m_DeferredChanges;
	
		CTrustedActorSubscriptionState();
		virtual ~CTrustedActorSubscriptionState(); 
		virtual FUnitVoidFutureFunction f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors) = 0;
		virtual FUnitVoidFutureFunction f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> const &_Actors) = 0;
		void f_ApplyDeferredChanges();
	};
	
	struct CTrustedPermissionSubscriptionState : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		friend class CDistributedActorTrustManager;
	
		CTrustedPermissionSubscriptionState();
		~CTrustedPermissionSubscriptionState();

		// Valid anywhere
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;

		// Valid on dispatched actor
		CTrustedPermissionSubscription *m_pSubscription = nullptr;
		NFunction::TCFunctionMovable<void (CPermissionIdentifiers const &_Identity, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded)> m_fOnPermissionsAdded;
		NFunction::TCFunctionMovable<void (CPermissionIdentifiers const &_Identity, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> m_fOnPermissionsRemoved;
		
		// Valid on trust manager
		TCWeakActor<CActor> m_DispatchActor;
		NContainer::TCVector<NStr::CStr> m_Wildcards;

		TCFuture<void> f_AddPermissions(CPermissionIdentifiers const &_Identity, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded);
		TCFuture<void> f_RemovePermissions(CPermissionIdentifiers const &_Identity, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved);
		TCFuture<void> f_SetAuthenticatedPermissionPattern
			(
				CPermissionIdentifiers const &_Identity
				, NStr::CStr &&_Pattern
				, NContainer::TCSet<NStr::CStr> &&_AuthenticationFactors
				, NTime::CTime const &_ExpirationTime
			 	, NTime::CTime const &_CacheTime
			)
		;
		TCFuture<void> f_AddAuthenticatedPermission
			(
				CPermissionIdentifiers const &_Identity
				, NStr::CStr const &_Permission
				, NTime::CTime const &_ExpirationTime
			 	, NTime::CTime const &_CacheTime
			)
		;
	};
}

