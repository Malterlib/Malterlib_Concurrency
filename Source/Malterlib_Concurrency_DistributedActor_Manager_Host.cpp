// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		namespace NActorDistributionManagerInternal
		{
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
			auto *pHost = Internal.m_Hosts.f_FindEqual(_HostID);
			if (!pHost)
			{
				// Already gone
				return fg_Explicit();
			}
			
			auto &Host = **pHost;

			TCActorResultVector<void> Results;
			for (auto &ClientConnection : Host.m_ClientConnections)
				ClientConnection.f_Disconnect() > Results.f_AddResult();
			
			for (auto &ServerConnection : Host.m_ServerConnections)
				ServerConnection.f_Disconnect() > Results.f_AddResult();
			
			Internal.fp_DestroyHost(**pHost, nullptr);

			TCContinuation<void> Continuation;
			Results.f_GetResults() > fg_ConcurrentActor() / [Continuation](auto &&_Results)
				{
					// Ignore errors
					Continuation.f_SetResult();
				}
			;
			
			return Continuation;
		}

		void CActorDistributionManager::CInternal::fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection)
		{
			auto &Host = _Host;
			
			for (auto iClientConnection = Host.m_ClientConnections.f_GetIterator(); iClientConnection; )
			{
				auto &Connection = *iClientConnection;
				++iClientConnection;
				
				if (&Connection == _pSaveConnection)
					continue;
				
				fp_DestroyClientConnection(Connection, true, "Reset host state");
			}
			
			for (auto iServerConnection = Host.m_ServerConnections.f_GetIterator(); iServerConnection; )
			{
				auto &Connection = *iServerConnection;
				++iServerConnection;
				
				if (&Connection == _pSaveConnection)
					continue;
				
				fp_DestroyServerConnection(Connection, true, "Reset host state");
			}
			
			DMibCheck(Host.m_ActiveConnections.f_IsEmpty());

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
			
			if (_pSaveConnection)
			{
				Host.m_bIncoming = _pSaveConnection->m_bIncoming;
				Host.m_bOutgoing = !_pSaveConnection->m_bIncoming;
			}
			else
			{
				Host.m_bIncoming = false;
				Host.m_bOutgoing = false;
			}
			Host.m_Incoming_NextPacketID = 1;

			Host.m_Outgoing_CurrentPacketID = 0;
			
			for (auto &Call : Host.m_OutstandingCalls)
				Call.f_SetException(DMibErrorInstance("Remote host no longer running"));
			Host.m_OutstandingCalls.f_Clear();
			
			Host.f_DeletePackets();
		}
		
		void CActorDistributionManager::CInternal::fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection)
		{
			fp_ResetHostState(_Host, _pSaveConnection);

			_Host.f_Destroy();
			m_Hosts.f_Remove(_Host.m_UniqueHostID);
		}
	}
}
