// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	namespace NActorDistributionManagerInternal
	{
		uint64 CPacket::f_GetPacketID() const
		{
			DMibRequire(m_pData && m_pData->f_GetLen() >= sizeof(uint64));
			uint8 const *pData = m_pData->f_GetArray() + 1;
			return fg_ByteSwapLE(*((uint64 const *)pData));
		}
	}

	void CActorDistributionManagerInternal::fp_SendPacket(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CSecureByteVector> &&_pMessage)
	{
		if (!_pConnection->m_Connection)
			return;
		_pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, fg_Move(_pMessage), 0)
			> [](TCAsyncResult<void> &&_Result)
			{
				if (!_Result)
				{
					DMibLog(DebugVerbose2, " ---- Error sending packet {}", _Result.f_GetExceptionStr());
					// TODO: Early reschedule here by deleting the connection?
				}
			}
		;
	}

	uint64 CActorDistributionManagerInternal::fp_QueuePacket(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, NContainer::CSecureByteVector &&_Data)
	{
		auto PacketID = ++_pHost->m_Outgoing_CurrentPacketID;
		DMibLog(DebugVerbose2, " ---- {} Queueing packet {}", _pHost->m_bIncoming, PacketID);

		{
			NStream::CBinaryStreamMemoryPtr<> Stream;
			Stream.f_OpenReadWrite(_Data.f_GetArray(), _Data.f_GetLen(), _Data.f_GetLen());
			Stream.f_AddPosition(sizeof(uint8));
			Stream << PacketID;
		}

		NStorage::TCUniquePointer<CActorDistributionManagerInternal::CPacket> pPacket = fg_Construct(fg_Construct(_Data));
		_pHost->m_Outgoing_QueuedPackets.f_Insert(pPacket.f_Detach());
		fp_SendPacketQueue(_pHost);

		return PacketID;
	}

	void CActorDistributionManagerInternal::fp_SendPacketQueue(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost)
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
			DMibLog(DebugVerbose2, " ---- {} {} Sending packet {}", _pHost->m_bIncoming, pConnection->f_GetConnectionID(), pPacket->f_GetPacketID());

			fp_SendPacket(pConnection, fg_TempCopy(pPacket->m_pData));
			pPacket->m_Link.f_Unlink();
			_pHost->m_Outgoing_SentPackets.f_Insert(pPacket);
			++iConnection;
			if (!iConnection)
				iConnection = _pHost->m_ActiveConnections.f_GetIterator();
		}

		_pHost->m_pLastSendConnection = iConnection;
	}

	void CActorDistributionManagerInternal::fp_ProcessPacketQueue(CConnection *_pConnection)
	{
		auto &pHost = _pConnection->m_pHost;
		uint64 AckPacket;
		bool bAccPacket = false;
		auto *pPacket = pHost->m_Incoming_ReceivedPackets.f_GetFirst();
		while (pPacket && pPacket->f_GetPacketID() == pHost->m_Incoming_NextPacketID)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Processing packet {}", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), pPacket->f_GetPacketID());
			AckPacket = pHost->m_Incoming_NextPacketID;
			++pHost->m_Incoming_NextPacketID;
			bAccPacket = true;
			pHost->m_Incoming_ReceivedPackets.f_Remove(pPacket);
			NStorage::TCUniquePointer<CPacket> pPacketStore = fg_Explicit(pPacket);

			try
			{
				NException::CDisableExceptionTraceScope DisableTrace;
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
				case EDistributedActorCommand_DestroySubscription:
					{
						if (!fp_HandleDestroySubscription(_pConnection, Stream))
						{
							// TODO: Handle malicious connection
							return;
						}
					}
					break;
				case EDistributedActorCommand_SubscriptionDestroyed:
					{
						if (!fp_HandleSubscriptionDestroyed(_pConnection, Stream))
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
			catch (NException::CException const &)
			{
				// TODO: Handle malicious connection
				return;
			}

			pPacket = pHost->m_Incoming_ReceivedPackets.f_GetFirst();
		}

		if (bAccPacket)
		{
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
			Stream << uint8(EDistributedActorCommand_Acknowledge);
			Stream << AckPacket;

			NStorage::TCSharedPointer<NContainer::CSecureByteVector> pMessage = fg_Construct(Stream.f_MoveVector());

			fp_SendPacket(_pConnection, fg_Move(pMessage));
		}
	}
}
