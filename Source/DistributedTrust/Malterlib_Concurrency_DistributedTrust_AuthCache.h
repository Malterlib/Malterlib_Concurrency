// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorAuthenticationHandler>
#include "Malterlib_Concurrency_DistributedTrust_Shared.h"

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;

	struct CDistributedActorTrustManagerAuthenticationCache
	{
		friend class CDistributedActorTrustManager; // To simplify creation of new trusted permission subscriptions

		CDistributedActorTrustManagerAuthenticationCache();
		CDistributedActorTrustManagerAuthenticationCache(CDistributedActorTrustManagerAuthenticationCache &&_Other);
		CDistributedActorTrustManagerAuthenticationCache &operator = (CDistributedActorTrustManagerAuthenticationCache &&_Other);
		~CDistributedActorTrustManagerAuthenticationCache();

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
		TCFuture<void> f_CleanupExpired();
		void f_StartPeriodicCleanup();

	private:
		struct CCacheLifetimes
		{
			NTime::CTime m_ExpirationTime;
			NTime::CTime m_CacheTime;
		};

		// Gives the cleanup timer the current address of the cache. The move operations of the cache update m_pCache, and
		// stopping the cleanup clears it.
		struct CTimerTarget
		{
			CDistributedActorTrustManagerAuthenticationCache *m_pCache = nullptr;
		};

		void fp_StopPeriodicCleanup();

		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CCacheLifetimes>> mp_AuthenticatedPermissions;
		// This maps Identity -> PermissionPattern -> Factor -> Expirationtime
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CCacheLifetimes>>> mp_PreauthenticatedPatterns;
		CActorSubscription mp_PeriodicCleanupSubscription; // TODO: Move the timer to the global cache in trust manager
		NStorage::TCSharedPointer<CTimerTarget> mp_pTimerTarget;
	};
}
