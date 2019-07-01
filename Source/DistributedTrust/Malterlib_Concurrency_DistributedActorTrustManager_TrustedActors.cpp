// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	CTrustedActorSubscriptionState::CTrustedActorSubscriptionState()
	{
	}

	CTrustedActorSubscriptionState::~CTrustedActorSubscriptionState()
	{
	}
}

namespace NMib::NConcurrency
{
	auto CDistributedActorTrustManager::f_EnumNamespacePermissions(bool _bIncludeHostInfo) -> TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>>
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
				for (auto iHost = Type.m_Hosts.f_GetIterator(); iHost; ++iHost)
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

		TCPromise<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> Promise;
			
		HostInfos.f_GetResults() 
			> Promise / [Promise, Return = fg_Move(Return)] 
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
				Promise.f_SetResult(fg_Move(Return));
			}
		;
		return Promise.f_MoveFuture();				
	}

	TCFuture<void> CDistributedActorTrustManager::f_AllowHostsForNamespace
		(
			 NStr::CStr const &_Namespace
			 , NContainer::TCSet<NStr::CStr> const &_Hosts
			 , EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return DMibErrorInstance("Invalid namespace name");
		
		for (auto &HostID : _Hosts)
		{
			if (!CActorDistributionManager::fs_IsValidHostID(HostID))
				return DMibErrorInstance("Invalid host id");
		}
		
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

		TCActorResultVector<void> SubscriptionResults;

		for (auto &Type : Namespace.m_Types)
		{
			for (auto &AddedHost : HostsAdded)
			{
				auto *pHost = Type.m_Hosts.f_FindEqual(AddedHost);
				if (!pHost)
					continue;
				for (auto &Actor : pHost->m_Actors)
					Type.m_AllowedActors.f_Insert(Actor);
				for (auto &pSubscription : Type.m_Subscriptions)
				{
					NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> AddedActors;
					for (auto &Actor : pHost->m_Actors)
					{
						auto NewActor = Actor.m_AbstractActor.f_GetActor(Type.f_GetTypeHash(), pSubscription->m_ProtocolVersions);
						if (NewActor)
						{
							auto &TrustedActor = AddedActors[Actor.f_GetIdentifier()];
							TrustedActor.m_TrustInfo = Actor.m_TrustedInfo;
							pSubscription->m_ProtocolVersions.f_HighestSupportedVersion(Actor.m_AbstractActor.f_GetProtocolVersions(), TrustedActor.m_ProtocolVersion); 
							TrustedActor.m_Actor = fg_Move(NewActor);
						}
					}
					if (!AddedActors.f_IsEmpty())
						pSubscription->f_AddDistributedActors(AddedActors) > SubscriptionResults.f_AddResult();
				}
			}
		}
		
		TCPromise<void> SaveDatabasePromise;
		if (Namespace.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetNamespace, _Namespace, Namespace.m_Namespace) > SaveDatabasePromise;
		else
		{
			Namespace.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, _Namespace, Namespace.m_Namespace) > SaveDatabasePromise;
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
		{
			TCPromise<void> Promise;
			SaveDatabasePromise.f_MoveFuture() + SubscriptionResults.f_GetResults()
				> Promise / [Promise](CVoidTag, NContainer::TCVector<TCAsyncResult<void>> &&) // Intentionally ignore results
				{
					Promise.f_SetResult();
				}
			;
			return Promise.f_MoveFuture();
		}

		SubscriptionResults.f_GetResults() > fg_DiscardResult();

		return SaveDatabasePromise.f_MoveFuture();
	}
	
	TCFuture<void> CDistributedActorTrustManager::f_DisallowHostsForNamespace
		(
			 NStr::CStr const &_Namespace
			 , NContainer::TCSet<NStr::CStr> const &_Hosts
			 , EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return DMibErrorInstance("Invalid namespace name");
		
		for (auto &HostID : _Hosts)
		{
			if (!CActorDistributionManager::fs_IsValidHostID(HostID))
				return DMibErrorInstance("Invalid host id");
		}
		
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
		
		TCActorResultVector<void> SubscriptionResults;

		for (auto &Type : Namespace.m_Types)
		{
			for (auto &RemovedHost : HostsRemoved)
			{
				auto *pHost = Type.m_Hosts.f_FindEqual(RemovedHost);
				if (!pHost)
					continue;
				for (auto &Actor : pHost->m_Actors)
					Type.m_AllowedActors.f_Remove(Actor);
				NContainer::TCSet<CDistributedActorIdentifier> RemovedActors; 
				for (auto &Actor : pHost->m_Actors)
					RemovedActors[Actor.f_GetIdentifier()];
				for (auto &pSubscription : Type.m_Subscriptions)
					pSubscription->f_RemoveDistributedActors(RemovedActors) > SubscriptionResults.f_AddResult();
			}
		}

		TCPromise<void> SaveDatabasePromise;

		if (Namespace.m_Namespace.m_AllowedHosts.f_IsEmpty())
		{
			if (Namespace.m_bExistsInDatabase)
			{
				Namespace.m_bExistsInDatabase = false;
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveNamespace, _Namespace) > SaveDatabasePromise;
			}
			else
				SaveDatabasePromise.f_SetResult();

			if (Namespace.m_nSubscriptions == 0)
				Internal.m_Namespaces.f_Remove(_Namespace);
		}
		else
		{
			if (Namespace.m_bExistsInDatabase)
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetNamespace, _Namespace, Namespace.m_Namespace) > SaveDatabasePromise;
			else
			{
				Namespace.m_bExistsInDatabase = true;
				Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, _Namespace, Namespace.m_Namespace) > SaveDatabasePromise;
			}
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
		{
			TCPromise<void> Promise;
			SaveDatabasePromise.f_MoveFuture() + SubscriptionResults.f_GetResults()
				> Promise / [Promise](CVoidTag, NContainer::TCVector<TCAsyncResult<void>> &&) // Intentionally ignore results
				{
					Promise.f_SetResult();
				}
			;
			return Promise.f_MoveFuture();
		}

		SubscriptionResults.f_GetResults() > fg_DiscardResult();

		return SaveDatabasePromise.f_MoveFuture();
	}
	
	auto CDistributedActorTrustManager::CInternal::CNamespaceTypeState::f_GetAllowed(NPrivate::CTrustedActorSubscriptionState const &_State)
		-> NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> 
	{
		NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> Actors;
		for (auto &Actor : m_AllowedActors)
		{
			auto NewActor = Actor.m_AbstractActor.f_GetActor(_State.m_TypeHash, _State.m_ProtocolVersions);
			if (NewActor)
			{
				auto &TrustedActor = Actors[Actor.f_GetIdentifier()];
				TrustedActor.m_Actor = fg_Move(NewActor);
				TrustedActor.m_TrustInfo = Actor.m_TrustedInfo; 
				_State.m_ProtocolVersions.f_HighestSupportedVersion(Actor.m_AbstractActor.f_GetProtocolVersions(), TrustedActor.m_ProtocolVersion); 
			}
		}
		return Actors;
	}

	auto CDistributedActorTrustManager::fp_SubscribeTrustedActors(NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
		-> TCFuture<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>
	{
		auto &Internal = *mp_pInternal;
		auto &Namespace = Internal.m_Namespaces[_pState->m_NamespaceName];
		auto NamespaceName = _pState->m_NamespaceName;

		if (!Namespace.m_Subscription.f_IsEmpty())
		{
			auto &Type = Namespace.m_Types[_pState->m_TypeHash];
			Type.m_Subscriptions[_pState];
			++Namespace.m_nSubscriptions;
			return fg_Explicit(Type.f_GetAllowed(*_pState));
		}
	
		TCPromise<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>> Promise;
		auto fOnSubscribe = [_pState, NamespaceName, Promise](TCAsyncResult<void> const &_Result, CInternal::CNamespaceState &_Namespace) -> bool
			{
				if (!_Result)
				{
					Promise.f_SetException(_Result);
					return false;
				}
				auto &Type = _Namespace.m_Types[_pState->m_TypeHash];
				Type.m_Subscriptions[_pState];
				++_Namespace.m_nSubscriptions;
				Promise.f_SetResult(Type.f_GetAllowed(*_pState));
				return false;
			}
		;
		
		Namespace.m_OnSubscribe.f_Insert(fg_Move(fOnSubscribe));
		
		if (Namespace.m_bSubscribing)
			return Promise.f_MoveFuture();
		
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
						auto ActorIdentifier = _NewActor.f_GetIdentifier();
						auto &TypeActor = Type.m_Actors[ActorIdentifier];
						auto &Host = Type.m_Hosts[HostID];
						TypeActor.m_TrustedInfo = TrustInfo;
						TypeActor.m_AbstractActor = _NewActor;
						TypeActor.m_pHost = &Host;
						Host.m_Actors.f_Insert(TypeActor);
						
						if (!bAllowed)
							continue;
						Type.m_AllowedActors.f_Insert(TypeActor);

						if (Type.m_Subscriptions.f_IsEmpty())
							continue;
						for (auto &pSubscription : Type.m_Subscriptions)
						{
							auto NewActor = TypeActor.m_AbstractActor.f_GetActor(TypeHash, pSubscription->m_ProtocolVersions);
							if (!NewActor)
								continue;
							NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> AddedActors;
							auto &TrustedActor = AddedActors[_NewActor.f_GetIdentifier()];
							TrustedActor.m_TrustInfo = TrustInfo;
							pSubscription->m_ProtocolVersions.f_HighestSupportedVersion(TypeActor.m_AbstractActor.f_GetProtocolVersions(), TrustedActor.m_ProtocolVersion); 
							TrustedActor.m_Actor = fg_Move(NewActor);
							pSubscription->f_AddDistributedActors(AddedActors) > fg_DiscardResult();
						}
					}
				}
				, [this, NamespaceName](CDistributedActorIdentifier const &_RemovedActor)
				{
					auto &Internal = *mp_pInternal;
					auto pNamespace = Internal.m_Namespaces.f_FindEqual(NamespaceName);
					if (!pNamespace)
						return;
					auto &Namespace = *pNamespace;
					
					NContainer::TCVector<uint32> TypesToRemove;
					for (auto &Type : Namespace.m_Types)
					{
						auto pTypeActor = Type.m_Actors.f_FindEqual(_RemovedActor);
						if (!pTypeActor)
							continue;
						auto &TypeActor = *pTypeActor;
						bool bWasAllowed = TypeActor.m_LinkAllowed.f_IsInTree();
						if (bWasAllowed)
							Type.m_AllowedActors.f_Remove(TypeActor);
						auto &Host = *TypeActor.m_pHost;
						Host.m_Actors.f_Remove(TypeActor);
						
						if (Host.m_Actors.f_IsEmpty())
						{
							Type.m_Hosts.f_Remove(&Host);
						
							if (Type.m_Hosts.f_IsEmpty() && Type.m_Subscriptions.f_IsEmpty())
								TypesToRemove.f_Insert(pNamespace->m_Types.fs_GetKey(Type));
						}
						
						if (!bWasAllowed)
							continue;

						if (Type.m_Subscriptions.f_IsEmpty())
							continue;
						NContainer::TCSet<CDistributedActorIdentifier> Removed;
						Removed[_RemovedActor];
						for (auto &pSubscription : Type.m_Subscriptions)
							pSubscription->f_RemoveDistributedActors(Removed) > fg_DiscardResult();
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
					Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, NamespaceName, Namespace.m_Namespace) > [](TCAsyncResult<void> &&_Result)
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
		return Promise.f_MoveFuture();
	}
	
	void CDistributedActorTrustManager::fp_UnsubscribeTrustedActors(NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
	{
		_pState->m_DispatchActor.f_Clear();

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
		if (!pType->m_Actors.f_IsEmpty())
			return;
		pNamespace->m_Types.f_Remove(pType);
	}
}
