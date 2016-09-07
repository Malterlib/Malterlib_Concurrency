// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency
{
	namespace NPrivate
 	{
		CTrustedActorSubscriptionState::CTrustedActorSubscriptionState()
		{
		}

		CTrustedActorSubscriptionState::~CTrustedActorSubscriptionState()
		{
		}
	}

	auto CDistributedActorTrustManager::f_EnumNamespacePermissions(bool _bIncludeHostInfo) -> TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>>
	{
		auto &Internal = *mp_pInternal;
		NContainer::TCMap<NStr::CStr, CNamespacePermissions> Return;
		NContainer::TCSet<NStr::CStr> HostIDs;
		for (auto &Namespace : Internal.m_Namespaces)
		{
			auto &Permissions = Return[Internal.m_Namespaces.fs_GetKey(Namespace)];
			for (auto &HostID : Namespace.m_Namespace.m_AllowedHosts)
			{
				HostIDs[HostID];
				Permissions.m_AllowedHosts[HostID];
			}
			for (auto &Type : Namespace.m_Types)
			{
				for (auto iHost = Type.m_HostToActors.f_GetIterator(); iHost; ++iHost)
				{
					auto &HostID = iHost.f_GetKey();
					if (!Namespace.m_Namespace.m_AllowedHosts.f_FindEqual(HostID))
					{
						HostIDs[HostID];
						Permissions.m_DisallowedHosts[HostID];
					}
				}
			}
		}
		if (!_bIncludeHostInfo)
			return fg_Explicit(fg_Move(Return));
			
		TCActorResultMap<NStr::CStr, CHostInfo> HostInfos;
		
		auto ThisActor = fg_ThisActor(this);
		for (auto &HostID : HostIDs)
			ThisActor(&CDistributedActorTrustManager::fp_GetHostInfo, HostID) > HostInfos.f_AddResult(HostID);

		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> Continuation;
			
		HostInfos.f_GetResults() 
			> Continuation / [this, Continuation, Return = fg_Move(Return)] 
			(NContainer::TCMap<NStr::CStr, TCAsyncResult<CHostInfo>> &&_HostInfos) mutable
			{
				for (auto &Permissions : Return)
				{
					for (auto &Host : Permissions.m_AllowedHosts)
					{
						auto &HostID = Permissions.m_AllowedHosts.fs_GetKey(Host);
						auto *pHostInfo = _HostInfos.f_FindEqual(HostID);
						if (pHostInfo && *pHostInfo)
							Host = (**pHostInfo);
					}
					for (auto &Host : Permissions.m_DisallowedHosts)
					{
						auto &HostID = Permissions.m_DisallowedHosts.fs_GetKey(Host);
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

	TCContinuation<void> CDistributedActorTrustManager::f_AllowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts)
	{
		auto &Internal = *mp_pInternal;
		auto &Namespace = Internal.m_Namespaces[_Namespace];

		NContainer::TCSet<NStr::CStr> HostsAdded;
		for (auto &Host : _Hosts)
		{
			if (Namespace.m_Namespace.m_AllowedHosts(Host).f_WasCreated())
				HostsAdded[Host];
		}
		if (HostsAdded.f_IsEmpty())
			return fg_Explicit();

		for (auto &Type : Namespace.m_Types)
		{
			for (auto &AddedHost : HostsAdded)
			{
				auto *pActors = Type.m_HostToActors.f_FindEqual(AddedHost);
				if (!pActors)
					continue;
				for (auto &Actor : *pActors)
					Type.m_AllowedActors[pActors->fs_GetKey(Actor)] = Actor;
				for (auto &pSubscription : Type.m_Subscriptions)
					pSubscription->f_AddDistributedActors(*pActors);
			}
		}
		
		TCContinuation<void> Continuation;
		if (Namespace.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetNamespace, _Namespace, Namespace.m_Namespace) > Continuation;
		else
		{
			Namespace.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, _Namespace, Namespace.m_Namespace) > Continuation;
		}
		
		return Continuation;			
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_DisallowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts)
	{
		auto &Internal = *mp_pInternal;
		auto *pNamespace = Internal.m_Namespaces.f_FindEqual(_Namespace);
		if (!pNamespace)
			return fg_Explicit();
		auto &Namespace = *pNamespace;
		
		NContainer::TCSet<NStr::CStr> HostsRemoved;
		for (auto &Host : _Hosts)
		{
			if (Namespace.m_Namespace.m_AllowedHosts.f_Remove(Host))
				HostsRemoved[Host];
		}
		if (HostsRemoved.f_IsEmpty())
			return fg_Explicit();
		
		for (auto &Type : Namespace.m_Types)
		{
			for (auto &RemovedHost : HostsRemoved)
			{
				auto *pActors = Type.m_HostToActors.f_FindEqual(RemovedHost);
				if (!pActors)
					continue;
				for (auto &Actor : *pActors)
					Type.m_AllowedActors.f_Remove(pActors->fs_GetKey(Actor));
				NContainer::TCSet<TCWeakDistributedActor<CActor>> WeakActors = *pActors; 
				for (auto &pSubscription : Type.m_Subscriptions)
					pSubscription->f_RemoveDistributedActors(WeakActors);
			}
		}
		
		if (!Namespace.m_Namespace.m_AllowedHosts.f_IsEmpty())
			return fg_Explicit();
		
		bool bExistsInDatabase = Namespace.m_bExistsInDatabase;
		Namespace.m_bExistsInDatabase = false;
		
		if (Namespace.m_nSubscriptions == 0)
			Internal.m_Namespaces.f_Remove(_Namespace);
		
		if (!bExistsInDatabase)
			return fg_Explicit();
		
		TCContinuation<void> Continuation;
		Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveNamespace, _Namespace) > Continuation;
		return Continuation;			
	}
	
	TCContinuation<NContainer::TCMap<TCDistributedActor<CActor>, CTrustedActorInfo>> CDistributedActorTrustManager::fp_SubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
	{
		auto &Internal = *mp_pInternal;
		auto &Namespace = Internal.m_Namespaces[_pState->m_NamespaceName];
		auto NamespaceName = _pState->m_NamespaceName;

		if (!Namespace.m_Subscription.f_IsEmpty())
		{
			auto &Type = Namespace.m_Types[_pState->m_TypeHash];
			Type.m_Subscriptions[_pState];
			++Namespace.m_nSubscriptions;
			return fg_Explicit(Type.m_AllowedActors);
		}
	
		TCContinuation<NContainer::TCMap<TCDistributedActor<CActor>, CTrustedActorInfo>> Continuation;
		auto fOnSubscribe = [this, _pState, NamespaceName, Continuation](TCAsyncResult<void> const &_Result, CInternal::CNamespaceState &_Namespace) -> bool
			{
				if (!_Result)
				{
					Continuation.f_SetException(_Result);
					return false;
				}
				auto &Type = _Namespace.m_Types[_pState->m_TypeHash];
				Type.m_Subscriptions[_pState];
				++_Namespace.m_nSubscriptions;
				Continuation.f_SetResult(Type.m_AllowedActors);
				return false;
			}
		;
		
		Namespace.m_OnSubscribe.f_Insert(fg_Move(fOnSubscribe));
		
		if (Namespace.m_bSubscribing)
			return Continuation;
		
		Namespace.m_bSubscribing = true;
		
		Internal.m_ActorDistributionManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, NContainer::fg_CreateVector(NamespaceName)
				, fg_ThisActor(this)
				, [this, NamespaceName](CAbstractDistributedActor &&_NewActor)
				{
					auto &Internal = *mp_pInternal;
					auto pNamespace = Internal.m_Namespaces.f_FindEqual(NamespaceName);
					if (!pNamespace)
						return;
					auto &Namespace = *pNamespace;
					
					NStr::CStr const &HostID = _NewActor.f_GetRealHostID();
					
					bool bAllowed = Namespace.m_Namespace.m_AllowedHosts.f_FindEqual(HostID);
					
					if (!bAllowed)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Trust
								, Warning
								, "Encountered disallowed actor in namespace '{}' from host '{}'"
								, NamespaceName
								, _NewActor.f_GetHostInfo().f_GetDesc() 
							)
						;
					}
					
					CTrustedActorInfo TrustInfo;
					TrustInfo.m_HostInfo = _NewActor.f_GetHostInfo();
					TrustInfo.m_UniqueHostID = _NewActor.f_GetUniqueHostID();
					
					for (auto TypeHash : _NewActor.f_GetTypeHashes())
					{
						auto &Type = Namespace.m_Types[TypeHash];
						auto Actor = _NewActor.f_GetActor(TypeHash);
						Type.m_HostToActors[HostID][Actor] = TrustInfo;
						Type.m_ActorToHost[Actor] = HostID;

						if (!bAllowed)
							continue;
						Type.m_AllowedActors[Actor] = TrustInfo;

						if (Type.m_Subscriptions.f_IsEmpty())
							continue;
						NContainer::TCMap<TCDistributedActor<CActor>, CTrustedActorInfo> Added;
						Added[Actor] = TrustInfo;
						for (auto &pSubscription : Type.m_Subscriptions)
							pSubscription->f_AddDistributedActors(Added);
					}
				}
				, [this, NamespaceName](TCWeakDistributedActor<CActor> const &_RemovedActor)
				{
					auto &Internal = *mp_pInternal;
					auto pNamespace = Internal.m_Namespaces.f_FindEqual(NamespaceName);
					if (!pNamespace)
						return;
					auto &Namespace = *pNamespace;
					
					NContainer::TCVector<uint32> TypesToRemove;
					for (auto &Type : Namespace.m_Types)
					{
						auto pHostID = Type.m_ActorToHost.f_FindEqual(_RemovedActor);
						if (!pHostID)
							continue;
						auto &HostID = *pHostID;
						bool bWasAllowed = Namespace.m_Namespace.m_AllowedHosts.f_FindEqual(HostID);
						auto &Actors = Type.m_HostToActors[HostID];
						bool bRemoved = Actors.f_Remove(_RemovedActor);
						(void)bRemoved;
						DMibCheck(bRemoved);
						
						if (Actors.f_IsEmpty())
							Type.m_HostToActors.f_Remove(HostID);
						Type.m_ActorToHost.f_Remove(pHostID);
						
						if (Type.m_ActorToHost.f_IsEmpty() && Type.m_Subscriptions.f_IsEmpty())
						{
							DMibCheck(Type.m_HostToActors.f_IsEmpty());
							TypesToRemove.f_Insert(pNamespace->m_Types.fs_GetKey(Type));
							continue;
						}
						
						if (!bWasAllowed)
							continue;
						bRemoved = Type.m_AllowedActors.f_Remove(_RemovedActor);
						DMibCheck(bRemoved);

						if (Type.m_Subscriptions.f_IsEmpty())
							continue;
						NContainer::TCSet<TCWeakDistributedActor<CActor>> Removed;
						Removed[_RemovedActor];
						for (auto &pSubscription : Type.m_Subscriptions)
							pSubscription->f_RemoveDistributedActors(Removed);
					}
					for (auto &TypeToRemove : TypesToRemove)
						pNamespace->m_Types.f_Remove(TypeToRemove);
				}
			)
			> [this, NamespaceName](TCAsyncResult<CActorSubscription> &&_Subscription)
			{
				auto &Internal = *mp_pInternal;
				auto &Namespace = Internal.m_Namespaces[NamespaceName];
				Namespace.m_bSubscribing = false;
				TCAsyncResult<void> Result;
				if (!_Subscription)
					Result.f_SetException(_Subscription);
				else
				{
					Namespace.m_Subscription = fg_Move(*_Subscription);
					Result.f_SetResult();
				}
				
				for (auto &fOnSubscribe : Namespace.m_OnSubscribe)
				{
					if (fOnSubscribe(Result, Namespace))
						return; // Deleted
				}
				Namespace.m_OnSubscribe.f_Clear();
				
				if (!_Subscription)
					Namespace.m_Types.f_Clear();
				
				if (!Namespace.m_bExistsInDatabase)
				{
					Namespace.m_bExistsInDatabase = true;
					Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, NamespaceName, Namespace.m_Namespace) > [this](TCAsyncResult<void> &&_Result)
						{
							if (_Result)
								return;
							// Log because error is not reported to anyone else
							DMibLogWithCategory
								(
									Mib/Concurrency/Trust
									, Error
									, "Failed to add namespace in database when adding subscription to namespace: {}"
									, _Result.f_GetExceptionStr()
								)
							;
						}
					;
				}
			}
		;
		return Continuation;
	}
	
	void CDistributedActorTrustManager::fp_UnsubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
	{
		auto &Internal = *mp_pInternal;
		auto *pNamespace = Internal.m_Namespaces.f_FindEqual(_pState->m_NamespaceName);
		if (!pNamespace)
			return;
		auto *pType = pNamespace->m_Types.f_FindEqual(_pState->m_TypeHash);
		if (!pType)
			return;
		
		pType->m_Subscriptions.f_Remove(_pState);
		--pNamespace->m_nSubscriptions;
		
		if (pNamespace->m_nSubscriptions == 0)
		{
			if (pNamespace->m_bSubscribing)
			{
				pNamespace->m_OnSubscribe.f_Insert
					(
						[this](TCAsyncResult<void> const &_Result, CInternal::CNamespaceState &_Namespace) -> bool
						{
							if (_Namespace.m_nSubscriptions != 0)
								return false;
							_Namespace.m_Subscription.f_Clear();
							auto &Internal = *mp_pInternal;
							if (!_Namespace.m_bExistsInDatabase)
							{
								Internal.m_Namespaces.f_Remove(&_Namespace);
								return true;
							}
							return false;
						}					
					)
				;
				return;
			}
			pNamespace->m_Subscription.f_Clear();
			pNamespace->m_Types.f_Clear();
			if (!pNamespace->m_bExistsInDatabase)
				Internal.m_Namespaces.f_Remove(pNamespace);
			return;
		}
		
		if (!pType->m_Subscriptions.f_IsEmpty())
			return;
		if (!pType->m_ActorToHost.f_IsEmpty())
			return;
		pNamespace->m_Types.f_Remove(pType);
	}
}
