// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency
{
	auto CDistributedActorTrustManager::f_EnumPermissions(bool _bIncludeHostInfo) -> TCContinuation<CDistributedActorTrustManagerInterface::CEnumPermissionsResult>
	{
		auto &Internal = *mp_pInternal;
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
			return fg_Explicit(fg_Move(Return));

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
				}
			}
		}

		TCActorResultMap<NStr::CStr, CHostInfo> HostInfos;
		for (auto &HostID : HostIDs)
			fp_GetHostInfo(HostID) > HostInfos.f_AddResult(HostID);

		NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> UserInfos;
		for (auto &UserID : UserIDs)
		{
			if (auto *pUser = Internal.m_Users.f_FindEqual(UserID))
				UserInfos[UserID] = CDistributedActorTrustManagerInterface::CUserInfo{pUser->m_UserInfo.m_UserName, pUser->m_UserInfo.m_Metadata};
		}

		TCContinuation<CDistributedActorTrustManagerInterface::CEnumPermissionsResult> Continuation;
			
		HostInfos.f_GetResults()
			> Continuation / [Continuation, Return = fg_Move(Return), UserInfos = fg_Move(UserInfos)]
			(
				NContainer::TCMap<NStr::CStr, TCAsyncResult<CHostInfo>> &&_HostInfos
			) mutable
			{
				for (auto &Permissions : Return.m_Permissions)
				{
					for (auto &PermissionInfo : Permissions)
					{
						auto const &Identity = Permissions.fs_GetKey(PermissionInfo);
						auto &HostID = Identity.f_GetHostID();
						auto &UserID = Identity.f_GetUserID();
						auto *pHostInfo = _HostInfos.f_FindEqual(HostID);
						if (pHostInfo && *pHostInfo)
							PermissionInfo.m_HostInfo = (**pHostInfo);
						if (auto *pUserInfo = UserInfos.f_FindEqual(UserID))
							PermissionInfo.m_UserInfo = {pUserInfo->m_UserName, pUserInfo->m_Metadata};
					}
				}
				Continuation.f_SetResult(fg_Move(Return));
			}
		;
		return Continuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_AddPermissions
		(
			CPermissionIdentifiers const &_Identity
			, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
			, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		auto &UserID = _Identity.f_GetUserID();
		auto &HostID = _Identity.f_GetHostID();
		if (!HostID && !UserID)
			return DMibErrorInstance("Cannot omit both host id and user id");
		if (HostID && !CActorDistributionManager::fs_IsValidHostID(HostID))
			return DMibErrorInstance("Invalid host id");
		if (UserID && !CActorDistributionManager::fs_IsValidUserID(UserID))
			return DMibErrorInstance("Invalid user id");

		auto &Internal = *mp_pInternal;
		auto &Permissions = Internal.m_Permissions[_Identity];

		NContainer::TCMap<NStr::CStr, CPermissionRequirements> PermissionsAdded;
		for (auto &AuthenticationFactors : _Permissions)
		{
			auto const &PermissionKey = _Permissions.fs_GetKey(AuthenticationFactors);
			if (auto Mapped = Permissions.m_Permissions.m_Permissions(PermissionKey, AuthenticationFactors); Mapped.f_WasCreated())
				PermissionsAdded[PermissionKey] = AuthenticationFactors;
			else if (*Mapped != AuthenticationFactors)
			{
				*Mapped = AuthenticationFactors;
				PermissionsAdded[PermissionKey] = AuthenticationFactors;
			}
		}
		if (PermissionsAdded.f_IsEmpty())
			return fg_Explicit();

		TCActorResultVector<void> SubscriptionResults;

		for (auto &Subscription : Internal.m_PermissionsSubscriptions)
		{
			auto const &Wildcard = Internal.m_PermissionsSubscriptions.fs_GetKey(Subscription);
			NContainer::TCMap<NStr::CStr, CPermissionRequirements> FilteredPermissions;
			for (auto &AuthenticationFactors : PermissionsAdded)
			{
				auto const &PermissionKey = PermissionsAdded.fs_GetKey(AuthenticationFactors);
				if (NStr::fg_StrMatchWildcard(PermissionKey.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;
				if (!Subscription.m_PermissionsPerIdentity[_Identity](PermissionKey, AuthenticationFactors).f_WasCreated())
					continue;
				FilteredPermissions[PermissionKey] = AuthenticationFactors;
			}
			
			if (FilteredPermissions.f_IsEmpty())
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
				pSubscription->f_AddPermissions(_Identity, FilteredPermissions) > SubscriptionResults.f_AddResult();
		}

		TCContinuation<void> SaveDatabaseContinuation;
		if (Permissions.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetPermissions, _Identity, Permissions.m_Permissions) > SaveDatabaseContinuation;
		else
		{
			Permissions.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddPermissions, _Identity, Permissions.m_Permissions) > SaveDatabaseContinuation;
		};

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
		{
			TCContinuation<void> Continuation;
			SaveDatabaseContinuation.f_Dispatch() + SubscriptionResults.f_GetResults()
				> Continuation / [Continuation](CVoidTag, NContainer::TCVector<TCAsyncResult<void>> &&) // Intentionally ignore results
				{
					Continuation.f_SetResult();
				}
			;
			return Continuation;
		}

		SubscriptionResults.f_GetResults() > fg_DiscardResult();

		return SaveDatabaseContinuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_RemovePermissions
		(
		 	CPermissionIdentifiers const &_Identity
			 , NContainer::TCSet<NStr::CStr> const &_Permissions
			 , EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		auto &UserID = _Identity.f_GetUserID();
		auto &HostID = _Identity.f_GetHostID();
		if (!HostID && !UserID)
			return DMibErrorInstance("Cannot omit both host id and user id");
		if (HostID && !CActorDistributionManager::fs_IsValidHostID(HostID))
			return DMibErrorInstance("Invalid host id");
		if (UserID && !CActorDistributionManager::fs_IsValidUserID(UserID))
			return DMibErrorInstance("Invalid user id");

		auto &Internal = *mp_pInternal;
		auto *pPermissions = Internal.m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			return fg_Explicit();
		auto &Permissions = *pPermissions;
		
		NContainer::TCSet<NStr::CStr> PermissionsRemoved;
		for (auto &Permission : _Permissions)
		{
			if (Permissions.m_Permissions.m_Permissions.f_Remove(Permission))
				PermissionsRemoved[Permission];
		}
		if (PermissionsRemoved.f_IsEmpty())
			return fg_Explicit();

		TCActorResultVector<void> SubscriptionResults;

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
				pSubscription->f_RemovePermissions(_Identity, FilteredPermissions) > SubscriptionResults.f_AddResult();
		}
		
		TCContinuation<void> SaveDatabaseContinuation;

		if (Permissions.m_Permissions.m_Permissions.f_IsEmpty())
		{
			if (Permissions.m_bExistsInDatabase)
			{
				Permissions.m_bExistsInDatabase = false;
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemovePermissions, _Identity) > SaveDatabaseContinuation;
			}
			else
				SaveDatabaseContinuation.f_SetResult();
			Internal.m_Permissions.f_Remove(_Identity);
		}
		else
		{
			if (Permissions.m_bExistsInDatabase)
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetPermissions, _Identity, Permissions.m_Permissions) > SaveDatabaseContinuation;
			else
			{
				Permissions.m_bExistsInDatabase = true;
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddPermissions, _Identity, Permissions.m_Permissions) > SaveDatabaseContinuation;
			}
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
		{
			TCContinuation<void> Continuation;
			SaveDatabaseContinuation.f_Dispatch() + SubscriptionResults.f_GetResults()
				> Continuation / [Continuation](CVoidTag, NContainer::TCVector<TCAsyncResult<void>> &&) // Intentionally ignore results
				{
					Continuation.f_SetResult();
				}
			;
			return Continuation;
		}

		SubscriptionResults.f_GetResults() > fg_DiscardResult();

		return SaveDatabaseContinuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_RegisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions)
	{
		auto &Internal = *mp_pInternal;
		Internal.m_RegisteredPermissions += _Permissions;
		return fg_Explicit();
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions)
	{
		auto &Internal = *mp_pInternal;
		Internal.m_RegisteredPermissions -= _Permissions;
		return fg_Explicit();
	}

	TCContinuation<CTrustedPermissionSubscription> CDistributedActorTrustManager::f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> const &_Wildcards, TCActor<CActor> const &_Actor)
	{
		if (!_Actor)
			return DMibErrorInstance("Invalid destination actor");
		
		NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TrustManager = fg_ThisActor(this);
		State.m_Wildcards = _Wildcards;
		
		TCContinuation<CTrustedPermissionSubscription> Continuation;
		
		fg_ThisActor(this)(&CDistributedActorTrustManager::fp_SubscribeToPermissions, pState) 
			> Continuation / [pState, Continuation](NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> &&_Permissions)
			{
				CTrustedPermissionSubscription Result;
				Result.mp_pState = pState;
				pState->m_pSubscription = &Result;
				Result.mp_Permissions = fg_Move(_Permissions);
				
				Continuation.f_SetResult(fg_Move(Result));
			}
		;
		return Continuation;
	}
	
	namespace NPrivate
 	{
		CTrustedPermissionSubscriptionState::CTrustedPermissionSubscriptionState()
		{
		}

		CTrustedPermissionSubscriptionState::~CTrustedPermissionSubscriptionState()
		{
		}

		TCDispatchedWeakActorCall<void> CTrustedPermissionSubscriptionState::f_AddPermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded
			)
		{
			return fg_Dispatch
				(
					m_DispatchActor
					, [this, _Identity, _PermissionsAdded, pThis = NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}]
					{
						if (!m_pSubscription)
							return;

						auto &Subscription = *m_pSubscription;
						auto &Permissions = Subscription.mp_Permissions[_Identity];
						NContainer::TCMap<NStr::CStr, CPermissionRequirements> Added;
						for (auto &Requirements : _PermissionsAdded)
						{
							auto &Permission = _PermissionsAdded.fs_GetKey(Requirements);
							if (Permissions(Permission, Requirements).f_WasCreated())
								Added[Permission] = Requirements;
						}
						if (!Added.f_IsEmpty() && m_fOnPermissionsAdded)
							m_fOnPermissionsAdded(_Identity, Added);
					}
				)
			;
		}
		
		TCDispatchedWeakActorCall<void> CTrustedPermissionSubscriptionState::f_RemovePermissions(CPermissionIdentifiers const &_Identity, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)
		{
			return fg_Dispatch
				(
					m_DispatchActor
					, [this, _Identity, _PermissionsRemoved, pThis = NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>{fg_Explicit(this)}]
					{
						if (!m_pSubscription)
							return;
						
						auto &Subscription = *m_pSubscription;
						auto *pPermissions = Subscription.mp_Permissions.f_FindEqual(_Identity);
						if (!pPermissions)
							return;
						auto &Permissions = *pPermissions;
						NContainer::TCSet<NStr::CStr> Removed; 
						for (auto &Permission : _PermissionsRemoved)
						{
							if (Permissions.f_Remove(Permission))
								Removed[Permission];
						}
						if (Permissions.f_IsEmpty())
							Subscription.mp_Permissions.f_Remove(_Identity);
						if (!Removed.f_IsEmpty() && m_fOnPermissionsRemoved)
							m_fOnPermissionsRemoved(_Identity, Removed);
					}
				)
			;
		}
	}
		
	CTrustedPermissionSubscription::CTrustedPermissionSubscription(CTrustedPermissionSubscription &&_Other)
		: mp_pState(fg_Move(_Other.mp_pState))
		, mp_Permissions(fg_Move(_Other.mp_Permissions))
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
			TrustManager(&CDistributedActorTrustManager::fp_UnsubscribeToPermissions, mp_pState) > fg_DiscardResult();
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
		) const
	{
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

						o_RequestedPermissions.f_InsertLast(ICDistributedActorAuthenticationHandler::CPermissionWithRequirements{Permission, AuthenticationFactors.m_AuthenticationFactors});
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

					auto &Permission = _Permissions.fs_GetKey(pFactors);
					o_RequestedPermissions.f_InsertLast(ICDistributedActorAuthenticationHandler::CPermissionWithRequirements{Permission, pFactors->m_AuthenticationFactors});
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
		) const
	{

		auto fMatchesRequirements = [&](CPermissionRequirements const &_Requirements, NStr::CStr const &_Permission) -> bool
			{
				auto *pAuthenticatedFactors = _AuthenticatedPermissions.f_FindEqual(_Permission);
				if (!pAuthenticatedFactors)
					return false;

				for (auto const &NeededFactors : _Requirements.m_AuthenticationFactors)
				{
					bool bFoundAll = true;
					for (auto const &NeededFactor : NeededFactors)
						bFoundAll &= !!pAuthenticatedFactors->f_FindEqual(NeededFactor);

					if (bFoundAll)
						return true;
				}
				return false;
			}
		;

		// This function basically does the same job as the one above, but instead of adding authentication factors that are needed, we check if the authenitcationhas succeeded
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

		NContainer::TCVector<CHandler::CPermissionWithRequirements> RequestedPermissions;
		for (auto const &Identity : _IdentityPowerSet)
		{
			if (auto pPermissions = mp_Permissions.f_FindEqual(Identity))
			{
				switch (_Permission.f_MatchesPermission(*pPermissions, RequestedPermissions))
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

	TCContinuation<bool> CTrustedPermissionSubscription::fp_AuthenticatePermissions
		(
			NContainer::TCVector<CPermissionQuery> const &_PermissionsQueries
			, ICDistributedActorAuthenticationHandler::CRequest const &_Request
			, CCallingHostInfo const &_CallingHostInfo
		) const
	{
		using CHandler = ICDistributedActorAuthenticationHandler;

		auto AuthenticationHandler = _CallingHostInfo.f_GetAuthenticationHandler();
		if (!AuthenticationHandler)
			return fg_Explicit(false);

		auto TrustManager = mp_pState->m_TrustManager.f_Lock();
		if (!TrustManager)
			return DMibErrorInstance("No trust manager");

		TCContinuation<bool> Continuation;
		auto const &UserID = _CallingHostInfo.f_GetClaimedUserID();
		auto const &HostID = _CallingHostInfo.f_GetRealHostID();

		auto Challenge = CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(UserID);
		DMibCallActor(AuthenticationHandler, CHandler::f_AuthenticateCommand, _Request, UserID, Challenge) > [=]
			(TCAsyncResult<NContainer::TCVector<CHandler::CResponse>> &&_Responses)
			{
				if (!_Responses)
				{
					DMibLogWithCategory(Mib/Concurrency/Trust, Error, "Failed to authenticate command: {}", _Responses.f_GetExceptionStr());
					Continuation.f_SetException(_Responses.f_GetException());
					return;
				}
				TrustManager(&CDistributedActorTrustManager::f_VerifyAuthenticationResponses, Challenge, _Request, *_Responses, UserID)
					> Continuation / [=](NContainer::TCVector<bool> &&_VerificationResults)
					{
						auto &Responses = *_Responses;

						DMibCheck(_VerificationResults.f_GetLen() == Responses.f_GetLen());

						// Collect all permissions that have been authenticated to avoid linear lookup in the inner most loop.
						NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> Succeeded;

						{
							auto iResponse = Responses.f_GetIterator();
							auto iVerified = _VerificationResults.f_GetIterator();
							for (; iResponse && iVerified; ++iResponse, ++iVerified)
							{
								if (!*iVerified)
									continue;

								auto &Response = *iResponse;

								for (auto const &Permission : Response.m_Permissions)
									Succeeded[Permission][Response.m_FactorName];
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
								if (auto pPermissions = mp_Permissions.f_FindEqual(Identity); pPermissions && AllPermissions.f_MatchesAuthentication(*pPermissions, Succeeded))
								{
									bFoundOne = true;
									break;
								}
							}
							if (!bFoundOne)
							{
								bAllAuthenticated = false;
								break;
							}
						}

						Continuation.f_SetResult(bAllAuthenticated);
					}
				;
			}
		;
		return Continuation;
	}

	TCContinuation<bool> CTrustedPermissionSubscription::f_HasPermission
		(
		 	NStr::CStr const &_Description
		 	, NContainer::TCVector<NStr::CStr> const &_Permissions
		 	, CCallingHostInfo const &_CallingHostInfo
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
			return fg_Explicit(false);

		case CPermissionQuery::EQueryResult_HasPermission:
			return fg_Explicit(true);
		}

		return fp_AuthenticatePermissions({Query}, Request, _CallingHostInfo);
	}

	TCContinuation<bool> CTrustedPermissionSubscription::f_HasPermissions
		(
		 	NStr::CStr const &_Description
		 	, NContainer::TCVector<CPermissionQuery> const &_PermissionsQueries
		 	, CCallingHostInfo const &_CallingHostInfo
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
			return fg_Explicit(false);

		case CPermissionQuery::EQueryResult_HasPermission:
			return fg_Explicit(true);
		}

		return fp_AuthenticatePermissions(_PermissionsQueries, Request, _CallingHostInfo);
	}

	TCContinuation<NContainer::TCMap<NStr::CStr, bool>> CTrustedPermissionSubscription::f_HasPermissions
		(
			NStr::CStr const &_Description
		 	, NContainer::TCMap<NStr::CStr, NContainer::TCVector<CPermissionQuery>> const &_NamedPermissionQueries
		 	, CCallingHostInfo const &_CallingHostInfo
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
			}
		}

		if (Request.m_RequestedPermissions.f_IsEmpty())
			return fg_Explicit(fg_Move(Results));

		auto AuthenticationHandler = _CallingHostInfo.f_GetAuthenticationHandler();
		if (!AuthenticationHandler)
			return fg_Explicit();

		auto TrustManager = mp_pState->m_TrustManager.f_Lock();
		if (!TrustManager)
			return DMibErrorInstance("No trust manager");

		auto const &HostID = _CallingHostInfo.f_GetRealHostID();
		auto const &UserID = _CallingHostInfo.f_GetClaimedUserID();
		CHandler::CChallenge Challenge = CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(UserID);

		TCContinuation<NContainer::TCMap<NStr::CStr, bool>> Continuation;
		DMibCallActor(AuthenticationHandler, CHandler::f_AuthenticateCommand, Request, UserID, Challenge)
			> [this, Continuation, Challenge, HostID, UserID, Results = fg_Move(Results), TrustManager = mp_pState->m_TrustManager.f_Lock(), _NamedPermissionQueries, Request]
			(TCAsyncResult<NContainer::TCVector<CHandler::CResponse>> &&_Responses) mutable
			{
				if (!_Responses)
				{
					DMibLogWithCategory(Mib/Concurrency/Trust, Error, "Failed to authenticate command: {}", _Responses.f_GetExceptionStr());
					Continuation.f_SetException(_Responses.f_GetException());
					return;
				}

				TrustManager(&CDistributedActorTrustManager::f_VerifyAuthenticationResponses, Challenge, Request, *_Responses, UserID)
					> Continuation / [this, Continuation, Results = fg_Move(Results), _NamedPermissionQueries, _Responses, HostID, UserID]
					(NContainer::TCVector<bool> &&_VerificationResults) mutable
					{
						auto &Responses = *_Responses;

						DMibCheck(_VerificationResults.f_GetLen() == Responses.f_GetLen());

						// Collect all permissions that have been authenticated to avoid linear lookup in the inner most loop.
						NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> Succeeded;

						{
							auto iResponse = Responses.f_GetIterator();
							auto iVerified = _VerificationResults.f_GetIterator();
							for (; iResponse && iVerified; ++iResponse, ++iVerified)
							{
								if (!*iVerified)
									continue;

								auto &Response = *iResponse;

								for (auto const &Permission : Response.m_Permissions)
									Succeeded[Permission][Response.m_FactorName];
							}
						}

						auto IdentitiesPowerSet = CPermissionIdentifiers::fs_GetPowerSet(HostID, UserID);
						// Must have authentication for each named permission query
						for (auto const &PermissionQueries : _NamedPermissionQueries)
						{
							// If already true, we found permission(s) that did not need authentication
							auto const &Name = _NamedPermissionQueries.fs_GetKey(PermissionQueries);

							auto &bResult = Results[Name];
							if (bResult)
								continue;

							bool bAllAuthenticated = true;
							for (auto const &AllPermissions : PermissionQueries)
							{
								bool bFoundOne = false;
								for (auto const &Identity : IdentitiesPowerSet)
								{
									if (auto pPermissions = mp_Permissions.f_FindEqual(Identity); pPermissions && AllPermissions.f_MatchesAuthentication(*pPermissions, Succeeded))
									{
										bFoundOne = true;
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

						Continuation.f_SetResult(fg_Move(Results));
					}
				;
			}
		;
		return Continuation;
	}

	NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> const &CTrustedPermissionSubscription::f_GetPermissions() const
	{
		return mp_Permissions;
	}
	
	void CTrustedPermissionSubscription::f_OnPermissionsAdded
		(
			NFunction::TCFunctionMovable
		 	<
		 		void (CPermissionIdentifiers const &_Identity, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded)
		 	>
		 	&&_fOnPermissionsAdded
		)
	{
		mp_pState->m_fOnPermissionsAdded = fg_Move(_fOnPermissionsAdded);
	}
	
	void CTrustedPermissionSubscription::f_OnPermissionsRemoved
		(
			NFunction::TCFunctionMovable<void (CPermissionIdentifiers const &_Identity, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> &&_fOnPermissionsRemoved
		)
	{
		mp_pState->m_fOnPermissionsRemoved = fg_Move(_fOnPermissionsRemoved);
	}

	TCContinuation<NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>>> CDistributedActorTrustManager::fp_SubscribeToPermissions
		(
			NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
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
		
		return fg_Explicit(fg_Move(ReturnPermissions));
	}
	
	void CDistributedActorTrustManager::fp_UnsubscribeToPermissions(NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState)
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
