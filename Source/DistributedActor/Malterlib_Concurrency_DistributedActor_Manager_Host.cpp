// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

#include <Mib/Network/Sockets/SSL>

namespace NMib::NConcurrency
{
	namespace NActorDistributionManagerInternal
	{
		CHost::CHost(CActorDistributionManager &_DistributionManager, CDistributedActorHostInfo &&_Info)
			: ICHost(fg_Move(_Info))
		{
		}

		CHost::~CHost()
		{
			f_DeletePackets();
		}

		CHost::COnDisconnect::~COnDisconnect()
		{
			*this->m_pDestroyed = true;
		}

		bool CHost::f_CanReceivePublish() const
		{
			return !m_bIncoming || !m_HostInfo.m_bAnonymous;
		}

		bool CHost::f_CanSendPublish() const
		{
			return m_bIncoming || !m_HostInfo.m_bAnonymous;
		}

		bool CHost::f_IsDaemon() const
		{
			return m_HostInfo.m_UniqueHostID == m_HostInfo.m_RealHostID;
		}

		CHostInfo CHost::f_GetHostInfo() const
		{
			CHostInfo Info;
			Info.m_HostID = m_HostInfo.m_RealHostID;
			Info.m_FriendlyName = m_FriendlyName;
			return Info;
		}

		void CHost::f_Destroy()
		{
			DMibFastCheck(m_ClientConnections.f_IsEmpty());
			DMibFastCheck(m_ServerConnections.f_IsEmpty());
			DMibFastCheck(m_OutstandingCalls.f_IsEmpty());

			m_bDeleted.f_Store(true, NAtomic::EMemoryOrder_Relaxed);

			f_DeletePackets();
		}

		void CHost::f_DeletePackets()
		{
			DMibFastCheck(m_RemoteActors.f_IsEmpty());
			m_Incoming_ReceivedPackets.f_DeleteAllDefiniteType();
			m_Outgoing_QueuedPackets.f_DeleteAllDefiniteType();
			m_Outgoing_SentPackets.f_DeleteAllDefiniteType();
		}
	}

	TCFuture<void> CActorDistributionManager::f_KickHost(NStr::CStr const &_HostID)
	{
		TCPromise<void> Promise;

		auto &Internal = *mp_pInternal;

		auto *pHosts = Internal.m_HostsByRealHostID.f_FindEqual(_HostID);
		if (!pHosts)
			return Promise <<= g_Void;

		TCActorResultVector<void> Results;

		for (auto iHost = pHosts->f_GetIterator(); iHost;)
		{
			auto &Host = *iHost;
			++iHost;
			for (auto &ClientConnection : Host.m_ClientConnections)
				ClientConnection.f_Disconnect() > Results.f_AddResult();

			for (auto &ServerConnection : Host.m_ServerConnections)
				ServerConnection.f_Disconnect() > Results.f_AddResult();

			Internal.fp_DestroyHost(Host, nullptr, "Host was kicked");
		}

		Results.f_GetResults() > fg_DirectCallActor() / [Promise](auto &&_Results)
			{
				// Ignore errors
				Promise.f_SetResult();
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<CActorDistributionManager::CHostState> CActorDistributionManager::f_GetHostState(NStr::CStr const &_UniqueHostID)
	{
		auto &Internal = *mp_pInternal;

		auto *pHost = Internal.m_Hosts.f_FindEqual(_UniqueHostID);
		if (!pHost)
			co_return {};

		auto &Host = **pHost;

		CHostState ReturnState;

		ReturnState.m_bActive = !Host.m_ActiveConnections.f_IsEmpty();
		ReturnState.m_LastConnectionError = Host.m_LastError;
		ReturnState.m_LastConnectionErrorTime = Host.m_LastErrorTime;

		co_return fg_Move(ReturnState);
	}

	void CActorDistributionManagerInternal::fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection, bool _bSaveInactive, NStr::CStr const &_Error)
	{
		auto &Host = _Host;

		bool bIncoming = false;
		bool bOutgoing = false;

		for (auto iClientConnection = Host.m_ClientConnections.f_GetIterator(); iClientConnection; )
		{
			auto &Connection = *iClientConnection;
			++iClientConnection;

			if (&Connection == _pSaveConnection || (_bSaveInactive && !Connection.m_Link.f_IsInList()))
			{
				if (Connection.m_bIncoming)
					bIncoming = true;
				else
					bOutgoing = true;
				continue;
			}

			if (Host.m_HostInfo.m_bAnonymous || m_bPreShutdown)
				fp_DestroyClientConnection(Connection, true, "Reset host state", false, nullptr);
			else
				fp_ScheduleReconnect(fg_Explicit(&Connection), true, Connection.m_ConnectionSequence, "Reset host state", true);
		}

		for (auto iServerConnection = Host.m_ServerConnections.f_GetIterator(); iServerConnection; )
		{
			auto &Connection = *iServerConnection;
			++iServerConnection;

			if (&Connection == _pSaveConnection || (_bSaveInactive && !Connection.m_Link.f_IsInList()))
			{
				if (Connection.m_bIncoming)
					bIncoming = true;
				else
					bOutgoing = true;
				continue;
			}

			fp_DestroyServerConnection(Connection, true, "Reset host state", false);
		}

		DMibCheck(Host.m_ActiveConnections.f_IsEmpty());

		for (auto &OnDisconnect : Host.m_OnDisconnect)
		{
			OnDisconnect.m_fOnDisconnect() > fg_DiscardResult();
			fg_Move(OnDisconnect.m_fOnDisconnect).f_Destroy() > fg_DiscardResult();
		}
		Host.m_OnDisconnect.f_Clear();

		for (auto &RemoteActor : Host.m_RemoteActors)
		{
			if (RemoteActor.m_Link.f_IsInList())
			{
				fp_NotifyRemovedActor(RemoteActor);

				auto pRemoteNamespace = m_RemoteNamespaces.f_FindEqual(RemoteActor.m_Namespace);
				DMibFastCheck(pRemoteNamespace);
				pRemoteNamespace->m_RemoteActors.f_Remove(RemoteActor);
				if (pRemoteNamespace->m_RemoteActors.f_IsEmpty())
					m_RemoteNamespaces.f_Remove(pRemoteNamespace);
			}
		}
		Host.m_RemoteActors.f_Clear();

		Host.m_pLastSendConnection = nullptr;

		Host.m_bAllowAllNamespaces = false;
		Host.m_AllowedNamespaces.f_Clear();

		Host.m_bIncoming = bIncoming;
		Host.m_bOutgoing = bOutgoing;

		Host.m_Incoming_NextPacketID = 1;

		Host.m_Outgoing_CurrentPacketID = 0;

		using namespace NStr;

		for (auto &Call : Host.m_OutstandingCalls)
			Call.m_Promise.f_SetException(DMibErrorInstance("Remote host '{}' no longer running: {}"_f << Host.f_GetHostInfo() << _Error));
		Host.m_OutstandingCalls.f_Clear();

		{
			NContainer::TCVector<CStr> ToDestroy;
			for (auto &Subscription : Host.m_LocalSubscriptionReferences)
				ToDestroy.f_Insert(Host.m_LocalSubscriptionReferences.fs_GetKey(Subscription));

			for (auto &SubscriptionID : ToDestroy)
				fp_DestroyLocalSubscription(Host, SubscriptionID);
		}
		{
			NContainer::TCVector<CStr> ToDestroy;
			for (auto &Subscription : Host.m_RemoteSubscriptionReferences)
				ToDestroy.f_Insert(Host.m_RemoteSubscriptionReferences.fs_GetKey(Subscription));

			for (auto &SubscriptionID : ToDestroy)
				fp_DestroyRemoteSubscriptionReset(Host, SubscriptionID);
		}

		Host.m_RemoteSubscriptionReferences.f_Clear();
		Host.m_LocalSubscriptionReferences.f_Clear();

		Host.m_ImplicitlyPublishedInterfaces.f_Clear();
		Host.m_ImplicitlyPublishedFunctions.f_Clear();
		Host.m_ImplicitlyPublishedSubscriptions.f_Clear();
		Host.m_ImplicitlyPublishedActors.f_Clear();

		for (auto &Destroy : Host.m_PendingRemoteSubscriptionDestroys)
			Destroy.f_SetResult();
		Host.m_PendingRemoteSubscriptionDestroys.f_Clear();

		Host.m_AuthenticationHandler.f_Clear();

		Host.f_DeletePackets();
	}

	void CActorDistributionManagerInternal::fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection, NStr::CStr const &_Reason)
	{
		CHostInfo HostInfo;
		HostInfo.m_HostID = _Host.m_HostInfo.m_RealHostID;
		HostInfo.m_FriendlyName = _Host.m_FriendlyName;

		DMibLogWithCategory(Mib/Concurrency/Actors, Info, "<{}> Destroyed host: {}", HostInfo, _Reason);

		fp_ResetHostState(_Host, _pSaveConnection, false, _Reason);
		_Host.f_Destroy();
		if (_Host.m_RealHostsLink.f_IsAloneInList())
			m_HostsByRealHostID.f_Remove(_Host.m_HostInfo.m_RealHostID);
		else
			_Host.m_RealHostsLink.f_Unlink();

		_Host.m_InactiveHostsLink.f_Unlink();

		m_Hosts.f_Remove(_Host.m_HostInfo.m_UniqueHostID);
	}
}
