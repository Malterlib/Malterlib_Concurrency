// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency
{
	auto CDistributedActorTrustManager::f_EnumHostPermissions(bool _bIncludeHostInfo) -> TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>>  
	{
		auto &Internal = *mp_pInternal;
		NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> Return;
		NContainer::TCSet<NStr::CStr> HostIDs;
		for (auto &Permissions : Internal.m_HostPermissions)
		{
			if (Permissions.m_HostPermissions.m_Permissions.f_IsEmpty())
				continue;
			auto &HostID = Permissions.f_GetHostID(); 
			HostIDs[HostID];
			for (auto &Permission : Permissions.m_HostPermissions.m_Permissions)
			{
				auto &OutPermissions = Return[Permission];
				OutPermissions[HostID];
			}
		}
		for (auto &Permission : Internal.m_RegisteredPermissions)
			Return[Permission];
		
		if (!_bIncludeHostInfo)
			return fg_Explicit(fg_Move(Return));
			
		TCActorResultMap<NStr::CStr, CHostInfo> HostInfos;
		
		auto ThisActor = fg_ThisActor(this);
		for (auto &HostID : HostIDs)
			ThisActor(&CDistributedActorTrustManager::fp_GetHostInfo, HostID) > HostInfos.f_AddResult(HostID);

		TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>> Continuation;
			
		HostInfos.f_GetResults() 
			> Continuation / [this, Continuation, Return = fg_Move(Return)] 
			(NContainer::TCMap<NStr::CStr, TCAsyncResult<CHostInfo>> &&_HostInfos) mutable
			{
				for (auto &Permissions : Return)
				{
					for (auto &Host : Permissions)
					{
						auto &HostID = Permissions.fs_GetKey(Host);
						auto *pHostInfo = _HostInfos.f_FindEqual(HostID);
						if (pHostInfo && *pHostInfo)
							Host = (**pHostInfo);
					}
				}
				Continuation.f_SetResult(fg_Move(Return));
			}
		;
		return Continuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_AddHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions)
	{
		auto &Internal = *mp_pInternal;
		auto &HostPermissions = Internal.m_HostPermissions[_HostID];

		NContainer::TCSet<NStr::CStr> PermissionsAdded;
		for (auto &Permission : _Permissions)
		{
			if (HostPermissions.m_HostPermissions.m_Permissions(Permission).f_WasCreated())
				PermissionsAdded[Permission];
		}
		if (PermissionsAdded.f_IsEmpty())
			return fg_Explicit();
		
		for (auto &Subscription : Internal.m_HostPermissionsSubscriptions)
		{
			auto const &Wildcard = Internal.m_HostPermissionsSubscriptions.fs_GetKey(Subscription);
			NContainer::TCSet<NStr::CStr> FilteredPermissions;
			for (auto &Permission : PermissionsAdded)
			{
				if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;
				if (!Subscription.m_Permissions(Permission).f_WasCreated())
					continue;
				FilteredPermissions[Permission];
			}
			
			if (FilteredPermissions.f_IsEmpty())
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
				pSubscription->f_AddPermissions(_HostID, FilteredPermissions);
		}

		TCContinuation<void> Continuation;
		if (HostPermissions.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetHostPermissions, _HostID, HostPermissions.m_HostPermissions) > Continuation;
		else
		{
			HostPermissions.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddHostPermissions, _HostID, HostPermissions.m_HostPermissions) > Continuation;
		}
		
		return Continuation;			
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_RemoveHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions)
	{
		auto &Internal = *mp_pInternal;
		auto *pHostPermissions = Internal.m_HostPermissions.f_FindEqual(_HostID);
		if (!pHostPermissions)
			return fg_Explicit();
		auto &HostPermissions = *pHostPermissions;
		
		NContainer::TCSet<NStr::CStr> PermissionsRemoved;
		for (auto &Permission : _Permissions)
		{
			if (HostPermissions.m_HostPermissions.m_Permissions.f_Remove(Permission))
				PermissionsRemoved[Permission];
		}
		if (PermissionsRemoved.f_IsEmpty())
			return fg_Explicit();

		for (auto &Subscription : Internal.m_HostPermissionsSubscriptions)
		{
			auto const &Wildcard = Internal.m_HostPermissionsSubscriptions.fs_GetKey(Subscription);
			NContainer::TCSet<NStr::CStr> FilteredPermissions;
			for (auto &Permission : PermissionsRemoved)
			{
				if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					continue;
				if (!Subscription.m_Permissions.f_Remove(Permission))
					continue;
				FilteredPermissions[Permission];
			}
			
			if (FilteredPermissions.f_IsEmpty())
				continue;

			for (auto &pSubscription : Subscription.m_Subscriptions)
				pSubscription->f_RemovePermissions(_HostID, FilteredPermissions);
		}
		
		if (!HostPermissions.m_HostPermissions.m_Permissions.f_IsEmpty())
			return fg_Explicit();
		
		bool bExistsInDatabase = HostPermissions.m_bExistsInDatabase;
		HostPermissions.m_bExistsInDatabase = false;
		Internal.m_HostPermissions.f_Remove(_HostID);
		
		if (!bExistsInDatabase)
			return fg_Explicit();
		
		TCContinuation<void> Continuation;
		Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveHostPermissions, _HostID) > Continuation;
		return Continuation;			
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
		NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TrustManager = fg_ThisActor(this);
		State.m_Wildcards = _Wildcards;
		
		TCContinuation<CTrustedPermissionSubscription> Continuation;
		
		fg_ThisActor(this)(&CDistributedActorTrustManager::fp_SubscribeToPermissions, pState) 
			> Continuation / [pState, Continuation](NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> &&_Permissions)
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
		void CTrustedPermissionSubscriptionState::f_AddPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded)
		{
			fg_Dispatch
				(
					m_DispatchActor
					, [this, _HostID, _PermissionsAdded, pThis = fg_Explicit(this)]
					{
						if (!m_pSubscription)
							return;

						auto &Subscription = *m_pSubscription;
						auto &HostPermissions = Subscription.mp_Permissions[_HostID];
						NContainer::TCSet<NStr::CStr> Added; 
						for (auto &Permission : _PermissionsAdded)
						{
							if (HostPermissions(Permission).f_WasCreated())
								Added[Permission];
						}
						if (!Added.f_IsEmpty() && m_fOnPermissionsAdded)
							m_fOnPermissionsAdded(_HostID, Added);
					}
				)
				> fg_DiscardResult() 
			;
		}
		
		void CTrustedPermissionSubscriptionState::f_RemovePermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)
		{
			fg_Dispatch
				(
					m_DispatchActor
					, [this, _HostID, _PermissionsRemoved, pThis = fg_Explicit(this)]
					{
						if (!m_pSubscription)
							return;
						
						auto &Subscription = *m_pSubscription;
						auto *pHostPermissions = Subscription.mp_Permissions.f_FindEqual(_HostID);
						if (!pHostPermissions)
							return;
						auto &HostPermissions = *pHostPermissions;
						NContainer::TCSet<NStr::CStr> Removed; 
						for (auto &Permission : _PermissionsRemoved)
						{
							if (HostPermissions.f_Remove(Permission))
								Removed[Permission];
						}
						if (HostPermissions.f_IsEmpty())
							Subscription.mp_Permissions.f_Remove(_HostID);
						if (!Removed.f_IsEmpty() && m_fOnPermissionsRemoved)
							m_fOnPermissionsRemoved(_HostID, Removed);
					}
				)
				> fg_DiscardResult() 
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
		State.m_DispatchActor.f_Clear();
		State.m_fOnPermissionsAdded.f_Clear();
		State.m_fOnPermissionsRemoved.f_Clear();
		mp_pState = nullptr;
		mp_Permissions.f_Clear();
	}

	bool CTrustedPermissionSubscription::f_HostHasPermission(NStr::CStr const &_Host, NStr::CStr const &_Permission) const
	{
		auto pHost = mp_Permissions.f_FindEqual(_Host);
		if (!pHost)
			return false;
		return pHost->f_FindEqual(_Permission) != nullptr;
	}
	
	NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> const &CTrustedPermissionSubscription::f_GetPermissions() const
	{
		return mp_Permissions;
	}
	
	void CTrustedPermissionSubscription::f_OnPermissionsAdded
		(
			NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded)> &&_fOnPermissionsAdded
		)
	{
		mp_pState->m_fOnPermissionsAdded = fg_Move(_fOnPermissionsAdded);
	}
	
	void CTrustedPermissionSubscription::f_OnPermissionsRemoved
		(
			NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> &&_fOnPermissionsRemoved
		)
	{
		mp_pState->m_fOnPermissionsRemoved = fg_Move(_fOnPermissionsRemoved);
	}

	TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>>> CDistributedActorTrustManager::fp_SubscribeToPermissions
		(
			NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
		)
	{
		auto &Internal = *mp_pInternal;
		NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> Permissions;
		for (auto &Wildcard : _pState->m_Wildcards)
		{
			auto SubscriptionMapped = Internal.m_HostPermissionsSubscriptions(Wildcard);
			if (SubscriptionMapped.f_WasCreated())
			{
				auto &OutPermissions = (*SubscriptionMapped).m_Permissions; 
				for (auto &Permissions : Internal.m_HostPermissions)
				{
					auto &HostID = Permissions.f_GetHostID();
					for (auto &Permission : Permissions.m_HostPermissions.m_Permissions)
					{
						if (NStr::fg_StrMatchWildcard(Permission.f_GetStr(), Wildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							continue;
						OutPermissions[HostID][Permission];
					}
				}
			}
			auto &Subscription = *SubscriptionMapped;
			Subscription.m_Subscriptions[_pState];
			for (auto &Host : Subscription.m_Permissions)
			{
				auto &HostID = Subscription.m_Permissions.fs_GetKey(Host);
				Permissions[HostID] += Host;
			}
		}
		
		return fg_Explicit(fg_Move(Permissions));
	}
	
	void CDistributedActorTrustManager::fp_UnsubscribeToPermissions(NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState)
	{
		auto &Internal = *mp_pInternal;
		
		for (auto &Wildcard : _pState->m_Wildcards)
		{
			auto pSubscriptionState = Internal.m_HostPermissionsSubscriptions.f_FindEqual(Wildcard);
			if (!pSubscriptionState)
				continue;
			auto &Subscription = *pSubscriptionState;
			Subscription.m_Subscriptions.f_Remove(_pState);
			if (Subscription.m_Subscriptions.f_IsEmpty())
				Internal.m_HostPermissionsSubscriptions.f_Remove(pSubscriptionState);
		}
	}
}
