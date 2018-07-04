// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorAuthenticationHandler>
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;

	struct CDistributedActorTrustManagerAuthenticationCache
	{
		friend class CDistributedActorTrustManager;	// To simplify creation of new trusted permission subscriptions

		bool f_HasAuthenticatedPermission(CPermissionIdentifiers const &_Identity, NStr::CStr const &_Permission, bool _bIsWildcard, int64 _MaximumAuthenticationLifetime);
		void f_AddAuthenticatedPermission(CPermissionIdentifiers const &_Identity, NStr::CStr const &_Permission, NTime::CTime const &_ExpirationTime, NTime::CTime const &_CacheTime);
		void f_RemoveAuthenticatedPermission(CPermissionIdentifiers const &_Identity, NStr::CStr const &_Permission);

		NContainer::TCSet<NStr::CStr> f_HasAuthenticatedPermissionPattern(CPermissionIdentifiers const &_Identity, NStr::CStr const &_Permission, int64 _MaximumAuthenticationLifetime);
		void f_SetAuthenticatedPermissionPattern
			(
				CPermissionIdentifiers const &_Identity
				, NStr::CStr &&_Pattern
				, NContainer::TCSet<NStr::CStr> &&_AuthenticationFactor
				, NTime::CTime const &_ExpirationTime
			 	, NTime::CTime const &_CacheTime
			)
		;
		TCContinuation<void> f_CleanupExpired();
		void f_StartPeriodicCleanup();

	private:
		struct CCacheLifetimes
		{
			NTime::CTime m_ExpirationTime;
			NTime::CTime m_CacheTime;
		};

		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CCacheLifetimes>> mp_AuthenticatedPermissions;
		// This maps Identity -> PermissionPattern -> Factor -> Expirationtime
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CCacheLifetimes>>> mp_PreauthenticatedPatterns;
		CActorSubscription mp_PeriodicCleanupSubscription; // TODO: Move the timer to the global cache in trust manager
	};
}
