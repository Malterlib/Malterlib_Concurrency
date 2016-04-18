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
		uint64 CActorDistributionManager::CInternal::CPacket::f_GetPacketID() const
		{
			DMibRequire(m_pData && m_pData->f_GetLen() >= sizeof(uint64));
			uint8 const *pData = m_pData->f_GetArray() + 1;
			return fg_ByteSwapLE(*((uint64 const *)pData));
		}	
		
		void CActorDistributionManager::CInternal::fp_QueuePacket(CHost *_pHost, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_Data)
		{
			NPtr::TCUniquePointer<CInternal::CPacket> pPacket = fg_Construct();

			auto PacketID = ++_pHost->m_Outgoing_CurrentPacketID;

			{
				NStream::CBinaryStreamMemoryPtr<> Stream;
				Stream.f_OpenReadWrite(_Data.f_GetArray(), _Data.f_GetLen(), _Data.f_GetLen());
				Stream.f_AddPosition(sizeof(uint8));
				Stream << PacketID;
			}
			
			pPacket->m_pData = fg_Construct(_Data);
			_pHost->m_Outgoing_QueuedPackets.f_Insert(pPacket.f_Detach());
			fp_SendPacketQueue(_pHost);
		}
		
		void CActorDistributionManager::CInternal::fp_SendPacketQueue(CHost *_pHost)
		{
			if (_pHost->m_ActiveConnections.f_IsEmpty())
				return; // No connections to send over
			
			// Round-robin the connections
			// Packets are kept in order on the other side
			
			auto iConnection = _pHost->m_ActiveConnections.f_GetIterator();
			if (_pHost->m_pLastSendConnection)
				iConnection = _pHost->m_pLastSendConnection; 
			while (auto *pPacket = _pHost->m_Outgoing_QueuedPackets.f_GetFirst())
			{
				auto *pConnection = &*iConnection;
				
				pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, pPacket->m_pData, 0) 
					> [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
						{
							// TODO: Early reschedule here by deleting the connection?
						}						
					}
				;
				pPacket->m_Link.f_Unlink();
				pPacket->m_TreeLink.f_Construct();
				_pHost->m_Outgoing_SentPackets.f_Insert(pPacket);
				++iConnection;
				if (!iConnection)
					iConnection = _pHost->m_ActiveConnections.f_GetIterator();
			}
			
			_pHost->m_pLastSendConnection = iConnection; 
		}
		
		void CActorDistributionManager::CInternal::fp_ProcessPacketQueue(CConnection *_pConnection)
		{
			auto pHost = _pConnection->m_pHost;
			uint64 AckPacket;
			bool bAccPacket = false;
			auto *pPacket = pHost->m_Incoming_ReceivedPackets.f_GetFirst();
			while (pPacket && pPacket->f_GetPacketID() == pHost->m_Incoming_ReceivedPacketID)
			{
				AckPacket = pHost->m_Incoming_ReceivedPacketID; 
				++pHost->m_Incoming_ReceivedPacketID;
				bAccPacket = true;
				pHost->m_Incoming_ReceivedPackets.f_Remove(pPacket);
				NPtr::TCUniquePointer<CPacket> pPacketStore = fg_Explicit(pPacket);
				
				try
				{
					auto &Data = *pPacketStore->m_pData;
					NStream::CBinaryStreamMemoryPtr<> Stream;
					Stream.f_OpenRead(Data.f_GetArray(), Data.f_GetLen());
					uint8 Command;
					Stream >> Command;
					switch (Command)
					{
					case EDistributedActorCommand_RemoteCall:
						{
							if (!fp_ApplyRemoteCall(_pConnection, Stream))
							{
								// TODO: Handle malicious connection
								return;
							}
						}
						break;
					case EDistributedActorCommand_RemoteCallResult:
						{
							if (!fp_ApplyRemoteCallResult(_pConnection, Stream))
							{
								// TODO: Handle malicious connection
								return;
							}
						}
						break;
					case EDistributedActorCommand_Publish:
						{
							if (!fp_HandlePublishPacket(_pConnection, Stream))
							{
								// TODO: Handle malicious connection
								return;
							}
						}
						break;
					case EDistributedActorCommand_Unpublish:
						{
							if (!fp_HandleUnpublishPacket(_pConnection, Stream))
							{
								// TODO: Handle malicious connection
								return;
							}
						}
						break;
					default:
						{
							DMibNeverGetHere;
						}
						break;
					}
				}
				catch (NException::CException const &_Exception)
				{
					// TODO: Handle malicious connection
					return;
				}
				
				pPacket = pHost->m_Incoming_ReceivedPackets.f_GetFirst();
			}
			
			if (bAccPacket)
			{
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
				Stream << AckPacket;
				
				NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> pMessage = fg_Construct(Stream.f_MoveVector());
				
				_pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, fg_Move(pMessage), 0) 
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
}
