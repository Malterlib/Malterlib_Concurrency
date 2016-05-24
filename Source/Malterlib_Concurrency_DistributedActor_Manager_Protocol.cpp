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
		bool CActorDistributionManager::CInternal::fp_HandleProtocolIncoming
			(
				CConnection *_pConnection
				, NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage
			)
		{
			mint Length = _pMessage->f_GetLen();
			if (Length < 1)
				return false;
			
			NStream::CBinaryStreamMemoryPtr<> Stream;
			Stream.f_OpenRead(_pMessage->f_GetArray(), _pMessage->f_GetLen());
			uint8 Command;
			Stream >> Command;
			
			try
			{
				switch (Command)
				{
				case EDistributedActorCommand_Identify:
					{
						CDistributedActorCommand_Identify Identify;
						Stream >> Identify;
						
						if (Identify.m_ProtocolVersion < 0x101 || Identify.m_ProtocolVersion > CDistributedActorCommand_Identify::EProtocolVersion)
							return false;
						
						auto &pHost = _pConnection->m_pHost;
						DMibFastCheck(pHost);
						
						if (pHost->m_LastExecutionID != Identify.m_ExecutionID || (!Identify.m_LastSeenExecutionID.f_IsEmpty() && Identify.m_LastSeenExecutionID != m_ExecutionID))
						{
							if (!pHost->m_LastExecutionID.f_IsEmpty())
								fp_ResetHostState(*pHost, _pConnection);
							pHost->m_LastExecutionID = Identify.m_ExecutionID;
						}
						
						// Remove packets that remote already knows about
						for (auto iPacket = pHost->m_Outgoing_SentPackets.f_GetIterator(); iPacket && iPacket->f_GetPacketID() <= Identify.m_HighestSeenPacketID; ++iPacket)
							iPacket.f_Delete(pHost->m_Outgoing_SentPackets);
						pHost->m_Outgoing_AckedPacketID = fg_Max(Identify.m_HighestSeenPacketID, pHost->m_Outgoing_AckedPacketID);
						
						pHost->m_bAllowAllNamespaces = Identify.m_bAllowAllNamespaces;
						pHost->m_AllowedNamespaces = Identify.m_AllowedNamespaces;

						// Resend packets that remote think are missing
						for (auto &MissingPacketID : Identify.m_MissingPacketIDs)
						{
							auto pPacket = pHost->m_Outgoing_SentPackets.f_FindEqual(MissingPacketID);
							if (pPacket)
							{
								_pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, pPacket->m_pData, 0) 
									> [](TCAsyncResult<void> &&_Result)
									{
										if (!_Result)
										{
											// TODO: Early reschedule here by deleting the connection?
										}						
									}
								;
							}
						}

						pHost->m_ActiveConnections.f_Insert(*_pConnection);
						
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

									NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
									Stream << Publish;
									auto Data = Stream.f_MoveVector();

									fp_QueuePacket(pHost, fg_TempCopy(Data));
								}
							}
						}
					}
					break;
				case EDistributedActorCommand_Acknowledge:
					{
						if (!_pConnection->m_Link.f_IsInList()) // Not an active connection
							return false;
						
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
					{
						if (!_pConnection->m_Link.f_IsInList()) // Not an active connection
							return false;
						
						uint64 PacketID;
						Stream >> PacketID;
						
						NPtr::TCUniquePointer<CPacket> pPacket = fg_Construct();
						pPacket->m_pData = _pMessage;
						auto &pHost = _pConnection->m_pHost;
						auto iPacket = pHost->m_Incoming_ReceivedPackets.f_GetIterator();
						iPacket.f_Reverse(pHost->m_Incoming_ReceivedPackets);
						for (; iPacket; --iPacket)
						{
							if (iPacket->f_GetPacketID() < PacketID)
							{
								pHost->m_Incoming_ReceivedPackets.f_InsertAfter(pPacket.f_Detach(), &*iPacket);
								break;
							}
						}
						if (!iPacket)
							pHost->m_Incoming_ReceivedPackets.f_InsertFirst(pPacket.f_Detach());
						fp_ProcessPacketQueue(_pConnection);
					}
					break;
				default:
					return false;
				}
			}
			catch (NException::CException const &)
			{
				// Malicious client
				return false;
			}
			
			return true;
		}
		
		void CActorDistributionManager::CInternal::fp_Identify(CConnection *_pConnection)
		{
			auto &pHost = _pConnection->m_pHost;
			CDistributedActorCommand_Identify Identify;
			Identify.m_ExecutionID = m_ExecutionID;
			Identify.m_LastSeenExecutionID = pHost->m_LastExecutionID;
			for (auto &SentPacket : pHost->m_Outgoing_SentPackets)
			{
				auto PacketID = SentPacket.f_GetPacketID();
				Identify.m_MissingPacketIDs.f_Insert(PacketID);
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

			_pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, fg_Move(pPacketData), 0) 
				> [](TCAsyncResult<void> &&_Result)
				{
					if (!_Result)
					{
						// TODO: Early reschedule here by deleting the connection?
					}						
				}
			;
		}
	}
}
