// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	namespace
	{
		struct CPendingWait
		{
			NStorage::TCSharedPointerSupportWeak<NActorDistributionManagerInternal::CHost> m_pHost;
			uint32 m_WaitID;
		};
	}

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

	TCFuture<void> CDistributedActorPublication::f_Destroy(fp32 _WaitForPublicationsTimeout)
	{
		TCPromise<void> Promise;

		if (!mp_DistributionManager)
			return Promise <<= g_Void;

		if (auto DistributionManager = mp_DistributionManager.f_Lock())
			DistributionManager(&CActorDistributionManager::fp_RemoveActorPublication, mp_Namespace, mp_ActorID, _WaitForPublicationsTimeout) > Promise;
		else
			Promise.f_SetResult();

		mp_DistributionManager.f_Clear();
		mp_Namespace.f_Clear();
		mp_ActorID.f_Clear();

		return Promise.f_MoveFuture();
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
			, fp32 _WaitForPublicationsTimeout
		)
	{
		TCPromise<CDistributedActorPublication> Promise;

		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return Promise <<= DMibErrorInstance("Invalid namespace name");

		auto pDistributedActorData = static_cast<NPrivate::CDistributedActorData const *>(_Actor->f_GetDistributedActorData().f_Get());
		DMibRequire(pDistributedActorData);

		auto &Internal = *mp_pInternal;

		if (Internal.m_PublishedActors.f_FindEqual(pDistributedActorData->m_ActorID))
			return Promise <<= DMibErrorInstance("An actor can only be published in one namespace");

		auto &LocalNamespace = Internal.m_LocalNamespaces[_Namespace];
		auto &PublishedActor = LocalNamespace.m_Actors[pDistributedActorData->m_ActorID];

		if (PublishedActor.m_Actor)
			return Promise <<= DMibErrorInstance("This actor has already been published in this namespace");

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

		TCActorResultVector<void> WaitResults;
		NContainer::TCVector<CPendingWait> PendingWaits;

		for (auto &pHost : Internal.m_Hosts)
		{
			auto &Host = *pHost;
			if (!Host.f_CanSendPublish())
				continue;

			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
				continue;

			if (Host.m_HostInfo.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_Namespace))
				continue;

			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
			auto VersionScope = Host.f_StreamVersion(Stream);

			if (_WaitForPublicationsTimeout && Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			{
				auto WaitID = Publish.m_WaitPublicationID = Host.f_GetUniqueWaitPublicationID();

				Host.m_WaitingPublications[WaitID].m_Promise.f_Future() > WaitResults.f_AddResult();

				PendingWaits.f_Insert(CPendingWait{.m_pHost = pHost, .m_WaitID = WaitID});
			}
			else
				Publish.m_WaitPublicationID = 0;

			Stream << Publish;

			Internal.fp_QueuePacket(pHost, Stream.f_MoveVector());
		}

		if (WaitResults.f_IsEmpty())
			return Promise <<= CDistributedActorPublication(fg_ThisActor(this), _Namespace, pDistributedActorData->m_ActorID);

		WaitResults.f_GetResults().f_Timeout(_WaitForPublicationsTimeout, "Timed out waiting for publication results")
			>
			[
				Promise
				, PendingWaits = fg_Move(PendingWaits)
				, Publication = CDistributedActorPublication(fg_ThisActor(this), _Namespace, pDistributedActorData->m_ActorID)
			]
			(TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results) mutable
			{
				if (!_Results)
					DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "Failed to wait for publications: {}", _Results.f_GetExceptionStr());
				else
				{
					for (auto &Result : *_Results)
					{
						if (!Result)
							DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "Failed to wait for publication: {}", Result.f_GetExceptionStr());
					}
				}

				for (auto &Wait : PendingWaits)
					Wait.m_pHost->m_WaitingPublications.f_Remove(Wait.m_WaitID);

				Promise.f_SetResult(fg_Move(Publication));
			}
		;

		return Promise.f_Future();
	}

	void CActorDistributionManager::fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID)
	{
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
			if (Host.m_HostInfo.m_RealHostID != _HostID)
				continue;
			if (!Host.f_CanSendPublish())
				return;
			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_NamespaceID))
				return;
			if (Host.m_HostInfo.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_NamespaceID))
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
				auto VersionScope = Host.f_StreamVersion(Stream);

				Stream << Publish;
				PublishData = Stream.f_MoveVector();
			}

			Internal.fp_QueuePacket(fg_Explicit(&Host), fg_Move(UnpublishData));
			Internal.fp_QueuePacket(fg_Explicit(&Host), fg_Move(PublishData));
		}
	}

	TCFuture<void> CActorDistributionManager::fp_RemoveActorPublication(NStr::CStr const &_Namespace, NStr::CStr const &_ActorID, fp32 _WaitForPublicationsTimeout)
	{
		TCPromise<void> Promise;

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

		TCActorResultVector<void> WaitResults;
		NContainer::TCVector<CPendingWait> PendingWaits;

		for (auto &pHost : Internal.m_Hosts)
		{
			auto &Host = *pHost;
			if (!Host.f_CanSendPublish())
				continue;

			if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
				continue;

			if (Host.m_HostInfo.m_bAnonymous && !Internal.fp_NamespaceAllowedForAnonymous(_Namespace))
				continue;

			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
			auto VersionScope = Host.f_StreamVersion(Stream);

			if (_WaitForPublicationsTimeout && Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			{
				auto WaitID = Unpublish.m_WaitPublicationID = Host.f_GetUniqueWaitPublicationID();

				Host.m_WaitingPublications[WaitID].m_Promise.f_Future() > WaitResults.f_AddResult();

				PendingWaits.f_Insert(CPendingWait{.m_pHost = pHost, .m_WaitID = WaitID});
			}
			else
				Unpublish.m_WaitPublicationID = 0;

			Stream << Unpublish;
			auto Data = Stream.f_MoveVector();

			Internal.fp_QueuePacket(pHost, fg_Move(Data));
		}

		if (WaitResults.f_IsEmpty())
			return Promise <<= g_Void;

		WaitResults.f_GetResults().f_Timeout(_WaitForPublicationsTimeout, "Timed out waiting for unpublication results")
			> [Promise, PendingWaits = fg_Move(PendingWaits)](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
			{
				if (!_Results)
					DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "Failed to wait for unpublications: {}", _Results.f_GetExceptionStr());
				else
				{
					for (auto &Result : *_Results)
					{
						if (!Result)
							DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "Failed to wait for unpublication: {}", Result.f_GetExceptionStr());
					}
				}

				for (auto &Wait : PendingWaits)
					Wait.m_pHost->m_WaitingPublications.f_Remove(Wait.m_WaitID);

				Promise.f_SetResult();
			}
		;

		return Promise.f_Future();
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
			, CDistributedActorProtocolVersions const &_ProtocolVersions
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

	void CActorDistributionManagerInternal::fp_NotifyNewActor
		(
			NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
			, CRemoteActor &_RemoteActor
			, TCActorResultVector<void> *_pResults
		)
	{
		CAbstractDistributedActor AbstractActor
			{
				_RemoteActor.f_GetActorID()
				, _pHost
				, fg_ThisActor(m_pThis)
				, _RemoteActor.m_Hierarchy
				, _pHost->m_HostInfo.m_UniqueHostID
				, _pHost->f_GetHostInfo()
				, _RemoteActor.m_ProtocolVersions
			}
		;
		auto fSendNotifications = [&](CActorPublicationSubscription &_Subscription)
			{
				if (_pResults)
				{
					for (auto &Instance : _Subscription.m_Instances)
					{
						fg_Dispatch
							(
								Instance.m_DispatchActor
								, [pOnNewActor = Instance.m_pOnNewActor, RemoteActor = fg_TempCopy(AbstractActor)]() mutable -> TCFuture<void>
								{
									co_await fg_CallSafe(pOnNewActor, fg_Move(RemoteActor));

									co_return {};
								}
							)
							> _pResults->f_AddResult()
						;
					}
				}
				else
				{
					for (auto &Instance : _Subscription.m_Instances)
					{
						fg_Dispatch
							(
								Instance.m_DispatchActor
								, [pOnNewActor = Instance.m_pOnNewActor, RemoteActor = fg_TempCopy(AbstractActor)]() mutable
								{
									fg_CallSafe(pOnNewActor, fg_Move(RemoteActor)) > fg_DiscardResult();
								}
							)
							> fg_DiscardResult()
						;
					}
				}
			}
		;
		auto pAll = m_SubscribedActors.f_FindEqual("");
		if (pAll)
			fSendNotifications(*pAll);

		auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
		if (pSpecific)
			fSendNotifications(*pSpecific);
	}

	void CActorDistributionManagerInternal::fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor, TCActorResultVector<void> *_pResults)
	{
		auto Identifier = CDistributedActorIdentifier{fg_Explicit(_RemoteActor.m_pHost), _RemoteActor.f_GetActorID()};

		auto fSendNotifications = [&](CActorPublicationSubscription &_Subscription)
			{
				if (_pResults)
				{
					for (auto &Instance : _Subscription.m_Instances)
					{
						fg_Dispatch
							(
								Instance.m_DispatchActor
								, [pOnRemovedActor = Instance.m_pOnRemovedActor, Identifier]() mutable -> TCFuture<void>
								{
									co_await fg_CallSafe(pOnRemovedActor, fg_Move(Identifier));

									co_return {};
								}
							)
							> _pResults->f_AddResult()
						;
					}
				}
				else
				{
					for (auto &Instance : _Subscription.m_Instances)
					{
						fg_Dispatch
							(
								Instance.m_DispatchActor
								, [pOnRemovedActor = Instance.m_pOnRemovedActor, Identifier]() mutable
								{
									fg_CallSafe(pOnRemovedActor, fg_Move(Identifier)) > fg_DiscardResult();
								}
							)
							> fg_DiscardResult()
						;
					}
				}
			}
		;

		auto pAll = m_SubscribedActors.f_FindEqual("");
		if (pAll)
			fSendNotifications(*pAll);

		auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
		if (pSpecific)
			fSendNotifications(*pSpecific);
	}

	bool CActorDistributionManagerInternal::fp_HandleUnpublishFinishedPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		auto &Host = *pHost;

		if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;

		CDistributedActorCommand_UnpublishFinished UnpublishFinished;

		auto VersionScope = Host.f_StreamVersion(_Stream);

		_Stream >> UnpublishFinished;

		auto *pWaiting = Host.m_WaitingPublications.f_FindEqual(UnpublishFinished.m_WaitPublicationID);
		if (pWaiting)
		{
			pWaiting->m_Promise.f_SetResult();
			Host.m_WaitingPublications.f_Remove(pWaiting);
		}

		return true;
	}

	bool CActorDistributionManagerInternal::fp_HandlePublishFinishedPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		auto &Host = *pHost;

		if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;

		CDistributedActorCommand_PublishFinished PublishFinished;

		auto VersionScope = Host.f_StreamVersion(_Stream);

		_Stream >> PublishFinished;

		auto *pWaiting = Host.m_WaitingPublications.f_FindEqual(PublishFinished.m_WaitPublicationID);
		if (pWaiting)
		{
			pWaiting->m_Promise.f_SetResult();
			Host.m_WaitingPublications.f_Remove(pWaiting);
		}

		return true;
	}

	bool CActorDistributionManagerInternal::fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		auto &Host = *pHost;

		if (!Host.f_CanReceivePublish() || Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;

		CDistributedActorCommand_Publish Publish;

		auto VersionScope = Host.f_StreamVersion(_Stream);

		_Stream >> Publish;

		bool bAllowAllNamespaces = false;
		auto &AllowedNamespaces = fp_GetAllowedNamespacesForHost(pHost, bAllowAllNamespaces);

		auto fReportResults = [this, pHost, WaitPublicationID = Publish.m_WaitPublicationID]() mutable
			{
				if (!WaitPublicationID)
					return;

				auto &Host = *pHost;

				if (Host.m_bDeleted.f_Load() || Host.m_ActorProtocolVersion.f_Load() < EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
					return;

				CDistributedActorCommand_PublishFinished PublishFinished;
				PublishFinished.m_WaitPublicationID = WaitPublicationID;

				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
				auto VersionScope = Host.f_StreamVersion(Stream);

				Stream << PublishFinished;
				auto Data = Stream.f_MoveVector();

				fp_QueuePacket(pHost, fg_Move(Data));
			}
		;

		if (!bAllowAllNamespaces && !AllowedNamespaces.f_FindEqual(Publish.m_Namespace))
		{
			fReportResults();
			return true;
		}

		auto Mapped = Host.m_RemoteActors(Publish.m_ActorID);

		bool bNeedResults = !_pConnection->m_bPulishFinished || Publish.m_WaitPublicationID;
		TCActorResultVector<void> Results;

		if (Mapped.f_WasCreated())
		{
			auto &RemoteActor = *Mapped;
			RemoteActor.m_Namespace = Publish.m_Namespace;
			RemoteActor.m_Hierarchy = Publish.m_Hierarchy;
			RemoteActor.m_ProtocolVersions = Publish.m_ProtocolVersions;
			RemoteActor.m_pHost = pHost.f_Get();

			fp_NotifyNewActor(pHost, RemoteActor, bNeedResults ? &Results : nullptr);

			m_RemoteNamespaces[Publish.m_Namespace].m_RemoteActors.f_Insert(RemoteActor);
		}

		if (bNeedResults && !Results.f_IsEmpty())
		{
			TCPromise<void> ConnectionPublishFinishedPromise{CPromiseConstructEmpty()};
			if (!_pConnection->m_bPulishFinished)
				ConnectionPublishFinishedPromise = _pConnection->m_PublishFinished.f_Insert();

			Results.f_GetResults() > [ConnectionPublishFinishedPromise, fReportResults = fg_Move(fReportResults)](auto &&_Result) mutable
				{
					if (ConnectionPublishFinishedPromise.f_IsValid() && !ConnectionPublishFinishedPromise.f_IsSet())
						ConnectionPublishFinishedPromise.f_SetResult();
					fReportResults();
				}
			;
		}
		else if (Publish.m_WaitPublicationID)
			fReportResults();

		return true;
	}

	bool CActorDistributionManagerInternal::fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		auto &Host = *pHost;

		if (!pHost->f_CanReceivePublish() || pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;

		auto VersionScope = Host.f_StreamVersion(_Stream);

		CDistributedActorCommand_Unpublish Unpublish;
		_Stream >> Unpublish;

		bool bNeedResults = !!Unpublish.m_WaitPublicationID;
		TCActorResultVector<void> Results;

		auto pRemoteActor = Host.m_RemoteActors.f_FindEqual(Unpublish.m_ActorID);
		if (pRemoteActor)
		{
			fp_NotifyRemovedActor(*pRemoteActor, bNeedResults ? &Results : nullptr);
			auto pRemoteNamespace = m_RemoteNamespaces.f_FindEqual(pRemoteActor->m_Namespace);
			DMibCheck(pRemoteNamespace);
			pRemoteNamespace->m_RemoteActors.f_Remove(pRemoteActor);
			if (pRemoteNamespace->m_RemoteActors.f_IsEmpty())
				m_RemoteNamespaces.f_Remove(pRemoteNamespace);

			Host.m_RemoteActors.f_Remove(pRemoteActor);
		}

		auto fReportResults = [this, pHost, WaitPublicationID = Unpublish.m_WaitPublicationID]() mutable
			{
				if (!WaitPublicationID)
					return;

				auto &Host = *pHost;

				if (Host.m_bDeleted || Host.m_ActorProtocolVersion.f_Load() < EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
					return;

				CDistributedActorCommand_UnpublishFinished UnpublishFinished;
				UnpublishFinished.m_WaitPublicationID = WaitPublicationID;

				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
				auto VersionScope = Host.f_StreamVersion(Stream);

				Stream << UnpublishFinished;
				auto Data = Stream.f_MoveVector();

				fp_QueuePacket(pHost, fg_Move(Data));
			}
		;

		if (bNeedResults && !Results.f_IsEmpty())
		{
			Results.f_GetResults() > [fReportResults = fg_Move(fReportResults)](auto &&_Result) mutable
				{
					fReportResults();
				}
			;
		}
		else if (Unpublish.m_WaitPublicationID)
			fReportResults();

		return true;
	}

	namespace NActorDistributionManagerInternal
	{
		CActorPublicationSubscription::CActorPublicationSubscription() = default;
		CActorPublicationSubscription::~CActorPublicationSubscription() = default;
	}

	CActorSubscription CActorDistributionManager::f_SubscribeHostInfoChanged(TCActorFunctorWeak<TCFuture<void> (CHostInfo const &_HostInfo)> &&_fHostInfoChanged)
	{
		auto &Internal = *mp_pInternal;
		auto &OnHostInfoChange = Internal.m_OnHostInfoChanged.f_Insert();
		OnHostInfoChange.m_fOnHostInfoChanged = fg_Move(_fHostInfoChanged);

		return g_ActorSubscription / [this, pOnHostInfoChange = &OnHostInfoChange, pDestroyed = OnHostInfoChange.m_pDestroyed]() -> TCFuture<void>
			{
				if (*pDestroyed)
					co_return {};

				auto ToDestroy = fg_Move(pOnHostInfoChange->m_fOnHostInfoChanged);

				auto &Internal = *mp_pInternal;
				Internal.m_OnHostInfoChanged.f_Remove(*pOnHostInfoChange);

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
	}

	TCFuture<CActorSubscription> CActorDistributionManager::f_SubscribeActors
		(
			NStr::CStr const &_Namespace
			, TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMovable<TCFuture<void> (CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
			, NFunction::TCFunctionMovable<TCFuture<void> (CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
		)
	{
		auto &Internal = *mp_pInternal;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<TCFuture<void> (CAbstractDistributedActor &&_NewActor)>> pOnNewActor = fg_Construct(fg_Move(_fOnNewActor));
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<TCFuture<void> (CDistributedActorIdentifier const &_RemovedActor)>> pOnRemovedActor = fg_Construct(fg_Move(_fOnRemovedActor));

		mint SubscriptionID = Internal.m_SubscribedActorsNextInstanceID++;

		if (_Namespace && !CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			co_return DMibErrorInstance("Invalid namespace name");

		auto &Subscribed = *Internal.m_SubscribedActors(_Namespace);
		auto &Instance = Subscribed.m_Instances[SubscriptionID];
		Instance.m_DispatchActor = _Actor;
		Instance.m_pOnNewActor = pOnNewActor;
		Instance.m_pOnRemovedActor = pOnRemovedActor;

		TCActorResultVector<void> ReportResults;
		auto fReportExistingActor = [&](CAbstractDistributedActor &&_RemoteActor)
			{
				fg_Dispatch
					(
						_Actor
						, [pOnNewActor = pOnNewActor, RemoteActor = fg_Move(_RemoteActor)]() mutable -> TCFuture<void>
						{
							co_await fg_CallSafe(pOnNewActor, fg_Move(RemoteActor));
							co_return {};
						}
					)
					> ReportResults.f_AddResult();
				;
			}
		;

		auto fReportExistingNamespace = [&](NActorDistributionManagerInternal::CRemoteNamespace const &_RemoteNamespace)
			{
				for (auto &RemoteActor : _RemoteNamespace.m_RemoteActors)
				{
					fReportExistingActor
						(
							CAbstractDistributedActor
							(
								RemoteActor.f_GetActorID()
								, fg_Explicit(RemoteActor.m_pHost)
								, fg_ThisActor(this)
								, RemoteActor.m_Hierarchy
								, RemoteActor.m_pHost->m_HostInfo.m_UniqueHostID
								, RemoteActor.m_pHost->f_GetHostInfo()
								, RemoteActor.m_ProtocolVersions
							)
						)
					;
				}
			}
		;

		if (_Namespace.f_IsEmpty())
		{
			for (auto &RemoteNamespace : Internal.m_RemoteNamespaces)
				fReportExistingNamespace(RemoteNamespace);
		}
		else
		{
			if (auto *pRemoteNamespace = Internal.m_RemoteNamespaces.f_FindEqual(_Namespace))
				fReportExistingNamespace(*pRemoteNamespace);
		}

		co_await ReportResults.f_GetResults();

		co_return g_ActorSubscription / [this, _Namespace, SubscriptionID]
			{
				auto &Internal = *mp_pInternal;

				auto pSubscribed = Internal.m_SubscribedActors.f_FindEqual(_Namespace);
				if (!pSubscribed)
					return;

				if (!pSubscribed->m_Instances.f_Remove(SubscriptionID))
					return;

				if (pSubscribed->m_Instances.f_IsEmpty())
					Internal.m_SubscribedActors.f_Remove(pSubscribed);
			}
		;
	}
}
