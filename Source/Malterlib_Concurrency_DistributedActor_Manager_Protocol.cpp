// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib
{
	namespace NConcurrency
	{
		bool CActorDistributionManagerInternal::fp_HandleProtocolIncoming
			(
				CConnection *_pConnection
				, NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage
			)
		{
			mint Length = _pMessage->f_GetLen();
			if (Length < 1)
			{
				DMibLog(DebugVerbose2, " ---- {} {} Too small", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
				return false;
			}
			
			NStream::CBinaryStreamMemoryPtr<> Stream;
			Stream.f_OpenRead(_pMessage->f_GetArray(), _pMessage->f_GetLen());
			uint8 Command;
			Stream >> Command;
			
			try
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				switch (Command)
				{
				case EDistributedActorCommand_Identify:
					{
						CDistributedActorCommand_Identify Identify;
						Stream >> Identify;
						
						if (Identify.m_ProtocolVersion < 0x101)
						{
							DMibLog(DebugVerbose2, " ---- {} {} Invalid protocol", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
							if (!_pConnection->m_IdentifyContinuation.f_IsSet())
								_pConnection->m_IdentifyContinuation.f_SetException(DMibErrorInstance("Invalid protocol version"));
							return false;
						}
						
						auto &pHost = _pConnection->m_pHost;
						DMibFastCheck(pHost);

						// Remove packets that remote already knows about
						for (auto iPacket = pHost->m_Outgoing_SentPackets.f_GetIterator(); iPacket && iPacket->f_GetPacketID() <= Identify.m_AckedPacketID; ++iPacket)
							iPacket.f_Delete(pHost->m_Outgoing_SentPackets);
						 
						bool bShouldReset = false;
						
						if (pHost->m_LastExecutionID != Identify.m_ExecutionID)
							bShouldReset = true;
						
#if 0
						// This seems to not be needed
						if (!Identify.m_LastSeenExecutionID.f_IsEmpty() && Identify.m_LastSeenExecutionID != m_ExecutionID)
						{
							bShouldReset = true;
						}
#endif

						if (bShouldReset && !pHost->m_LastExecutionID.f_IsEmpty())
							fp_ResetHostState(*pHost, _pConnection, true);
						
						pHost->m_LastExecutionID = Identify.m_ExecutionID;
						
						pHost->m_bAllowAllNamespaces = Identify.m_bAllowAllNamespaces;
						pHost->m_AllowedNamespaces = Identify.m_AllowedNamespaces;
						
						if (!Identify.m_FriendlyName.f_IsEmpty() && Identify.m_FriendlyName != pHost->m_FriendlyName)
						{
							pHost->m_FriendlyName = Identify.m_FriendlyName;
							CHostInfo HostInfo;
							HostInfo.m_HostID = pHost->m_RealHostID;
							HostInfo.m_FriendlyName = pHost->m_FriendlyName; 
							m_OnHostInfoChanged(HostInfo);
						}
						
						// Resend packets that remote think are missing
						for (auto &MissingPacketID : Identify.m_MissingPacketIDs)
						{
							auto pPacket = pHost->m_Outgoing_SentPackets.f_FindEqual(MissingPacketID);
							if (pPacket)
								fp_SendPacket(_pConnection, fg_TempCopy(pPacket->m_pData));
						}
						
						{
							decltype(pHost->m_Outgoing_SentPackets)::CIteratorConst iSentPackets;
							iSentPackets.f_InitForSearch(pHost->m_Outgoing_SentPackets);
							iSentPackets.f_FindSmallestGreaterThanEqualForward(Identify.m_HighestSeenPacketID);
							
							while (iSentPackets)
							{
								fp_SendPacket(_pConnection, fg_TempCopy(iSentPackets->m_pData));
								++iSentPackets;
							}
						}
						
						DMibLog(DebugVerbose2, " ---- {} {} Identify", pHost->m_bIncoming, _pConnection->f_GetConnectionID());

						pHost->m_ActiveConnections.f_Insert(*_pConnection);
						fp_SendPacketQueue(pHost);
						fp_ProcessPacketQueue(_pConnection);
						
						if (pHost->f_CanSendPublish())
						{
							for (auto &NamespaceActors : m_LocalNamespaces)
							{
								auto &Namespace = NamespaceActors.f_GetNamespace();
								if (!pHost->m_bAllowAllNamespaces && !pHost->m_AllowedNamespaces.f_FindEqual(Namespace))
									continue;
								if (pHost->m_bAnonymous && !fp_NamespaceAllowedForAnonymous(Namespace))
									continue;
								
								for (auto &Actor : NamespaceActors.m_Actors)
								{
									CDistributedActorCommand_Publish Publish;
									Publish.m_ActorID = Actor.f_GetActorID();
									Publish.m_Namespace = Namespace;
									Publish.m_Hierarchy = Actor.m_Hierarchy;
									Publish.m_ProtocolVersions = Actor.m_ProtocolVersions;

									NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
									Stream << Publish;
									auto Data = Stream.f_MoveVector();

									fp_QueuePacket(pHost, fg_TempCopy(Data));
								}
							}
						}
						
						if (!_pConnection->m_IdentifyContinuation.f_IsSet())
							_pConnection->m_IdentifyContinuation.f_SetResult();
					}
					break;
				case EDistributedActorCommand_Acknowledge:
					{
						if (!_pConnection->m_Link.f_IsInList()) // Not an active connection
						{
							DMibLog(DebugVerbose2, " ---- {} {} Not active ack", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
							return false;
						}
						
						CDistributedActorCommand_Acknowledge Acknowledge;
						Stream >> Acknowledge;

						auto &pHost = _pConnection->m_pHost;
						
						// Remove packets that remote already knows about
						for (auto iPacket = pHost->m_Outgoing_SentPackets.f_GetIterator(); iPacket && iPacket->f_GetPacketID() <= Acknowledge.m_LastInOrderPacketID; ++iPacket)
							iPacket.f_Delete(pHost->m_Outgoing_SentPackets);
					}
					break;
				case EDistributedActorCommand_RemoteCall:
				case EDistributedActorCommand_RemoteCallResult:
				case EDistributedActorCommand_Publish:
				case EDistributedActorCommand_Unpublish:
				case EDistributedActorCommand_DestroySubscription: 
					{
						auto &pHost = _pConnection->m_pHost;
						
						uint64 PacketID;
						Stream >> PacketID;
						
						if (PacketID < pHost->m_Incoming_NextPacketID)
						{
							DMibLog(DebugVerbose2, " ---- {} {} IGNORING PACKET {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID);
							return true; // Already received
						}
						
						auto iPacket = pHost->m_Incoming_ReceivedPackets.f_GetIterator();
						iPacket.f_Reverse(pHost->m_Incoming_ReceivedPackets);
						for (; iPacket; --iPacket)
						{
							if (iPacket->f_GetPacketID() == PacketID)
							{
								DMibLog(DebugVerbose2, " ---- {} {} ALREADY RECEIVED PACKET {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID);
								return true; // Already received
							}
							else if (iPacket->f_GetPacketID() < PacketID)
							{
								DMibLog(DebugVerbose2, " ---- {} {} Receive packet {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID);
								NPtr::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage);
								pHost->m_Incoming_ReceivedPackets.f_InsertAfter(pPacket.f_Detach(), &*iPacket);
								break;
							}
						}
						if (!iPacket)
						{
							NPtr::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage);
							DMibLog(DebugVerbose2, " ---- {} {} Receive2 packet {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID);
							pHost->m_Incoming_ReceivedPackets.f_InsertFirst(pPacket.f_Detach());
						}
						if (_pConnection->m_Link.f_IsInList()) // Only active connections
							fp_ProcessPacketQueue(_pConnection);
						else
							DMibLog(DebugVerbose2, " ---- {} {} NOT ACTIVE {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID);
							
					}
					break;
				default:
					DMibLog(DebugVerbose2, " ---- {} {} Invalid packet command", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
					return false;
				}
			}
			catch (NException::CException const &_Exception)
			{
				DMibLog(DebugVerbose2, " ---- {} {} Exception processing data {}", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID(), _Exception.f_GetErrorStr());
				// Malicious client
				return false;
			}
			
			return true;
		}
		
		void CActorDistributionManagerInternal::fp_Identify(CConnection *_pConnection)
		{
			auto &pHost = _pConnection->m_pHost;
			CDistributedActorCommand_Identify Identify;
			Identify.m_FriendlyName = m_FriendlyName;
			Identify.m_ExecutionID = m_ExecutionID;
			Identify.m_LastSeenExecutionID = pHost->m_LastExecutionID;
			Identify.m_AckedPacketID = pHost->m_Incoming_NextPacketID - 1;
			Identify.m_HighestSeenPacketID = Identify.m_AckedPacketID; 
			uint64 NextExpectedPacketID = pHost->m_Incoming_NextPacketID; 
			for (auto &ReceivedPacket : pHost->m_Incoming_ReceivedPackets)
			{
				uint64 PacketID = ReceivedPacket.f_GetPacketID();
				while (NextExpectedPacketID < PacketID)
				{
					Identify.m_MissingPacketIDs.f_Insert(NextExpectedPacketID);
					++NextExpectedPacketID;
				}
				Identify.m_HighestSeenPacketID = fg_Max(Identify.m_HighestSeenPacketID, PacketID);
			}
			
			bool bAllowAllNamespaces = false;
			auto &AllowedNamespaces = fp_GetAllowedNamespacesForHost(pHost, bAllowAllNamespaces);
			
			Identify.m_bAllowAllNamespaces = bAllowAllNamespaces; 
			if (!bAllowAllNamespaces)
				Identify.m_AllowedNamespaces = AllowedNamespaces; 
			
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Identify;
			
			NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> pPacketData = fg_Construct(Stream.f_MoveVector());
			
			fp_SendPacket(_pConnection, fg_Move(pPacketData));
		}
	}
}
