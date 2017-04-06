// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		namespace NActorDistributionManagerInternal
		{
			CHost::CHost(CActorDistributionManager &_DistributionManager)
				: m_OnDisconnect(&_DistributionManager, false)
			{
			}

			CHost::~CHost()
			{
				f_DeletePackets();
			}
			
			bool CHost::f_CanReceivePublish() const
			{
				return !m_bIncoming || !m_bAnonymous; 
			}
			
			bool CHost::f_CanSendPublish() const
			{
				return m_bIncoming || !m_bAnonymous;
			}
			
			CHostInfo CHost::f_GetHostInfo() const
			{
				CHostInfo Info;
				Info.m_HostID = m_RealHostID;
				Info.m_FriendlyName = m_FriendlyName;
				return Info;
			}

			void CHost::f_Destroy()
			{
				DMibFastCheck(m_ClientConnections.f_IsEmpty());
				DMibFastCheck(m_ServerConnections.f_IsEmpty());
				DMibFastCheck(m_OutstandingCalls.f_IsEmpty());

				m_bDeleted = true;
				
				f_DeletePackets();
			}
			
			void CHost::f_DeletePackets()
			{
				DMibFastCheck(m_RemoteActors.f_IsEmpty());
				m_Incoming_ReceivedPackets.f_DeleteAll();
				m_Outgoing_QueuedPackets.f_DeleteAll();
				m_Outgoing_SentPackets.f_DeleteAll();
			}
		}
		
		TCContinuation<void> CActorDistributionManager::f_KickHost(NStr::CStr const &_HostID)
		{
			auto &Internal = *mp_pInternal;
			
			auto *pHosts = Internal.m_HostsByRealHostID.f_FindEqual(_HostID);
			if (!pHosts)
				return fg_Explicit();
			
			TCActorResultVector<void> Results;
			
			for (auto iHost = pHosts->f_GetIterator(); iHost;)
			{
				auto &Host = *iHost;
				++iHost;
				for (auto &ClientConnection : Host.m_ClientConnections)
					ClientConnection.f_Disconnect() > Results.f_AddResult();
				
				for (auto &ServerConnection : Host.m_ServerConnections)
					ServerConnection.f_Disconnect() > Results.f_AddResult();
				
				Internal.fp_DestroyHost(Host, nullptr);
			}

			TCContinuation<void> Continuation;
			Results.f_GetResults() > fg_ConcurrentActor() / [Continuation](auto &&_Results)
				{
					// Ignore errors
					Continuation.f_SetResult();
				}
			;
			return Continuation;
		}

		void CActorDistributionManagerInternal::fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection, bool _bSaveInactive)
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
				
				fp_DestroyClientConnection(Connection, true, "Reset host state");
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
				
				fp_DestroyServerConnection(Connection, true, "Reset host state");
			}
			
			DMibCheck(Host.m_ActiveConnections.f_IsEmpty());
			
			Host.m_OnDisconnect();
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
			
			for (auto &Call : Host.m_OutstandingCalls)
				Call.m_Continuation.f_SetException(DMibErrorInstance("Remote host no longer running"));
			Host.m_OutstandingCalls.f_Clear();
			
 			Host.m_ImplicitlyPublishedActors.f_Clear();
			Host.m_ImplicitlyPublishedInterfaces.f_Clear();
			Host.m_ImplicitlyPublishedFunctions.f_Clear();
			Host.m_ImplicitlyPublishedSubscriptions.f_Clear();
			Host.m_RemoteSubscriptionReferences.f_Clear();
			Host.m_LocalSubscriptionReferences.f_Clear();
			
			for (auto &Destroy : Host.m_PendingRemoteSubscriptionDestroys)
				Destroy.f_SetResult();
			Host.m_PendingRemoteSubscriptionDestroys.f_Clear();
			
			Host.f_DeletePackets();
		}
		
		void CActorDistributionManagerInternal::fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection)
		{
			fp_ResetHostState(_Host, _pSaveConnection, false);
			_Host.f_Destroy();
			if (_Host.m_RealHostsLink.f_IsAloneInList())
				m_HostsByRealHostID.f_Remove(_Host.m_RealHostID);
			else
				_Host.m_RealHostsLink.f_Unlink();
			m_Hosts.f_Remove(_Host.m_UniqueHostID);
		}
	}
}
