// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
	struct CTrustedActorSubscriptionState
	{
		friend class NConcurrency::CDistributedActorTrustManager;

		NStorage::CIntrusiveRefCount m_RefCount;
		TCWeakActor<CActor> m_DispatchActor;
		uint32 m_TypeHash = 0;
		CDistributedActorProtocolVersions m_ProtocolVersions;
		NStr::CStr m_NamespaceName;
#if (DMibSysLogSeverities) & DMibConcurrency_SubscriptionLogVerbosity
		NStr::CStr m_DebugID;
#endif

		// Handled on dipatch actor
		NContainer::TCVector<NStorage::TCVariant<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>, NContainer::TCSet<CDistributedActorIdentifier>>> m_DeferredChanges;

		CTrustedActorSubscriptionState();
		virtual ~CTrustedActorSubscriptionState();
		virtual FUnitVoidFutureFunction f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> &&_Actors) = 0;
		virtual FUnitVoidFutureFunction f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> &&_Actors) = 0;
		void f_ApplyDeferredChanges();
	};

	struct CTrustedPermissionSubscriptionState
	{
		friend class NConcurrency::CDistributedActorTrustManager;

		CTrustedPermissionSubscriptionState();
		~CTrustedPermissionSubscriptionState();

		NStorage::CIntrusiveRefCount m_RefCount;

		// Valid anywhere
		TCWeakActor<CDistributedActorTrustManager> m_TrustManager;

		// Valid on dispatched actor
		CTrustedPermissionSubscription *m_pSubscription = nullptr;
		TCActorFunctorWeak<TCFuture<void> (CPermissionIdentifiers _Identity, NContainer::TCMap<NStr::CStr, CPermissionRequirements> _PermissionsAdded)> m_fOnPermissionsAdded;
		TCActorFunctorWeak<TCFuture<void> (CPermissionIdentifiers _Identity, NContainer::TCSet<NStr::CStr> _PermissionsRemoved)> m_fOnPermissionsRemoved;

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

