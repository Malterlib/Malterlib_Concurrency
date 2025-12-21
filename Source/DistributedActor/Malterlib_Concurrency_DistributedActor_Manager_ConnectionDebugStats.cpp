// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Network/Sockets/SSL>

namespace NMib::NConcurrency
{
	using namespace NActorDistributionManagerInternal;

	auto CActorDistributionManager::f_GetConnectionsDebugStats() -> TCFuture<CConnectionsDebugStats>
	{
		CConnectionsDebugStats Stats;

		auto &Internal = *mp_pInternal;

		NContainer::TCSet<NStr::CStr> HostIDs;

		for (auto &pHost : Internal.m_Hosts)
			HostIDs[Internal.m_Hosts.fs_GetKey(pHost)];

		for (auto &HostID : HostIDs)
		{
			auto *pHostFind = Internal.m_Hosts.f_FindEqual(HostID);
			if (!pHostFind)
				continue;

			auto pHost = *pHostFind;
			auto &Host = *pHost;

			auto &OutHost = Stats.m_Hosts.f_Insert();

			auto fAddQueue = [&](auto &o_Bytes, auto &o_PacketIDs, auto &&_Container)
				{
					for (auto &Packet: _Container)
					{
						o_Bytes += Packet.m_pData->f_GetLen();
						o_PacketIDs.f_Insert(Packet.f_GetPacketID());
					}
				}
			;

			OutHost.m_HostID = Host.m_HostInfo.m_RealHostID;
			OutHost.m_UniqueHostID = Host.m_HostInfo.m_UniqueHostID;

			for (auto &PriorityQueuesEntry : Host.m_PriorityQueues.f_Entries())
			{
				auto Priority = PriorityQueuesEntry.f_Key();
				auto &PriorityQueues = PriorityQueuesEntry.f_Value();
				auto &OutQueues = OutHost.m_PriorityQueues[Priority];
				OutQueues.m_Incoming_NextPacketID = PriorityQueues.m_IncomingNextPacketID;
				OutQueues.m_Outgoing_CurrentPacketID = PriorityQueues.m_OutgoingCurrentPacketID;

				OutQueues.m_Incoming_PacketsQueueLength += PriorityQueues.m_IncomingPackets.f_GetLen();
				fAddQueue(OutQueues.m_Incoming_PacketsQueueBytes, OutQueues.m_Incoming_PacketsQueueIDs, PriorityQueues.m_IncomingPackets);

				OutQueues.m_Outgoing_PacketsQueueLength += PriorityQueues.m_OutgoingPackets.f_GetLen();
				fAddQueue(OutQueues.m_Outgoing_PacketsQueueBytes, OutQueues.m_Outgoing_PacketsQueueIDs, PriorityQueues.m_OutgoingPackets);
			}

			for (auto &Packet : Host.m_Outgoing_SentPackets)
			{
				auto Priority = Packet.m_Priority;
				auto &OutQueues = OutHost.m_PriorityQueues[Priority];
				++OutQueues.m_Outgoing_SentPacketsQueueLength;
				OutQueues.m_Outgoing_SentPacketsQueueBytes += Packet.m_pData->f_GetLen();
				OutQueues.m_Outgoing_SentPacketsQueueIDs.f_Insert(Packet.f_GetPacketID());
			}

			OutHost.m_nSentPackets = Host.m_nSentPackets;
			OutHost.m_nSentBytes = Host.m_nSentBytes;

			OutHost.m_nReceivedPackets = Host.m_nReceivedPackets;
			OutHost.m_nReceivedBytes = Host.m_nReceivedBytes;

			OutHost.m_nDiscardedPackets = Host.m_nDiscardedPackets;
			OutHost.m_nDiscardedBytes = Host.m_nDiscardedBytes;

			OutHost.m_LastExecutionID = Host.m_LastExecutionID;
			OutHost.m_ExecutionID = Host.m_ExecutionID;
			OutHost.m_FriendlyName = Host.m_FriendlyName;
			OutHost.m_LastError = Host.m_LastError;
			OutHost.m_LastErrorTime = Host.m_LastErrorTime;

			TCFutureMap<mint, NWeb::CWebSocketActor::CDebugStats> WebsocketDebugStats;

			auto fAddConnection = [&](CConnection const &_Connection) -> CActorDistributionManager::CConnectionDebugStats &
				{
					mint iConnection = OutHost.m_Connections.f_GetLen();
					auto &OutConnection = OutHost.m_Connections.f_Insert();
					OutConnection.m_bIncoming = _Connection.m_bIncoming;
					OutConnection.m_bIdentified = _Connection.m_bIdentified;
					if (_Connection.m_Connection)
						_Connection.m_Connection(&NWeb::CWebSocketActor::f_DebugGetStats) > WebsocketDebugStats[iConnection];
					return OutConnection;
				}
			;

			for (auto &ClientConnection : Host.m_ClientConnections)
			{
				auto &OutConnection = fAddConnection(ClientConnection);
				OutConnection.m_URL = ClientConnection.m_ServerURL;
			}

			for (auto &ServerConnection : Host.m_ServerConnections)
			{
				auto &OutConnection = fAddConnection(ServerConnection);
				if (auto pListen = Internal.m_Listens.f_FindEqual(ServerConnection.m_ListenID); pListen && !pListen->m_ListenAddresses.f_IsEmpty())
					OutConnection.m_URL = pListen->m_ListenAddresses.f_GetFirst();
			}

			auto DebugStats = co_await fg_AllDone(WebsocketDebugStats);

			for (auto &Stats : DebugStats)
			{
				auto iConnection = DebugStats.fs_GetKey(Stats);
				auto &OutStats = OutHost.m_Connections[iConnection].m_WebsocketStats;
				OutStats.m_nSentBytes = Stats.m_nSentBytes;
				OutStats.m_nReceivedBytes = Stats.m_nReceivedBytes;
				OutStats.m_IncomingDataBufferBytes = Stats.m_IncomingDataBufferBytes;
				OutStats.m_OutgoingDataBufferBytes = Stats.m_OutgoingDataBufferBytes;
				OutStats.m_SecondsSinceLastSend = Stats.m_SecondsSinceLastSend;
				OutStats.m_SecondsSinceLastReceive = Stats.m_SecondsSinceLastReceive;
				OutStats.m_State = Stats.m_State;
			}
		}

		co_return fg_Move(Stats);
	}
}
