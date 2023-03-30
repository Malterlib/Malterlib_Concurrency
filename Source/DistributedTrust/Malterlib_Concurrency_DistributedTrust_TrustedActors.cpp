// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	CTrustedActorSubscriptionState::CTrustedActorSubscriptionState()
	{
	}

	CTrustedActorSubscriptionState::~CTrustedActorSubscriptionState()
	{
	}

	void CTrustedActorSubscriptionState::f_ApplyDeferredChanges()
	{
		auto DeferredChanged = fg_Move(m_DeferredChanges);

		for (auto &Change : DeferredChanged)
		{
			if (Change.f_IsOfType<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>())
				fg_DirectDispatch(f_AddDistributedActors(fg_Move(Change.f_GetAsType<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>()))) > fg_DiscardResult();
			else
				fg_DirectDispatch(f_RemoveDistributedActors(fg_Move(Change.f_GetAsType<NContainer::TCSet<CDistributedActorIdentifier>>()))) > fg_DiscardResult();
		}
	}
}

namespace NMib::NConcurrency
{
	auto CDistributedActorTrustManager::f_EnumNamespacePermissions(bool _bIncludeHostInfo) -> TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>>
	{
		TCPromise<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> Promise;

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
			return Promise <<= fg_Move(Return);

		TCActorResultMap<NStr::CStr, CHostInfo> HostInfos;

		auto ThisActor = fg_ThisActor(this);
		for (auto &HostID : HostIDs)
			ThisActor(&CDistributedActorTrustManager::fp_GetHostInfo, HostID) > HostInfos.f_AddResult(HostID);

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
			co_return DMibErrorInstance("Invalid namespace name");

		for (auto &HostID : _Hosts)
		{
			if (!CActorDistributionManager::fs_IsValidHostID(HostID))
				co_return DMibErrorInstance("Invalid host id");
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
			co_return {};

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
						fg_Dispatch(pSubscription->m_DispatchActor, pSubscription->f_AddDistributedActors(fg_Move(AddedActors))) > SubscriptionResults.f_AddResult();
				}
			}
		}

		if (Namespace.m_bExistsInDatabase)
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetNamespace, _Namespace, Namespace.m_Namespace);
		else
		{
			Namespace.m_bExistsInDatabase = true;
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, _Namespace, Namespace.m_Namespace);
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
			co_await SubscriptionResults.f_GetResults(); // Intentionally ignore results
		else
			SubscriptionResults.f_GetResults() > fg_DiscardResult();

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_DisallowHostsForNamespace
		(
			 NStr::CStr const &_Namespace
			 , NContainer::TCSet<NStr::CStr> const &_Hosts
			 , EDistributedActorTrustManagerOrderingFlag _OrderingFlags
		)
	{
		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			co_return DMibErrorInstance("Invalid namespace name");

		for (auto &HostID : _Hosts)
		{
			if (!CActorDistributionManager::fs_IsValidHostID(HostID))
				co_return DMibErrorInstance("Invalid host id");
		}

		auto &Internal = *mp_pInternal;
		auto *pNamespace = Internal.m_Namespaces.f_FindEqual(_Namespace);
		if (!pNamespace)
			co_return {};

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> NException::CExceptionPointer
				{
					pNamespace = Internal.m_Namespaces.f_FindEqual(_Namespace);
					return {};
				}
			)
		;

		NContainer::TCSet<NStr::CStr> HostsRemoved;
		for (auto &Host : _Hosts)
		{
			if (pNamespace->m_Namespace.m_AllowedHosts.f_Remove(Host))
				HostsRemoved[Host];
		}
		if (HostsRemoved.f_IsEmpty())
			co_return {};

		TCActorResultVector<void> SubscriptionResults;

		for (auto &Type : pNamespace->m_Types)
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
					fg_Dispatch(pSubscription->m_DispatchActor, pSubscription->f_RemoveDistributedActors(fg_Move(RemovedActors))) > SubscriptionResults.f_AddResult();
			}
		}

		if (pNamespace->m_Namespace.m_AllowedHosts.f_IsEmpty())
		{
			if (pNamespace->m_bExistsInDatabase)
			{
				pNamespace->m_bExistsInDatabase = false;
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveNamespace, _Namespace);
			}

			if (pNamespace && pNamespace->m_nSubscriptions == 0)
				Internal.m_Namespaces.f_Remove(_Namespace);
		}
		else
		{
			if (pNamespace->m_bExistsInDatabase)
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetNamespace, _Namespace, pNamespace->m_Namespace);
			else
			{
				pNamespace->m_bExistsInDatabase = true;
				co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddNamespace, _Namespace, pNamespace->m_Namespace);
			}
		}

		if (_OrderingFlags & EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
			co_await SubscriptionResults.f_GetResults(); // Intentionally ignore results
		else
			SubscriptionResults.f_GetResults() > fg_DiscardResult();

		co_return {};
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
		-> TCFuture<CTrustedActorSubscription>
	{
		TCPromise<CTrustedActorSubscription> Promise;

		auto &Internal = *mp_pInternal;
		auto &Namespace = Internal.m_Namespaces[_pState->m_NamespaceName];
		auto NamespaceName = _pState->m_NamespaceName;

		++Namespace.m_nSubscriptions;

		auto Subscription = g_ActorSubscription / [this, _pState]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				_pState->m_DispatchActor.f_Clear();

				auto *pNamespace = Internal.m_Namespaces.f_FindEqual(_pState->m_NamespaceName);
				DMibFastCheck(pNamespace);

				if (!pNamespace)
					co_return {};

				--pNamespace->m_nSubscriptions;

				auto *pType = pNamespace->m_Types.f_FindEqual(_pState->m_TypeHash);
				if (pType)
				{
					pType->m_Subscriptions.f_Remove(_pState);

					if (pType->m_Subscriptions.f_IsEmpty())
						pNamespace->m_Types.f_Remove(pType);
				}

				if (pNamespace->m_nSubscriptions == 0 && pNamespace->m_Types.f_IsEmpty())
				{
					DMibFastCheck(!pNamespace->m_bSubscribing);

					pNamespace->m_Subscription.f_Clear();

					if (!pNamespace->m_bExistsInDatabase)
						Internal.m_Namespaces.f_Remove(pNamespace);
				}

				co_return {};
			}
		;

		if (!Namespace.m_Subscription.f_IsEmpty())
		{
			auto &Type = Namespace.m_Types[_pState->m_TypeHash];
			Type.m_Subscriptions[_pState];

			return Promise <<= CTrustedActorSubscription
				{
					.m_InitialActors = Type.f_GetAllowed(*_pState)
					, .m_Subscription = fg_Move(Subscription)
				}
			;
		}


		auto fOnSubscribe = [_pState, NamespaceName, Promise, Subscription = fg_Move(Subscription)](TCAsyncResult<void> const &_Result, CInternal::CNamespaceState &_Namespace) mutable -> bool
			{
				if (!_Result)
				{
					Promise.f_SetException(_Result);
					return false;
				}
				auto &Type = _Namespace.m_Types[_pState->m_TypeHash];
				Type.m_Subscriptions[_pState];
				Promise.f_SetResult
					(
						CTrustedActorSubscription
						{
							.m_InitialActors = Type.f_GetAllowed(*_pState)
							, .m_Subscription = fg_Move(Subscription)
						}
					)
				;
				return false;
			}
		;

		Namespace.m_OnSubscribe.f_Insert(fg_Move(fOnSubscribe));

		if (Namespace.m_bSubscribing)
			return Promise.f_MoveFuture();

		Namespace.m_bSubscribing = true;
		mint SubscriptionSequence = ++Internal.m_NamespaceSubscriptionSequence;
		Namespace.m_SubscriptionSequence = SubscriptionSequence;

		Internal.m_ActorDistributionManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, NamespaceName
				, fg_ThisActor(this)
				, [this, NamespaceName, SubscriptionSequence](CAbstractDistributedActor &&_NewActor)
				{
					auto &Internal = *mp_pInternal;
					auto pNamespace = Internal.m_Namespaces.f_FindEqual(NamespaceName);
					if (!pNamespace)
						return;
					auto &Namespace = *pNamespace;

					if (Namespace.m_SubscriptionSequence != SubscriptionSequence)
						return;

					if (!Namespace.m_Subscription && !Namespace.m_bSubscribing)
						return;

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

						for (auto &pSubscription : Type.m_Subscriptions)
						{
							auto NewActor = TypeActor.m_AbstractActor.f_GetActor(TypeHash, pSubscription->m_ProtocolVersions);
							if (!NewActor)
							{
								if (TypeActor.m_AbstractActor.f_GetTypeHashes().f_BinarySearch(TypeHash) < 0)
								{
									DMibLogWithCategory
										(
											Mib/Concurrency/Trust
											, Warning
											, "Encountered actor with type missing in type hierarchy in namespace '{}' from host '{}'"
											, NamespaceName
											, _NewActor.f_GetHostInfo().f_GetDesc()
										)
									;
									continue;
								}

								uint32 Version;
								if (!TypeActor.m_AbstractActor.f_GetProtocolVersions().f_HighestSupportedVersion(pSubscription->m_ProtocolVersions, Version))
								{
									DMibLogWithCategory
										(
											Mib/Concurrency/Trust
											, Warning
											, "Encountered actor with unsupported versions in namespace '{}' from host '{}'"
											, NamespaceName
											, _NewActor.f_GetHostInfo().f_GetDesc()
										)
									;
									continue;
								}

								continue;
							}
							NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> AddedActors;
							auto &TrustedActor = AddedActors[_NewActor.f_GetIdentifier()];
							TrustedActor.m_TrustInfo = TrustInfo;
							pSubscription->m_ProtocolVersions.f_HighestSupportedVersion(TypeActor.m_AbstractActor.f_GetProtocolVersions(), TrustedActor.m_ProtocolVersion);
							TrustedActor.m_Actor = fg_Move(NewActor);
							fg_Dispatch(pSubscription->m_DispatchActor, pSubscription->f_AddDistributedActors(fg_Move(AddedActors))) > fg_DiscardResult();
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

					if (!Namespace.m_Subscription && !Namespace.m_bSubscribing)
						return;

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
							fg_Dispatch(pSubscription->m_DispatchActor, pSubscription->f_RemoveDistributedActors(fg_Move(Removed))) > fg_DiscardResult();
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
			}
		;
		return Promise.f_MoveFuture();
	}
}
