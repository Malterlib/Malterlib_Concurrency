// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"

namespace NMib::NConcurrency
{
	auto CDistributedActorTrustManager::f_EnumPermissions(bool _bIncludeHostInfo) -> TCFuture<CDistributedActorTrustManagerInterface::CEnumPermissionsResult>
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CDistributedActorTrustManagerInterface::CEnumPermissionsResult Return;
		NContainer::TCSet<NStr::CStr> HostIDs;
		NContainer::TCSet<NStr::CStr> UserIDs;
		NContainer::TCMap<NStr::CStr, NStr::CStr> AuthenticationFactors;
		for (auto &Permissions : Internal.m_Permissions)
		{
			if (Permissions.m_Permissions.m_Permissions.f_IsEmpty())
				continue;
			auto &Identity = Permissions.f_GetIdentity();
			HostIDs[Identity.f_GetHostID()];
			UserIDs[Identity.f_GetUserID()];
			for (auto &Permission : Permissions.m_Permissions.m_Permissions)
			{
				auto &OutPermissions = Return.m_Permissions[Permissions.m_Permissions.m_Permissions.fs_GetKey(Permission)];
				OutPermissions[Identity];
			}
		}
		HostIDs.f_Remove("");
		UserIDs.f_Remove("");
		for (auto &Permission : Internal.m_RegisteredPermissions)
			Return.m_Permissions[Permission];

		if (!_bIncludeHostInfo)
			co_return fg_Move(Return);

		for (auto &Permissions : Internal.m_Permissions)
		{
			if (Permissions.m_Permissions.m_Permissions.f_IsEmpty())
				continue;
			auto &Identity = Permissions.f_GetIdentity();
			for (auto &AuthenticationFactors : Permissions.m_Permissions.m_Permissions)
			{
				if (!AuthenticationFactors.m_AuthenticationFactors.f_IsEmpty())
				{
					auto &OutPermissions = Return.m_Permissions[Permissions.m_Permissions.m_Permissions.fs_GetKey(AuthenticationFactors)];
					OutPermissions[Identity].m_AuthenticationFactors = AuthenticationFactors.m_AuthenticationFactors;
					OutPermissions[Identity].m_MaximumAuthenticationLifetime = AuthenticationFactors.m_MaximumAuthenticationLifetime;
				}
			}
		}

		TCFutureMap<NStr::CStr, CHostInfo> HostInfosFuture;
		for (auto &HostID : HostIDs)
			fp_GetHostInfo(HostID) > HostInfosFuture[HostID];

		NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> UserInfos;
		for (auto &UserID : UserIDs)
		{
			if (auto *pUser = Internal.m_Users.f_FindEqual(UserID))
				UserInfos[UserID] = CDistributedActorTrustManagerInterface::CUserInfo{pUser->m_UserInfo.m_UserName, pUser->m_UserInfo.m_Metadata};
		}

		NContainer::TCMap<NStr::CStr, TCAsyncResult<CHostInfo>> HostInfos = co_await fg_AllDoneWrapped(HostInfosFuture);

		for (auto &Permissions : Return.m_Permissions)
		{
			for (auto &PermissionInfo : Permissions)
			{
				auto const &Identity = Permissions.fs_GetKey(PermissionInfo);
				auto &HostID = Identity.f_GetHostID();
				auto &UserID = Identity.f_GetUserID();
				auto *pHostInfo = HostInfos.f_FindEqual(HostID);
				if (pHostInfo && *pHostInfo)
					PermissionInfo.m_HostInfo = (**pHostInfo);
				if (auto *pUserInfo = UserInfos.f_FindEqual(UserID))
					PermissionInfo.m_UserInfo = {pUserInfo->m_UserName, pUserInfo->m_Metadata};
			}
		}

		co_return fg_Move(Return);
	}

	TCFuture<void> CDistributedActorTrustManager::f_AddPermissions
		(
			CPermissionIdentifiers _Identity
			, NContainer::TCMap<NStr::CStr, CPermissionRequirements> _Permissions
			, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		using namespace NStr;

		auto &UserID = _Identity.f_GetUserID();
		auto &HostID = _Identity.f_GetHostID();
		if (!HostID && !UserID)
			co_return DMibErrorInstance("Cannot omit both host id and user id");
		if (HostID && !CActorDistributionManager::fs_IsValidHostID(HostID))
			co_return DMibErrorInstance("Invalid host id");
		if (UserID && !CActorDistributionManager::fs_IsValidUserID(UserID))
			co_return DMibErrorInstance("Invalid user id");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto &Permissions = Internal.m_Permissions[_Identity];

		for (auto const &Requirements : _Permissions)
		{
			for (auto const &Factors : Requirements.m_AuthenticationFactors)
			{
				for (auto const &Factor : Factors)
				{
					if (!Internal.m_AuthenticationActors.f_FindEqual(Factor))
						co_return DMibErrorInstance("No authentication factor named '{}'"_f << Factor);
				}
			}
		}

		NContainer::TCMap<NStr::CStr, CPermissionRequirements> PermissionsAdded;
		for (auto &Requirements : _Permissions)
		{
			auto const &Permission = _Permissions.fs_GetKey(Requirements);
			if (auto Mapped = Permissions.m_Permissions.m_Permissions(Permission, Requirements); Mapped.f_WasCreated())
				PermissionsAdded[Permission] = Requirements;
			else if (*Mapped != Requirements)
			{
				*Mapped = Requirements;
				PermissionsAdded[Permission] = Requirements;
			}
		}
		if (PermissionsAdded.f_IsEmpty())
			co_return {};

		TCFutureVector<void> SubscriptionResults;

		for (auto &Subscription : Internal.m_PermissionsSubscriptions)
		{
			auto const &Wildcard = Internal.m_PermissionsSubscriptions.fs_GetKey(Subscription);
			NContainer::TCMap<NStr::CStr, CPermissionRequirements> FilteredPermissions;
			for (auto &Requirements : PermissionsAdded)
			{
				auto const &Permission = PermissionsAdded.fs_GetKey(Requirements);
				if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;
				if (auto *pRequirements = Subscription.m_PermissionsPerIdentity[_Identity].f_FindEqual(Permission); pRequirements && *pRequirements == Requirements)
					continue;

				Subscription.m_PermissionsPerIdentity[_Identity][Permission] = Requirements;
				FilteredPermissions[Permission] = Requirements;
			}

			if (FilteredPermissions.f_IsEmpty())
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
				pSubscription->f_AddPermissions(_Identity, FilteredPermissions) > SubscriptionResults;
		}

		TCFuture<void> SaveDatabaseFuture;
		if (Permissions.m_bExistsInDatabase)
			SaveDatabaseFuture = Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetPermissions, _Identity, Permissions.m_Permissions);
		else
		{
			Permissions.m_bExistsInDatabase = true;
			SaveDatabaseFuture = Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddPermissions, _Identity, Permissions.m_Permissions);
		};

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
		{
			co_await (fg_Move(SaveDatabaseFuture) + fg_AllDoneWrapped(SubscriptionResults)); // Intentionally ignore results

			co_return {};
		}

		fg_AllDoneWrapped(SubscriptionResults).f_DiscardResult();

		co_return co_await fg_Move(SaveDatabaseFuture);
	}

	TCFuture<void> CDistributedActorTrustManager::f_RemovePermissions
		(
			CPermissionIdentifiers _Identity
			 , NContainer::TCSet<NStr::CStr> _Permissions
			 , EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		auto &UserID = _Identity.f_GetUserID();
		auto &HostID = _Identity.f_GetHostID();
		if (!HostID && !UserID)
			co_return DMibErrorInstance("Cannot omit both host id and user id");
		if (HostID && !CActorDistributionManager::fs_IsValidHostID(HostID))
			co_return DMibErrorInstance("Invalid host id");
		if (UserID && !CActorDistributionManager::fs_IsValidUserID(UserID))
			co_return DMibErrorInstance("Invalid user id");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pPermissions = Internal.m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			co_return {};
		auto &Permissions = *pPermissions;

		NContainer::TCSet<NStr::CStr> PermissionsRemoved;
		for (auto &Permission : _Permissions)
		{
			if (Permissions.m_Permissions.m_Permissions.f_Remove(Permission))
				PermissionsRemoved[Permission];
			Internal.m_AuthenticationCache.f_RemoveAuthenticatedPermission(_Identity, Permission); // Invalidate cached permission
		}
		if (PermissionsRemoved.f_IsEmpty())
			co_return {};

		TCFutureVector<void> SubscriptionResults;

		for (auto &Subscription : Internal.m_PermissionsSubscriptions)
		{
			auto const &Wildcard = Internal.m_PermissionsSubscriptions.fs_GetKey(Subscription);
			NContainer::TCSet<NStr::CStr> FilteredPermissions;
			for (auto &Permission : PermissionsRemoved)
			{
				if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;
				if (!Subscription.m_PermissionsPerIdentity[_Identity].f_Remove(Permission))
					continue;
				FilteredPermissions[Permission];
			}

			if (FilteredPermissions.f_IsEmpty())
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
				pSubscription->f_RemovePermissions(_Identity, FilteredPermissions) > SubscriptionResults;
		}

		if (Permissions.m_Permissions.m_Permissions.f_IsEmpty())
		{
			if (Permissions.m_bExistsInDatabase)
			{
				Permissions.m_bExistsInDatabase = false;
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemovePermissions, _Identity);
			}
			Internal.m_Permissions.f_Remove(_Identity);
		}
		else
		{
			if (Permissions.m_bExistsInDatabase)
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetPermissions, _Identity, Permissions.m_Permissions);
			else
			{
				Permissions.m_bExistsInDatabase = true;
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddPermissions, _Identity, Permissions.m_Permissions);
			}
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
			co_await fg_AllDoneWrapped(SubscriptionResults);
		else
			fg_AllDoneWrapped(SubscriptionResults).f_DiscardResult();

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_RegisterPermissions(NContainer::TCSet<NStr::CStr> _Permissions)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		Internal.m_RegisteredPermissions += _Permissions;
		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> _Permissions)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		Internal.m_RegisteredPermissions -= _Permissions;
		co_return {};
	}

	TCFuture<bool> CDistributedActorTrustManager::CInternal::f_AuthenticatePermissionPattern
		(
			NStr::CStr _Pattern
			, NContainer::TCSet<NStr::CStr> _AuthenticationFactors
			, CCallingHostInfo _CallingHostInfo
			, NStr::CStr _RequestID
		)
	{
		using namespace NStr;

		DMibRequire(_CallingHostInfo.f_GetRealHostID());
		auto const &UserID = _CallingHostInfo.f_GetClaimedUserID();
		auto const &HostID = _CallingHostInfo.f_GetRealHostID();
		if (!HostID)
			co_return DMibErrorInstance("Cannot omit host id");
		if (!UserID)
			co_return DMibErrorInstance("Cannot omit user id");
		if (!CActorDistributionManager::fs_IsValidHostID(HostID))
			co_return DMibErrorInstance("Invalid host id");
		if (!CActorDistributionManager::fs_IsValidUserID(UserID))
			co_return DMibErrorInstance("Invalid user id");

		for (auto const &Factor : _AuthenticationFactors)
		{
			if (!m_AuthenticationActors.f_FindEqual(Factor))
				co_return DMibErrorInstance("No authentication factor named '{}'"_f << Factor);
		}

		auto AuthenticationHandler = _CallingHostInfo.f_GetAuthenticationHandler();
		if (!AuthenticationHandler)
			co_return false;

		using CHandler = ICDistributedActorAuthenticationHandler;

		CHandler::CRequest Request;
		Request.m_Description = "Provide advance authentication for all permissions matching the pattern";
		Request.m_RequestedPermissions = {{CHandler::CPermissionWithRequirements{_Pattern, "", {_AuthenticationFactors}, -1}}};

		auto CacheTime = NTime::CTime::fs_NowUTC();

		auto Challenge = CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(UserID);
		auto Responses = co_await AuthenticationHandler.f_CallActor(&CHandler::f_RequestAuthentication)(Request, Challenge, _RequestID).f_Wrap();
		if (!Responses)
		{
			DMibLogWithCategory(Mib/Concurrency/Trust, Error, "Failed to authenticate command: {}", Responses.f_GetExceptionStr());
			co_return Responses.f_GetException();
		}

		if (Responses->f_IsEmpty()) // This can happen if, for example, there is a typo in the factor name
			co_return false;

		auto VerificationResults = co_await m_pThis->f_VerifyAuthenticationResponses(Challenge, Request, *Responses);
		DMibCheck(VerificationResults.f_GetLen() == Responses->f_GetLen());

		bool bAllSuccess = true;

		NContainer::TCSet<NStr::CStr> SuccessfulAuthenticationFactors;
		NTime::CTime ExpirationTime = NTime::CTime::fs_EndOfTime();

		{
			auto iResponse = Responses->f_GetIterator();
			for (auto iVerificationResult = VerificationResults.f_GetIterator(); iVerificationResult; ++iVerificationResult, ++iResponse)
			{
				if (*iVerificationResult)
				{
					SuccessfulAuthenticationFactors[iResponse->m_FactorName];
					ExpirationTime = fg_Min(iResponse->m_SignedProperties.m_ExpirationTime, ExpirationTime);
				}
				else
					bAllSuccess = false;
			}
		}

		if (SuccessfulAuthenticationFactors.f_IsEmpty())
			co_return false;

		CPermissionIdentifiers Identity(HostID, UserID);

		// This is a simpler case than for permissions. We don't care if the permission already is in the cache. We will always add the new pattern
		// since the lifetime will be different from the pattern in the cache. For the same reason we will also propagate it to all permission subscriptions
		// with a matching pattern. The cached patterns are not written to the database.
		m_AuthenticationCache.f_SetAuthenticatedPermissionPattern(Identity, fg_TempCopy(_Pattern), fg_TempCopy(SuccessfulAuthenticationFactors), ExpirationTime, CacheTime);

		TCFutureVector<void> SubscriptionResults;
		for (auto &Subscription : m_PermissionsSubscriptions)
		{
			auto const &SubscriptionWildcard = m_PermissionsSubscriptions.fs_GetKey(Subscription);
			// Note that the matching is "other" way this time and the subscription wildcard appears as the first argument.
			// We check if our pattern can match anything the subscription wildcard would match
			if (NStr::fg_StrMatchWildcard(SubscriptionWildcard.f_GetStr(), _Pattern.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
			{
				pSubscription->f_SetAuthenticatedPermissionPattern(Identity, fg_TempCopy(_Pattern), fg_TempCopy(SuccessfulAuthenticationFactors), ExpirationTime, CacheTime)
					> SubscriptionResults
				;
			}
		}

		co_await fg_AllDoneWrapped(SubscriptionResults);

		co_return bAllSuccess;
	}

	TCFuture<void> CDistributedActorTrustManager::f_UpdateAuthenticationCache
		(
			CPermissionIdentifiers _Identity
			, NContainer::TCSet<NStr::CStr> _AuthenticatedPermissions
			, NTime::CTime _ExpirationTime
			, NTime::CTime _CacheTime
		)
	{
		DMibRequire(_Identity.f_GetHostID() && _Identity.f_GetUserID());
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!_ExpirationTime.f_IsValid())
			co_return {};

		TCFutureVector<void> SubscriptionResults;
		for (auto const &Permission : _AuthenticatedPermissions)
		{
			Internal.m_AuthenticationCache.f_AddAuthenticatedPermission(_Identity, Permission, _ExpirationTime, _CacheTime);

			for (auto &Subscription : Internal.m_PermissionsSubscriptions)
			{
				auto const &SubscriptionWildcard = Internal.m_PermissionsSubscriptions.fs_GetKey(Subscription);
				if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), SubscriptionWildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;

				for (auto &pSubscription : Subscription.m_Subscriptions)
					pSubscription->f_AddAuthenticatedPermission(_Identity, Permission, _ExpirationTime, _CacheTime) > SubscriptionResults;
			}
		}

		co_await fg_AllDoneWrapped(SubscriptionResults);
		co_return {};
	}

	TCFuture<CTrustedPermissionSubscription> CDistributedActorTrustManager::f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> _Wildcards, TCActor<CActor> _Actor)
	{
		if (!_Actor)
			co_return DMibErrorInstance("Invalid destination actor");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TrustManager = fg_ThisActor(this);
		State.m_Wildcards = _Wildcards;

		CTrustedPermissionSubscription Result;
		Result.mp_pState = pState;
		pState->m_pSubscription = &Result;
		Result.mp_Permissions = fp_SubscribeToPermissions(pState);
		Result.mp_AuthenticationCache = fp_FilterCachedAuthentications(pState);

		co_return fg_Move(Result);
	}

	namespace NPrivate
	{
		CTrustedPermissionSubscriptionState::CTrustedPermissionSubscriptionState()
		{
		}

		CTrustedPermissionSubscriptionState::~CTrustedPermissionSubscriptionState()
		{
		}

		TCFuture<void> CTrustedPermissionSubscriptionState::f_AddPermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded
			)
		{
			return fg_Dispatch
				(
					m_DispatchActor
					, [this, _Identity, _PermissionsAdded, pThis = NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}] -> TCFuture<void>
					{
						if (!m_pSubscription)
							co_return {};

						auto &Subscription = *m_pSubscription;
						auto &Permissions = Subscription.mp_Permissions[_Identity];
						NContainer::TCMap<NStr::CStr, CPermissionRequirements> Added;
						for (auto &Requirements : _PermissionsAdded)
						{
							auto &Permission = _PermissionsAdded.fs_GetKey(Requirements);
							if (auto *pRequirements = Permissions.f_FindEqual(Permission); !pRequirements || *pRequirements != Requirements)
								Added[Permission] = Requirements;
							Permissions[Permission] = Requirements;
						}
						if (!Added.f_IsEmpty() && m_fOnPermissionsAdded)
							co_await m_fOnPermissionsAdded(_Identity, Added);

						co_return {};
					}
				)
			;
		}

		TCFuture<void> CTrustedPermissionSubscriptionState::f_RemovePermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved
			)
		{
			return fg_Dispatch
				(
					m_DispatchActor
					, [this, _Identity, _PermissionsRemoved, pThis = NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}] -> TCFuture<void>
					{
						if (!m_pSubscription)
							co_return {};

						auto &Subscription = *m_pSubscription;
						auto *pPermissions = Subscription.mp_Permissions.f_FindEqual(_Identity);
						if (!pPermissions)
							co_return {};

						auto &Permissions = *pPermissions;
						NContainer::TCSet<NStr::CStr> Removed;
						for (auto &Permission : _PermissionsRemoved)
						{
							if (Permissions.f_Remove(Permission))
								Removed[Permission];
							Subscription.mp_AuthenticationCache.f_RemoveAuthenticatedPermission(_Identity, Permission); // Invalidate cached permission
						}
						if (Permissions.f_IsEmpty())
							Subscription.mp_Permissions.f_Remove(_Identity);
						if (!Removed.f_IsEmpty() && m_fOnPermissionsRemoved)
							co_await m_fOnPermissionsRemoved(_Identity, Removed);

						co_return {};
					}
				)
			;
		}

		TCFuture<void> CTrustedPermissionSubscriptionState::f_SetAuthenticatedPermissionPattern
			(
				CPermissionIdentifiers const &_Identity
				, NStr::CStr &&_Pattern
				, NContainer::TCSet<NStr::CStr> &&_AuthenticationFactors
				, NTime::CTime const &_ExpirationTime
				, NTime::CTime const &_CacheTime
			)
		{
			return g_Dispatch(m_DispatchActor) /
				[
					this
					, _Identity
					, Pattern = fg_Move(_Pattern)
					, AuthenticationFactors = fg_Move(_AuthenticationFactors)
					, _ExpirationTime
					, _CacheTime
					, pThis = NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}
				] () mutable
				{
					if (!m_pSubscription)
						return;

					m_pSubscription->mp_AuthenticationCache.f_SetAuthenticatedPermissionPattern(_Identity, fg_Move(Pattern), fg_Move(AuthenticationFactors), _ExpirationTime, _CacheTime);
				}
			;
		}

		TCFuture<void> CTrustedPermissionSubscriptionState::f_AddAuthenticatedPermission
			(
				CPermissionIdentifiers const &_Identity
				, NStr::CStr const &_Permission
				, NTime::CTime const &_ExpirationTime
				, NTime::CTime const &_CacheTime
			)
		{
			return g_Dispatch(m_DispatchActor) /
				[
					this
					, _Identity
					, _Permission
					, _ExpirationTime
					, _CacheTime
					, pThis = NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}
				] () mutable
				{
					if (!m_pSubscription)
						return;

					m_pSubscription->mp_AuthenticationCache.f_AddAuthenticatedPermission(_Identity, _Permission, _ExpirationTime, _CacheTime);
				}
			;
		}
	}

	CTrustedPermissionSubscription::CTrustedPermissionSubscription(CTrustedPermissionSubscription &&_Other)
		: mp_pState(fg_Move(_Other.mp_pState))
		, mp_Permissions(fg_Move(_Other.mp_Permissions))
		, mp_AuthenticationCache(fg_Move(_Other.mp_AuthenticationCache))
	{
		if (mp_pState)
			mp_pState->m_pSubscription = this;
	}

	CTrustedPermissionSubscription &CTrustedPermissionSubscription::operator =(CTrustedPermissionSubscription &&_Other)
	{
		mp_pState = fg_Move(_Other.mp_pState);
		mp_Permissions = fg_Move(_Other.mp_Permissions);
		if (mp_pState)
			mp_pState->m_pSubscription = this;
		mp_AuthenticationCache = fg_Move(_Other.mp_AuthenticationCache);

		return *this;
	}

	CTrustedPermissionSubscription::~CTrustedPermissionSubscription()
	{
		f_Clear();
	}

	void CTrustedPermissionSubscription::f_Clear()
	{
		if (!mp_pState)
			return;
		auto &State = *mp_pState;
		auto TrustManager = State.m_TrustManager.f_Lock();
		if (TrustManager)
			TrustManager.f_Bind<&CDistributedActorTrustManager::fp_UnsubscribeToPermissions>(mp_pState).f_DiscardResult();
		State.m_pSubscription = nullptr;
		State.m_fOnPermissionsAdded.f_Clear();
		State.m_fOnPermissionsRemoved.f_Clear();
		mp_pState = nullptr;
		mp_Permissions.f_Clear();
	}

	CPermissionQuery::CPermissionQuery() = default;

	CPermissionQuery::CPermissionQuery(TCInitializerList<NStr::CStr> const &_Init)
		: m_Permissions{_Init}
	{
	}

	CPermissionQuery &&CPermissionQuery::f_Description(NStr::CStr const &_Description) &&
	{
		m_Description = _Description;
		return fg_Move(*this);
	}

	CPermissionQuery &&CPermissionQuery::f_Wildcard(bool _bIsWildcard) &&
	{
		m_bIsWildcard = _bIsWildcard;
		return fg_Move(*this);
	}

	CPermissionQuery::EQueryResult CPermissionQuery::f_MatchesPermission
		(
			NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
			, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements> &o_RequestedPermissions
			, CPermissionIdentifiers const &_Identity
			, CPermissionIdentifiers const &_FullIdentity
			, CDistributedActorTrustManagerAuthenticationCache &_AuthenticationCache
		) const
	{
		auto fFindAllInGroup = [](NContainer::TCSet<NStr::CStr> const &_Preauthenticated, NContainer::TCSet<NStr::CStr> const &_FactorGroup)
			{
				for (auto const &Factor : _FactorGroup)
				{
					if (!_Preauthenticated.f_FindEqual(Factor))
						return false;
				}
				return true;
			}
		;
		auto fFindAll = [fFindAllInGroup](NContainer::TCSet<NStr::CStr> const &_Preauthenticated, NContainer::TCSet<NContainer::TCSet<NStr::CStr>> const &_RequiredFactors)
			{
				for (auto const &FactorGroup : _RequiredFactors)
				{
					if (fFindAllInGroup(_Preauthenticated, FactorGroup))
						return true;
				}
				return false;
			}
		;

		EQueryResult Result = EQueryResult_NoPermission;
		// CAuthenticationInstance Authentication;
		// Does the host permissions contain a match for any of the permissions in m_Permissions?
		if (m_bIsWildcard)
		{
			for (auto &AuthenticationFactors : _Permissions)
			{
				for (auto const &AnyPermission : m_Permissions)
				{
					auto &Permission = _Permissions.fs_GetKey(AuthenticationFactors);
					if (NStr::fg_StrMatchWildcard(AnyPermission.f_GetStr(), Permission.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					{
						if (AuthenticationFactors.m_AuthenticationFactors.f_IsEmpty())
							return EQueryResult_HasPermission; // Matching permission and no authentication factor - no need to look further

						if (_AuthenticationCache.f_HasAuthenticatedPermission(_FullIdentity, AnyPermission, true, AuthenticationFactors.m_MaximumAuthenticationLifetime))
							return EQueryResult_HasPermission;

						NContainer::TCSet<NStr::CStr> Preauthenticated;
						if (AuthenticationFactors.m_MaximumAuthenticationLifetime != 0) // Cacheable?
						{
							Preauthenticated = _AuthenticationCache.f_HasAuthenticatedPermissionPattern(_FullIdentity, AnyPermission, AuthenticationFactors.m_MaximumAuthenticationLifetime);
							if (fFindAll(Preauthenticated, AuthenticationFactors.m_AuthenticationFactors))
								return EQueryResult_HasPermission;
						}
						o_RequestedPermissions.f_InsertLast
							(
								ICDistributedActorAuthenticationHandler::CPermissionWithRequirements
								{
									Permission
									, m_Description
									, AuthenticationFactors.m_AuthenticationFactors
									, AuthenticationFactors.m_MaximumAuthenticationLifetime
									, Preauthenticated
								}
							)
						;
						Result = EQueryResult_NeedAuthentication;
					}
				}
			}
		}
		else
		{
			for (auto const &AnyPermission : m_Permissions)
			{
				auto *pFactors = _Permissions.f_FindEqual(AnyPermission);
				if (pFactors)
				{
					if (pFactors->m_AuthenticationFactors.f_IsEmpty())
						return EQueryResult_HasPermission; // Matching permission and no authentication factor - no need to look further

					if (_AuthenticationCache.f_HasAuthenticatedPermission(_FullIdentity, AnyPermission, false, pFactors->m_MaximumAuthenticationLifetime))
						return EQueryResult_HasPermission;

					NContainer::TCSet<NStr::CStr> Preauthenticated;
					if (pFactors->m_MaximumAuthenticationLifetime != 0) // Cacheable?
					{
						Preauthenticated = _AuthenticationCache.f_HasAuthenticatedPermissionPattern(_FullIdentity, AnyPermission, pFactors->m_MaximumAuthenticationLifetime);
						if (fFindAll(Preauthenticated, pFactors->m_AuthenticationFactors))
							return EQueryResult_HasPermission;
					}

					auto &Permission = _Permissions.fs_GetKey(pFactors);
					o_RequestedPermissions.f_InsertLast
						(
							ICDistributedActorAuthenticationHandler::CPermissionWithRequirements
							{
								Permission
								, m_Description
								, pFactors->m_AuthenticationFactors
								, pFactors->m_MaximumAuthenticationLifetime
								, Preauthenticated
							}
						)
					;
					Result = EQueryResult_NeedAuthentication;
				}
			}
		}
		return Result;
	}

	bool CPermissionQuery::f_MatchesAuthentication
		(
			NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
			, NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> const &_AuthenticatedPermissions
			, NContainer::TCMap<NStr::CStr, int64> &o_AuthenticatedPermissions
		) const
	{
		auto fMatchesRequirements = [&](CPermissionRequirements const &_Requirements, NStr::CStr const &_Permission) -> bool
			{
				// Check all permissions in the query and verify the all required authentication factors succeeded
				auto *pAuthenticatedFactors = _AuthenticatedPermissions.f_FindEqual(_Permission);
				if (!pAuthenticatedFactors)
					return false;

				for (auto const &NeededFactors : _Requirements.m_AuthenticationFactors)
				{
					bool bFoundAll = true;
					for (auto const &NeededFactor : NeededFactors)
					{
						if (!pAuthenticatedFactors->f_FindEqual(NeededFactor))
						{
							bFoundAll = false;
							break;
						}
					}

					if (bFoundAll)
					{
						o_AuthenticatedPermissions[_Permission] = _Requirements.m_MaximumAuthenticationLifetime;
						return true;
					}
				}
				return false;
			}
		;

		// This function basically does the same job as the one above, but instead of adding authentication factors that are needed we check if the authentication has succeeded
		if (m_bIsWildcard)
		{
			for (auto &Requirements : _Permissions)
			{
				auto &Permission = _Permissions.fs_GetKey(Requirements);
				for (auto const &AnyPermission : m_Permissions)
				{
					if (NStr::fg_StrMatchWildcard(AnyPermission.f_GetStr(), Permission.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					{
						if (Requirements.m_AuthenticationFactors.f_IsEmpty())
							return true; // Matching permission and no authentication factor - should not have been called for this case, already known to be true

						if (fMatchesRequirements(Requirements, Permission))
							return true;
					}
				}
			}
		}
		else
		{
			for (auto const &AnyPermission : m_Permissions)
			{
				auto *pRequirements = _Permissions.f_FindEqual(AnyPermission);
				if (!pRequirements)
					continue;

				if (pRequirements->m_AuthenticationFactors.f_IsEmpty())
					return true; // Matching permission and no authentication factor - should not have been called for this case, already known to be true

				if (fMatchesRequirements(*pRequirements, AnyPermission))
					return true;
			}
		}
		return false;
	}

	CPermissionQuery::EQueryResult CTrustedPermissionSubscription::fp_HasPermissionForQuery
		(
			CPermissionQuery const &_Permission
			, CCallingHostInfo const &_CallingHostInfo
			, NContainer::TCVector<CPermissionIdentifiers> const &_IdentityPowerSet
			, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
		) const
	{
		using CHandler = ICDistributedActorAuthenticationHandler;

		// Here we check if the host/user combo has { perm1 || perm2 } && { perm3 || perm4 }

		CPermissionQuery::EQueryResult Result = CPermissionQuery::EQueryResult_NeedAuthentication;
		bool bFoundMatchingPermission = false;

		CPermissionIdentifiers FullIdentity(_CallingHostInfo.f_GetRealHostID(), _CallingHostInfo.f_GetClaimedUserID());

		NContainer::TCVector<CHandler::CPermissionWithRequirements> RequestedPermissions;
		for (auto const &Identity : _IdentityPowerSet)
		{
			if (auto pPermissions = mp_Permissions.f_FindEqual(Identity))
			{
				switch (_Permission.f_MatchesPermission(*pPermissions, RequestedPermissions, Identity, FullIdentity, mp_AuthenticationCache))
				{
				case CPermissionQuery::EQueryResult_NoPermission:
					// Couldn't find a matching permission, but we have to check all the host/user combos in the power set before we know we will not find a matching permission
					break;

				case CPermissionQuery::EQueryResult_HasPermission:
					return CPermissionQuery::EQueryResult_HasPermission;

				case CPermissionQuery::EQueryResult_NeedAuthentication:
					bFoundMatchingPermission = true;
					break;
				}
			}
		}

		if (!bFoundMatchingPermission)
			return CPermissionQuery::EQueryResult_NoPermission;

		if (Result == CPermissionQuery::EQueryResult_NeedAuthentication)
			o_RequestedPermissions.f_InsertLast(fg_Move(RequestedPermissions));

		return Result;
	}

	CPermissionQuery::EQueryResult CTrustedPermissionSubscription::fp_HasPermissionForQuery
		(
			CPermissionQuery const &_Permission
			, CCallingHostInfo const &_CallingHostInfo
			, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
		) const
	{
		auto PowerSet = CPermissionIdentifiers::fs_GetPowerSet(_CallingHostInfo.f_GetRealHostID(), _CallingHostInfo.f_GetClaimedUserID());
		return fp_HasPermissionForQuery(_Permission, _CallingHostInfo, PowerSet, o_RequestedPermissions);
	}

	CPermissionQuery::EQueryResult CTrustedPermissionSubscription::fp_HasPermissionForQuery
		(
			NContainer::TCVector<CPermissionQuery> const &_Permissions
			, CCallingHostInfo const &_CallingHostInfo
			, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
		) const
	{
		using CHandler = ICDistributedActorAuthenticationHandler;
		// Here we check if the host/user combo has { perm1 || perm2 } && { perm3 || perm4 }
		CPermissionQuery::EQueryResult Result = CPermissionQuery::EQueryResult_HasPermission;
		auto PowerSet = CPermissionIdentifiers::fs_GetPowerSet(_CallingHostInfo.f_GetRealHostID(), _CallingHostInfo.f_GetClaimedUserID());
		NContainer::TCVector<NContainer::TCVector<CHandler::CPermissionWithRequirements>> CollectedRequestedPermissions;

		for (auto const &Permission : _Permissions)
		{
			switch (fp_HasPermissionForQuery(Permission, _CallingHostInfo, PowerSet, CollectedRequestedPermissions))
			{
			case CPermissionQuery::EQueryResult_HasPermission:
				break;
			case CPermissionQuery::EQueryResult_NoPermission:
				return CPermissionQuery::EQueryResult_NoPermission;
			case CPermissionQuery::EQueryResult_NeedAuthentication:
				Result = CPermissionQuery::EQueryResult_NeedAuthentication;
				break;
			}
		}

		if (Result == CPermissionQuery::EQueryResult_NeedAuthentication)
			o_RequestedPermissions.f_InsertLast(fg_Move(CollectedRequestedPermissions));
		return Result;
	}

	TCUnsafeFuture<bool> CTrustedPermissionSubscription::fp_AuthenticatePermissions
		(
			NContainer::TCVector<CPermissionQuery> _PermissionsQueries
			, ICDistributedActorAuthenticationHandler::CRequest _Request
			, CCallingHostInfo _CallingHostInfo
		) const
	{
		using CHandler = ICDistributedActorAuthenticationHandler;

		auto AuthenticationHandler = _CallingHostInfo.f_GetAuthenticationHandler();
		if (!AuthenticationHandler)
			co_return false;

		auto pState = mp_pState;

		auto TrustManager = pState->m_TrustManager.f_Lock();
		if (!TrustManager)
			co_return DMibErrorInstance("No trust manager");

		auto const &UserID = _CallingHostInfo.f_GetClaimedUserID();
		auto const &HostID = _CallingHostInfo.f_GetRealHostID();

		DMibCheck(HostID);
		if (!UserID)
			co_return DMibErrorInstance("Cannot authenticate without user");

		CPermissionIdentifiers FullIdentity(HostID, UserID);

		auto CacheTime = NTime::CTime::fs_NowUTC();

		auto Challenge = CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(UserID);
		auto ResponsesWrapped = co_await AuthenticationHandler.f_CallActor(&CHandler::f_RequestAuthentication)(_Request, Challenge, "").f_Wrap();

		if (!ResponsesWrapped)
		{
			DMibLogWithCategory(Mib/Concurrency/Trust, Error, "Failed to authenticate command: {}", ResponsesWrapped.f_GetExceptionStr());
			co_return ResponsesWrapped.f_GetException();
		}

		if (!pState->m_pSubscription)
			co_return DMibErrorInstance("Permission subscription destroyed");

		auto &Responses = *ResponsesWrapped;
		auto VerificationResults = co_await TrustManager(&CDistributedActorTrustManager::f_VerifyAuthenticationResponses, Challenge, _Request, Responses);

		if (!pState->m_pSubscription)
			co_return DMibErrorInstance("Permission subscription destroyed");

		NTime::CTime ExpirationTime;
		DMibCheck(VerificationResults.f_GetLen() == Responses.f_GetLen());

		// Collect all permissions that have been authenticated to avoid linear lookup in the inner most loop.
		NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> Succeeded;

		{
			auto iResponse = Responses.f_GetIterator();
			auto iVerified = VerificationResults.f_GetIterator();
			for (; iResponse && iVerified; ++iResponse, ++iVerified)
			{
				if (!*iVerified)
					continue;

				auto &Response = *iResponse;
				if (ExpirationTime.f_IsValid())
					ExpirationTime = fg_Min(ExpirationTime, Response.m_SignedProperties.m_ExpirationTime);
				else
					ExpirationTime = Response.m_SignedProperties.m_ExpirationTime;

				for (auto const &Permission : Response.m_SignedProperties.m_Permissions)
					Succeeded[Permission][Response.m_FactorName];
			}
		}

		// Also add the preauthenticated factors that we didn't ask for - this is simplifies the matching
		//
		// If the preauthenticated pattern expired while we were collecting needed permissions for the command and we have some instances of a permission
		// that uses the preathenticated pattern and some that do not, this operation could miss a failed authentication.
		for (auto const &PermissionGroup : _Request.m_RequestedPermissions)
		{
			for (auto const &PermissionWithRequirements : PermissionGroup)
			{
				for (auto const &Preauthenticated : PermissionWithRequirements.m_Preauthenticated)
					Succeeded[PermissionWithRequirements.m_Permission][Preauthenticated];
			}
		}

		auto IdentitiesPowerSet = CPermissionIdentifiers::fs_GetPowerSet(HostID, UserID);
		// Must have authentication for each named permission query

		bool bAllAuthenticated = true;
		for (auto const &AllPermissions : _PermissionsQueries)
		{
			bool bFoundOne = false;
			for (auto const &Identity : IdentitiesPowerSet)
			{
				auto *pPermissions = mp_Permissions.f_FindEqual(Identity);
				NContainer::TCMap<NStr::CStr, int64> AuthenticatedPermissions;
				if (pPermissions && AllPermissions.f_MatchesAuthentication(*pPermissions, Succeeded, AuthenticatedPermissions))
				{
					bFoundOne = true;
					TrustManager.f_Bind<&CDistributedActorTrustManager::f_UpdateAuthenticationCache>
						(
							FullIdentity
							, fg_Move(AuthenticatedPermissions)
							, ExpirationTime
							, CacheTime
						)
						.f_DiscardResult()
					;
					break;
				}
			}
			if (!bFoundOne)
			{
				bAllAuthenticated = false;
				break;
			}
		}

		co_return bAllAuthenticated;
	}

	TCUnsafeFuture<bool> CTrustedPermissionSubscription::f_HasPermission
		(
			NStr::CStr _Description
			, NContainer::TCVector<NStr::CStr> _Permissions
			, CCallingHostInfo _CallingHostInfo
		) const
	{
		DMibRequire(_CallingHostInfo.f_GetRealHostID());

		using CHandler = ICDistributedActorAuthenticationHandler;
		// This is slightly more complicated than the case above - check if the user has permission for all permissions in _Permissions
		// Here we check if the host/user combo has { perm1 || perm2 } && { perm3 || perm4 }
		CHandler::CRequest Request;
		Request.m_Description = _Description;

		CPermissionQuery Query;
		Query.m_Permissions = _Permissions;
		switch (fp_HasPermissionForQuery(Query, _CallingHostInfo, Request.m_RequestedPermissions))
		{
		case CPermissionQuery::EQueryResult_NoPermission:
			co_return false;
		case CPermissionQuery::EQueryResult_HasPermission:
			co_return true;
		case CPermissionQuery::EQueryResult_NeedAuthentication:
			break;
		}

		co_return co_await fp_AuthenticatePermissions({fg_Move(Query)}, fg_Move(Request), _CallingHostInfo);
	}

	TCUnsafeFuture<bool> CTrustedPermissionSubscription::f_HasPermissions
		(
			NStr::CStr _Description
			, NContainer::TCVector<CPermissionQuery> _PermissionsQueries
			, CCallingHostInfo _CallingHostInfo
		) const
	{
		DMibRequire(_CallingHostInfo.f_GetRealHostID());

		using CHandler = ICDistributedActorAuthenticationHandler;
		// This is slightly more complicated than the case above - check if the user has permission for all permissions in _Permissions
		// Here we check if the host/user combo has { perm1 || perm2 } && { perm3 || perm4 }
		CHandler::CRequest Request;
		Request.m_Description = _Description;

		switch (fp_HasPermissionForQuery(_PermissionsQueries, _CallingHostInfo, Request.m_RequestedPermissions))
		{
		case CPermissionQuery::EQueryResult_NoPermission:
			co_return false;
		case CPermissionQuery::EQueryResult_HasPermission:
			co_return true;
		case CPermissionQuery::EQueryResult_NeedAuthentication:
			break;
		}

		co_return co_await fp_AuthenticatePermissions(_PermissionsQueries, fg_Move(Request), _CallingHostInfo);
	}

	TCUnsafeFuture<NContainer::TCMap<NStr::CStr, bool>> CTrustedPermissionSubscription::f_HasPermissions
		(
			NStr::CStr _Description
			, NContainer::TCMap<NStr::CStr, NContainer::TCVector<CPermissionQuery>> _NamedPermissionQueries
			, CCallingHostInfo _CallingHostInfo
		) const
	{
		DMibRequire(_CallingHostInfo.f_GetRealHostID());

		using CHandler = ICDistributedActorAuthenticationHandler;
		// This is the messy case.
		// For each named query we check if the host/user combo has { perm1 || perm2 } && { perm3 || perm4 }
		NContainer::TCMap<NStr::CStr, zbool> Results;
		for (auto const &PermissionQuery : _NamedPermissionQueries)
			Results[_NamedPermissionQueries.fs_GetKey(PermissionQuery)] = false;

		CHandler::CRequest Request;
		Request.m_Description = _Description;

		for (auto const &PermissionQueries : _NamedPermissionQueries)
		{
			switch (fp_HasPermissionForQuery(PermissionQueries, _CallingHostInfo, Request.m_RequestedPermissions))
			{
			case CPermissionQuery::EQueryResult_HasPermission:
				Results[_NamedPermissionQueries.fs_GetKey(PermissionQueries)] = true;
				break;
			case CPermissionQuery::EQueryResult_NoPermission:
			case CPermissionQuery::EQueryResult_NeedAuthentication:
				break;
			}
		}

		if (Request.m_RequestedPermissions.f_IsEmpty())
			co_return fg_Move(Results);

		auto AuthenticationHandler = _CallingHostInfo.f_GetAuthenticationHandler();
		if (!AuthenticationHandler)
			co_return {};

		auto pState = mp_pState;

		auto TrustManager = pState->m_TrustManager.f_Lock();
		if (!TrustManager)
			co_return DMibErrorInstance("No trust manager");

		auto HostID = _CallingHostInfo.f_GetRealHostID();
		auto UserID = _CallingHostInfo.f_GetClaimedUserID();

		if (!UserID)
			co_return DMibErrorInstance("Cannot authenticate without user");

		CPermissionIdentifiers FullIdentity(HostID, UserID);

		CHandler::CChallenge Challenge = CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(UserID);
		auto CacheTime = NTime::CTime::fs_NowUTC();

		auto NamedPermissionQueries = _NamedPermissionQueries;

		auto ResponsesResult = co_await AuthenticationHandler.f_CallActor(&CHandler::f_RequestAuthentication)(Request, Challenge, "").f_Wrap();

		if (!ResponsesResult)
		{
			DMibLogWithCategory(Mib/Concurrency/Trust, Error, "Failed to authenticate command: {}", ResponsesResult.f_GetExceptionStr());
			co_return ResponsesResult.f_GetException();
		}

		if (!pState->m_pSubscription)
			co_return DMibErrorInstance("Permission subscription destroyed");

		auto &Responses = *ResponsesResult;

		auto VerificationResults = co_await TrustManager(&CDistributedActorTrustManager::f_VerifyAuthenticationResponses, Challenge, Request, Responses);

		if (!pState->m_pSubscription)
			co_return DMibErrorInstance("Permission subscription destroyed");

		NTime::CTime ExpirationTime;

		DMibCheck(VerificationResults.f_GetLen() == Responses.f_GetLen());

		// Collect all permissions that have been authenticated to avoid linear lookup in the inner most loop.
		NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> Succeeded;

		{
			auto iResponse = Responses.f_GetIterator();
			auto iVerified = VerificationResults.f_GetIterator();
			for (; iResponse && iVerified; ++iResponse, ++iVerified)
			{
				if (!*iVerified)
					continue;

				auto &Response = *iResponse;
				if (ExpirationTime.f_IsValid())
					ExpirationTime = fg_Min(ExpirationTime, Response.m_SignedProperties.m_ExpirationTime);
				else
					ExpirationTime = Response.m_SignedProperties.m_ExpirationTime;

				for (auto const &Permission : Response.m_SignedProperties.m_Permissions)
					Succeeded[Permission][Response.m_FactorName];
			}
		}

		// Also add the preauthenticated factors that we didn't ask for - this is simplifies the matching
		//
		// If the preauthenticated pattern expired while we were collecting needed permissions for the command and we have some instances of a permission
		// that uses the preathenticated pattern and some that do not, this operation could miss a failed authentication.
		for (auto const &PermissionGroup : Request.m_RequestedPermissions)
		{
			for (auto const &PermissionWithRequirements : PermissionGroup)
			{
				for (auto const &Preauthenticated : PermissionWithRequirements.m_Preauthenticated)
					Succeeded[PermissionWithRequirements.m_Permission][Preauthenticated];
			}
		}

		auto IdentitiesPowerSet = CPermissionIdentifiers::fs_GetPowerSet(HostID, UserID);
		// Must have authentication for each named permission query
		for (auto const &PermissionQueries : NamedPermissionQueries)
		{
			// If already true, we found permission(s) that did not need authentication
			auto const &Name = NamedPermissionQueries.fs_GetKey(PermissionQueries);

			auto &bResult = Results[Name];
			if (bResult)
				continue;

			bool bAllAuthenticated = true;
			for (auto const &AllPermissions : PermissionQueries)
			{
				bool bFoundOne = false;
				for (auto const &Identity : IdentitiesPowerSet)
				{
					auto pPermissions = mp_Permissions.f_FindEqual(Identity);
					NContainer::TCMap<NStr::CStr, int64> AuthenticatedPermissions;
					if (pPermissions && AllPermissions.f_MatchesAuthentication(*pPermissions, Succeeded, AuthenticatedPermissions))
					{
						bFoundOne = true;
						TrustManager.f_Bind<&CDistributedActorTrustManager::f_UpdateAuthenticationCache>
							(
								FullIdentity
								, fg_Move(AuthenticatedPermissions)
								, ExpirationTime
								, CacheTime
							)
							.f_DiscardResult()
						;
						break;
					}
				}
				if (!bFoundOne)
				{
					bAllAuthenticated = false;
					break;
				}
			}
			if (bAllAuthenticated)
				bResult = true;
		}

		co_return fg_Move(Results);
	}

	NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> const &CTrustedPermissionSubscription::f_GetPermissions() const
	{
		return mp_Permissions;
	}

	void CTrustedPermissionSubscription::f_OnPermissionsAdded
		(
			TCActorFunctorWeak<TCFuture<void>(CPermissionIdentifiers _Identity, NContainer::TCMap<NStr::CStr, CPermissionRequirements> _PermissionsAdded)> &&_fOnPermissionsAdded
		)
	{
		mp_pState->m_fOnPermissionsAdded = fg_Move(_fOnPermissionsAdded);
	}

	void CTrustedPermissionSubscription::f_OnPermissionsRemoved
		(
			TCActorFunctorWeak<TCFuture<void> (CPermissionIdentifiers _Identity, NContainer::TCSet<NStr::CStr> _PermissionsRemoved)> &&_fOnPermissionsRemoved
		)
	{
		mp_pState->m_fOnPermissionsRemoved = fg_Move(_fOnPermissionsRemoved);
	}

	NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> CDistributedActorTrustManager::fp_SubscribeToPermissions
		(
			NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
		)
	{
		auto &Internal = *mp_pInternal;
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> ReturnPermissions;
		for (auto &Wildcard : _pState->m_Wildcards)
		{
			auto SubscriptionMapped = Internal.m_PermissionsSubscriptions(Wildcard);
			if (SubscriptionMapped.f_WasCreated())
			{
				auto &OutPermissions = (*SubscriptionMapped).m_PermissionsPerIdentity;
				for (auto &Permissions : Internal.m_Permissions)
				{
					auto &Identity = Permissions.f_GetIdentity();
					for (auto &AuthenticationFactors : Permissions.m_Permissions.m_Permissions)
					{
						auto const &PermissionKey = Permissions.m_Permissions.m_Permissions.fs_GetKey(AuthenticationFactors);
						if (NStr::fg_StrMatchWildcard(PermissionKey.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							continue;
						OutPermissions[Identity][PermissionKey] = AuthenticationFactors;
					}
				}
			}
			auto &Subscription = *SubscriptionMapped;
			Subscription.m_Subscriptions[_pState];
			for (auto &Permissions : Subscription.m_PermissionsPerIdentity)
			{
				auto &Identity = Subscription.m_PermissionsPerIdentity.fs_GetKey(Permissions);
				ReturnPermissions[Identity] += Permissions;
			}
		}

		return fg_Move(ReturnPermissions);
	}

	CDistributedActorTrustManagerAuthenticationCache CDistributedActorTrustManager::fp_FilterCachedAuthentications
		(
			NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
		)
	{
		auto Now = NTime::CTime::fs_NowUTC();
		auto &Internal = *mp_pInternal;
		CDistributedActorTrustManagerAuthenticationCache ReturnCache;
		auto &AuthenticatedPermissions = Internal.m_AuthenticationCache.mp_AuthenticatedPermissions;
		for (auto &Permissions : AuthenticatedPermissions)
		{
			NContainer::TCSet<NStr::CStr> ExpiredPermissions;
			for (auto const &CacheLifetimes : Permissions)
			{
				if (CacheLifetimes.m_ExpirationTime < Now)
					continue;

				auto const &Permission = Permissions.fs_GetKey(CacheLifetimes);
				for (auto &Wildcard : _pState->m_Wildcards)
				{
					if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						continue;

					ReturnCache.mp_AuthenticatedPermissions[AuthenticatedPermissions.fs_GetKey(Permissions)][Permission] = CacheLifetimes;
					break;
				}
			}
		}

		auto &Preauthenticated = Internal.m_AuthenticationCache.mp_PreauthenticatedPatterns;
		for (auto &PermissionPatterns : Preauthenticated)
		{
			NContainer::TCSet<NStr::CStr> EmptyPatterns;
			for (auto &Factors : PermissionPatterns)
			{
				NContainer::TCSet<NStr::CStr> ExpiredFactors;
				auto const &Pattern = PermissionPatterns.fs_GetKey(Factors);
				for (auto &Wildcard : _pState->m_Wildcards)
				{
					// Note that the subscription wildcard is on the left side in the matching
					if (NStr::fg_StrMatchWildcard(Wildcard.f_GetStr(), Pattern.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						continue;
				}

				NContainer::TCSet<NStr::CStr> ExpiredPermissions;
				for (auto &CacheLifetimes : Factors)
				{
					if (CacheLifetimes.m_ExpirationTime >= Now)
						ReturnCache.mp_PreauthenticatedPatterns[Preauthenticated.fs_GetKey(PermissionPatterns)][Pattern][Factors.fs_GetKey(CacheLifetimes)] = CacheLifetimes;
				}
			}
		}

		return fg_Move(ReturnCache);
	}

	void CDistributedActorTrustManager::fp_UnsubscribeToPermissions(NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState)
	{
		_pState->m_DispatchActor.f_Clear();

		auto &Internal = *mp_pInternal;

		for (auto &Wildcard : _pState->m_Wildcards)
		{
			auto pSubscriptionState = Internal.m_PermissionsSubscriptions.f_FindEqual(Wildcard);
			if (!pSubscriptionState)
				continue;
			auto &Subscription = *pSubscriptionState;
			Subscription.m_Subscriptions.f_Remove(_pState);
			if (Subscription.m_Subscriptions.f_IsEmpty())
				Internal.m_PermissionsSubscriptions.f_Remove(pSubscriptionState);
		}
	}
}
