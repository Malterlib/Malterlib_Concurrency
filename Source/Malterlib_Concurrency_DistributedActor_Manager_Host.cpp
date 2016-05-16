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
				f_Clear();
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
				
				f_Clear();
			}
			
			void CHost::f_Clear()
			{
				DMibFastCheck(m_RemoteActors.f_IsEmpty());
				
				m_ActiveConnections.f_Clear();
				m_OutstandingCalls.f_Clear();
				m_RemoteActors.f_Clear();
				m_ClientConnections.f_Clear();
				m_ServerConnections.f_Clear();
				m_AllowedNamespaces.f_Clear();
				m_Incoming_ReceivedPackets.f_DeleteAll();
				m_Incoming_QueuedPackets.f_DeleteAll();
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
			
			Internal.fp_DestroyHost(**pHost, nullptr);
			
			return fg_Explicit();
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
				
				fp_DestroyClientConnection(Connection, true);
			}
			Host.m_ClientConnections.f_Clear();
			
			for (auto iServerConnection = Host.m_ServerConnections.f_GetIterator(); iServerConnection; )
			{
				auto &Connection = *iServerConnection;
				++iServerConnection;
				
				if (&Connection == _pSaveConnection)
					continue;
				
				fp_DestroyServerConnection(Connection, true);
			}
			Host.m_ServerConnections.f_Clear();

			for (auto &RemoteActor : Host.m_RemoteActors)
			{
				if (RemoteActor.m_Link.f_IsInList())
				{
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
			Host.m_Incoming_ReceivedPacketID = 1;
			Host.m_Incoming_AckedPacketID = 0;

			Host.m_Outgoing_CurrentPacketID = 0;
			Host.m_Outgoing_AckedPacketID = 0;
			
			for (auto &Call : Host.m_OutstandingCalls)
				Call.f_SetException(DMibErrorInstance("Remote host no longer running"));
			Host.m_OutstandingCalls.f_Clear();
			
			Host.f_Clear();
		}
		
		void CActorDistributionManager::CInternal::fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection)
		{
			fp_ResetHostState(_Host, _pSaveConnection);

			_Host.f_Destroy();
			m_Hosts.f_Remove(_Host.m_UniqueHostID);
		}
	}
}
