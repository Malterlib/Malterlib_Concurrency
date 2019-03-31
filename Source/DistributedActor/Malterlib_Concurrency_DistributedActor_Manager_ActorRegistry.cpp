// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	CDistributedActorPublication::CDistributedActorPublication(CDistributedActorPublication &&) = default;
	CDistributedActorPublication &CDistributedActorPublication::operator = (CDistributedActorPublication &&) = default;

	CDistributedActorPublication::CDistributedActorPublication(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID)
		: mp_DistributionManager(_DistributionManager)
		, mp_Namespace(_Namespace)
		, mp_ActorID(_ActorID)
	{
	}

	CDistributedActorPublication::CDistributedActorPublication() = default;

	CDistributedActorPublication::~CDistributedActorPublication()
	{
		if (mp_DistributionManager)
			f_Destroy() > fg_DiscardResult();
	}

	bool CDistributedActorPublication::f_IsValid() const
	{
		return !mp_DistributionManager.f_IsEmpty();
	}

	TCFuture<void> CDistributedActorPublication::f_Destroy()
	{
		if (!mp_DistributionManager)
			return fg_Explicit();

		TCFuture<void> RemovePublicationFuture;
		if (auto DistributionManager = mp_DistributionManager.f_Lock())
			RemovePublicationFuture = DistributionManager(&CActorDistributionManager::fp_RemoveActorPublication, mp_Namespace, mp_ActorID);
		else
			RemovePublicationFuture = fg_Explicit();

		mp_DistributionManager.f_Clear();
		mp_Namespace.f_Clear();
		mp_ActorID.f_Clear();

		return RemovePublicationFuture;
	}

	void CDistributedActorPublication::f_Republish(NStr::CStr const &_HostID) const
	{
		if (mp_DistributionManager)
		{
			if (auto DistributionManager = mp_DistributionManager.f_Lock())
				DistributionManager(&CActorDistributionManager::fp_RepublishActorPublication, mp_Namespace, mp_ActorID, _HostID) > fg_DiscardResult();
		}
	}

	bool CActorDistributionManagerInternal::fp_NamespaceAllowedForAnonymous(NStr::CStr const &_Namespace) const
	{
		return _Namespace.f_StartsWith("Anonymous/");
	}

	TCFuture<CDistributedActorPublication> CActorDistributionManager::fp_PublishActor
		(
			TCDistributedActor<CActor> &&_Actor
			, NStr::CStr const &_Namespace
			, NPrivate::CDistributedActorInterfaceInfo const &_ClassesToPublish
		)
	{
		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return DMibErrorInstance("Invalid namespace name");

		TCPromise<CDistributedActorPublication> Promise;
		auto &Internal = *mp_pInternal;
		auto &LocalNamespace = Internal.m_LocalNamespaces[_Namespace];
		auto pDistributedActorData = static_cast<NPrivate::CDistributedActorData const *>(_Actor->f_GetDistributedActorData().f_Get());
		DMibRequire(pDistributedActorData);

		auto &PublishedActor = LocalNamespace.m_Actors[pDistributedActorData->m_ActorID];

		if (PublishedActor.m_Actor)
		{
			Promise.f_SetException(DMibErrorInstance("This actor has already been published in this namespace"));
			return Promise.f_MoveFuture();
		}

		PublishedActor.m_pNamespace = &LocalNamespace;
		Internal.m_PublishedActors.f_Insert(PublishedActor);
		PublishedActor.m_Actor = _Actor;
		PublishedActor.m_Hierarchy = _ClassesToPublish.f_GetInterfaceHashes();
		PublishedActor.m_Hierarchy.f_Sort();
		PublishedActor.m_ProtocolVersions = _ClassesToPublish.f_GetProtocolVersions();

		CDistributedActorCommand_Publish Publish;
		Publish.m_ActorID = pDistributedActorData->m_ActorID;
		Publish.m_Namespace = _Namespace;
		Publish.m_Hierarchy = PublishedActor.m_Hierarchy;
		Publish.m_ProtocolVersions = _ClassesToPublish.f_GetProtocolVersions();

		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		Stream << Publish;
		auto Data = Stream.f_MoveVector();

		for (auto &pHost : Internal.m_Hosts)
		{
			auto &Host = *pHost;
			if (!Host.f_CanSendPublish())
				continue;
			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
				continue;
			if (Host.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_Namespace))
				continue;

			Internal.fp_QueuePacket(pHost, fg_TempCopy(Data));
		}

		Promise.f_SetResult(CDistributedActorPublication(fg_ThisActor(this), _Namespace, pDistributedActorData->m_ActorID));

		return Promise.f_MoveFuture();
	}

	void CActorDistributionManager::fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID)
	{
		TCPromise<CDistributedActorPublication> Promise;
		auto &Internal = *mp_pInternal;
		auto *pLocalNamespace = Internal.m_LocalNamespaces.f_FindEqual(_NamespaceID);
		if (!pLocalNamespace)
			return;
		auto &LocalNamespace = *pLocalNamespace;
		auto *pPublishedActor = LocalNamespace.m_Actors.f_FindEqual(_ActorID);
		if (!pPublishedActor)
			return;

		auto *pHosts = Internal.m_HostsByRealHostID.f_FindEqual(_HostID);
		if (!pHosts)
			return;

		for (auto &Host : *pHosts)
		{
			if (Host.m_RealHostID != _HostID)
				continue;
			if (!Host.f_CanSendPublish())
				return;
			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_NamespaceID))
				return;
			if (Host.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_NamespaceID))
				return;

			auto &PublishedActor = *pPublishedActor;

			CDistributedActorCommand_Unpublish Unpublish;
			Unpublish.m_ActorID = _ActorID;
			Unpublish.m_Namespace = _NamespaceID;

			CDistributedActorCommand_Publish Publish;
			Publish.m_ActorID = _ActorID;
			Publish.m_Namespace = _NamespaceID;
			Publish.m_Hierarchy = PublishedActor.m_Hierarchy;
			Publish.m_ProtocolVersions = PublishedActor.m_ProtocolVersions;

			NContainer::CSecureByteVector UnpublishData;
			{
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
				Stream << Unpublish;
				UnpublishData = Stream.f_MoveVector();
			}

			NContainer::CSecureByteVector PublishData;
			{
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
				Stream << Publish;
				PublishData = Stream.f_MoveVector();
			}

			Internal.fp_QueuePacket(fg_Explicit(&Host), fg_Move(UnpublishData));
			Internal.fp_QueuePacket(fg_Explicit(&Host), fg_Move(PublishData));
		}
	}

	void CActorDistributionManager::fp_RemoveActorPublication(NStr::CStr const &_Namespace, NStr::CStr const &_ActorID)
	{
		auto &Internal = *mp_pInternal;
		auto *pLocalNamespace = Internal.m_LocalNamespaces.f_FindEqual(_Namespace);
		DMibRequire(pLocalNamespace);
		auto *pPublishedActor = pLocalNamespace->m_Actors.f_FindEqual(_ActorID);
		DMibRequire(pPublishedActor);

		Internal.m_PublishedActors.f_Remove(pPublishedActor);

		pLocalNamespace->m_Actors.f_Remove(pPublishedActor);

		if (pLocalNamespace->m_Actors.f_IsEmpty())
			Internal.m_LocalNamespaces.f_Remove(pLocalNamespace);

		CDistributedActorCommand_Unpublish Unpublish;
		Unpublish.m_ActorID = _ActorID;
		Unpublish.m_Namespace = _Namespace;
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		Stream << Unpublish;
		auto Data = Stream.f_MoveVector();

		for (auto &pHost : Internal.m_Hosts)
		{
			auto &Host = *pHost;
			if (!Host.f_CanSendPublish())
				continue;
			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
				continue;
			if (Host.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_Namespace))
				continue;

			Internal.fp_QueuePacket(pHost, fg_TempCopy(Data));
		}
	}

	CAbstractDistributedActor::CAbstractDistributedActor() = default;

	CAbstractDistributedActor::CAbstractDistributedActor
		(
			NStr::CStr const &_ActorID
			, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
			, TCWeakActor<CActorDistributionManager> const &_DistributionManager
			, NContainer::TCVector<uint32> const &_InheritanceHierarchy
			, NStr::CStr const &_UniqueHostID
			, CHostInfo const &_HostInfo
			, CDistributedActorProtocolVersions &_ProtocolVersions
		)
		: mp_ActorID(_ActorID)
		, mp_pHost(_pHost)
		, mp_DistributionManager(_DistributionManager)
		, mp_InheritanceHierarchy(_InheritanceHierarchy)
		, mp_UniqueHostID(_UniqueHostID)
		, mp_HostInfo(_HostInfo)
		, mp_ProtocolVersions(_ProtocolVersions)
	{
	}

	CDistributedActorIdentifier CAbstractDistributedActor::f_GetIdentifier() const
	{
		return CDistributedActorIdentifier(mp_pHost, mp_ActorID);
	}

	TCDistributedActor<CActor> CAbstractDistributedActor::f_GetActor(uint32 _TypeHash, CDistributedActorProtocolVersions const &_SupportedVersions) const
	{
		if (mp_InheritanceHierarchy.f_BinarySearch(_TypeHash) < 0)
			return {};

		uint32 Version;
		if (!mp_ProtocolVersions.f_HighestSupportedVersion(_SupportedVersions, Version))
			return {};

		auto DistributionManager = mp_DistributionManager.f_Lock();
		if (!DistributionManager)
			return {};

		return fg_ConstructRemoteDistributedActor<CActor>
			(
				DistributionManager
				, mp_ActorID
				, mp_pHost
				, Version
			)
		;
	}

	NContainer::TCVector<uint32> const &CAbstractDistributedActor::f_GetTypeHashes() const
	{
		return mp_InheritanceHierarchy;
	}

	void CActorDistributionManagerInternal::fp_NotifyNewActor(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, CRemoteActor &_RemoteActor)
	{
		CAbstractDistributedActor AbstractActor
			{
				_RemoteActor.f_GetActorID()
				, _pHost
				, fg_ThisActor(m_pThis)
				, _RemoteActor.m_Hierarchy
				, _pHost->m_UniqueHostID
				, _pHost->f_GetHostInfo()
				, _RemoteActor.m_ProtocolVersions
			}
		;
		auto pAll = m_SubscribedActors.f_FindEqual("");
		if (pAll)
			pAll->m_fOnNewActor(fg_TempCopy(AbstractActor)) > fg_DiscardResult();

		auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
		if (pSpecific)
			pSpecific->m_fOnNewActor(fg_TempCopy(AbstractActor)) > fg_DiscardResult();
	}

	void CActorDistributionManagerInternal::fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor)
	{
		auto pAll = m_SubscribedActors.f_FindEqual("");
		if (pAll)
			pAll->m_fOnRemovedActorActor(CDistributedActorIdentifier{fg_Explicit(_RemoteActor.m_pHost), _RemoteActor.f_GetActorID()}) > fg_DiscardResult();

		auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
		if (pSpecific)
			pSpecific->m_fOnRemovedActorActor(CDistributedActorIdentifier{fg_Explicit(_RemoteActor.m_pHost), _RemoteActor.f_GetActorID()}) > fg_DiscardResult();
	}

	bool CActorDistributionManagerInternal::fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;

		if (!pHost->f_CanReceivePublish() || pHost->m_bDeleted)
			return true;

		CDistributedActorCommand_Publish Publish;
		_Stream >> Publish;

		bool bAllowAllNamespaces = false;
		auto &AllowedNamespaces = fp_GetAllowedNamespacesForHost(pHost, bAllowAllNamespaces);

		if (!bAllowAllNamespaces && !AllowedNamespaces.f_FindEqual(Publish.m_Namespace))
			return true;

		auto Mapped = pHost->m_RemoteActors(Publish.m_ActorID);

		if (Mapped.f_WasCreated())
		{
			auto &RemoteActor = *Mapped;
			RemoteActor.m_Namespace = Publish.m_Namespace;
			RemoteActor.m_Hierarchy = Publish.m_Hierarchy;
			RemoteActor.m_ProtocolVersions = Publish.m_ProtocolVersions;
			RemoteActor.m_pHost = pHost.f_Get();

			fp_NotifyNewActor(pHost, RemoteActor);

			m_RemoteNamespaces[Publish.m_Namespace].m_RemoteActors.f_Insert(RemoteActor);
		}

		return true;
	}

	bool CActorDistributionManagerInternal::fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		if (!pHost->f_CanReceivePublish() || pHost->m_bDeleted)
			return true;

		CDistributedActorCommand_Unpublish Unpublish;
		_Stream >> Unpublish;

		auto pRemoteActor = pHost->m_RemoteActors.f_FindEqual(Unpublish.m_ActorID);
		if (pRemoteActor)
		{
			fp_NotifyRemovedActor(*pRemoteActor);
			auto pRemoteNamespace = m_RemoteNamespaces.f_FindEqual(pRemoteActor->m_Namespace);
			DMibCheck(pRemoteNamespace);
			pRemoteNamespace->m_RemoteActors.f_Remove(pRemoteActor);
			if (pRemoteNamespace->m_RemoteActors.f_IsEmpty())
				m_RemoteNamespaces.f_Remove(pRemoteNamespace);

			pHost->m_RemoteActors.f_Remove(pRemoteActor);
		}

		return true;
	}

	namespace NActorDistributionManagerInternal
	{
		CActorPublicationSubscription::CActorPublicationSubscription(CActorDistributionManager *_pManager)
			: m_fOnNewActor(_pManager, false)
			, m_fOnRemovedActorActor(_pManager, false)
		{
		}
	}

	CActorSubscription CActorDistributionManager::f_SubscribeHostInfoChanged
		(
			TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMovable<void (CHostInfo const &_HostInfo)> &&_fHostInfoChanged
		)
	{
		auto &Internal = *mp_pInternal;
		return Internal.m_OnHostInfoChanged.f_Register(_Actor, fg_Move(_fHostInfoChanged));
	}

	TCFuture<CActorSubscription> CActorDistributionManager::f_SubscribeActors
		(
			NContainer::TCVector<NStr::CStr> const &_NameSpaces
			, TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMovable<void (CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
			, NFunction::TCFunctionMovable<void (CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
		)
	{
		auto &Internal = *mp_pInternal;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<void (CAbstractDistributedActor &&_NewActor)>> pOnNewActor = fg_Construct(fg_Move(_fOnNewActor));
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<void (CDistributedActorIdentifier const &_RemovedActor)>> pOnRemovedActor = fg_Construct(fg_Move(_fOnRemovedActor));

		NStorage::TCUniquePointer<CCombinedCallbackReference> pCallback = fg_Construct();

		auto fReportExistingActor = [&](CAbstractDistributedActor &&_RemoteActor)
			{
				fg_Dispatch
					(
						_Actor
						,
						[
							pOnNewActor = pOnNewActor
							, Actor = _RemoteActor
						]
						() mutable
						{
							(*pOnNewActor)(fg_Move(Actor));
						}
					)
					> fg_DiscardResult()
				;
			}
		;
		if (_NameSpaces.f_IsEmpty())
		{
			auto &Subscribed = *Internal.m_SubscribedActors("", this);

			pCallback->m_References.f_Insert(Subscribed.m_fOnNewActor.f_Register(_Actor, pOnNewActor));
			pCallback->m_References.f_Insert(Subscribed.m_fOnRemovedActorActor.f_Register(_Actor, pOnRemovedActor));

			for (auto &RemoteNamespace : Internal.m_RemoteNamespaces)
			{
				for (auto &RemoteActor : RemoteNamespace.m_RemoteActors)
				{
					fReportExistingActor
						(
							CAbstractDistributedActor
							(
								RemoteActor.f_GetActorID()
								, fg_Explicit(RemoteActor.m_pHost)
								, fg_ThisActor(this)
								, RemoteActor.m_Hierarchy
								, RemoteActor.m_pHost->m_UniqueHostID
								, RemoteActor.m_pHost->f_GetHostInfo()
								, RemoteActor.m_ProtocolVersions
							)
						)
					;
				}
			}
		}
		else
		{
			for (auto &Namespace : _NameSpaces)
			{
				if (!CActorDistributionManager::fs_IsValidNamespaceName(Namespace))
					return DMibErrorInstance("Invalid namespace name");
			}

			for (auto &Namespace : _NameSpaces)
			{
				auto &Subscribed = *Internal.m_SubscribedActors(Namespace, this);

				pCallback->m_References.f_Insert(Subscribed.m_fOnNewActor.f_Register(_Actor, pOnNewActor));
				pCallback->m_References.f_Insert(Subscribed.m_fOnRemovedActorActor.f_Register(_Actor, pOnRemovedActor));
				if (auto *pRemoteNamespace = Internal.m_RemoteNamespaces.f_FindEqual(Namespace))
				{
					for (auto &RemoteActor : pRemoteNamespace->m_RemoteActors)
					{
						fReportExistingActor
							(
								CAbstractDistributedActor
								(
									RemoteActor.f_GetActorID()
									, fg_Explicit(RemoteActor.m_pHost)
									, fg_ThisActor(this)
									, RemoteActor.m_Hierarchy
									, RemoteActor.m_pHost->m_UniqueHostID
									, RemoteActor.m_pHost->f_GetHostInfo()
									, RemoteActor.m_ProtocolVersions
								)
							)
						;
					}
				}
			}
		}

		return fg_Explicit(fg_Move(pCallback));
	}
}
