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
			uint64 PacketID;
			NMemory::fg_MemCopy(&PacketID, pData, sizeof(PacketID));
			return fg_ByteSwapLE(PacketID);
		}
	}

	void CActorDistributionManagerInternal::fp_OnInvalidConnection
		(
			CConnection *_pConnection
			, NException::CExceptionPointer &&_pException
		)
	{
		if (_pConnection->m_bIncoming)
		{
			auto *pConnection = static_cast<CServerConnection *>(_pConnection);
			if (!pConnection->m_pHost)
				return;

			bool bLast = pConnection->m_Link.f_IsAloneInList();

			NStr::CStr CloseMessage = fg_Format
				(
					"Invalid connection: <{}> Closed {} incoming '{}'. {}"
					, pConnection->f_GetHostInfo()
					, bLast ? "last" : "additional"
					, pConnection->f_GetConnectionID()
					, NException::fg_ExceptionString(_pException)
				)
			;

			pConnection->m_pHost->m_bLoggedConnection = false;
			DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "{}", CloseMessage);

			fp_DestroyServerConnection(*pConnection, true, CloseMessage, false);
		}
		else
		{
			using namespace NStr;

			auto *pConnection = static_cast<CClientConnection *>(_pConnection);

			bool bLast = pConnection->m_Link.f_IsAloneInList();
			auto pHost = pConnection->m_pHost;

			CStr Error = "Invalid connection: {}"_f << NException::fg_ExceptionString(_pException);
			pConnection->f_Reset(!pConnection->m_bRetryConnectOnFailure, *this, Error);
			pConnection->f_SetLastError(NException::fg_ExceptionString(_pException));

			if (!pConnection->m_bRetryConnectOnFailure)
			{
				if (bLast && pHost)
					fp_DestroyHost(*pHost, pConnection, Error);
				return;
			}

			fp_ScheduleReconnect(NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag>(pConnection), true, pConnection->m_ConnectionSequence, "Invalid connection", false);
		}
	}

	void CActorDistributionManagerInternal::fp_SendPacket(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CIOByteVector> &&_pMessage)
	{
		if (!_pConnection->m_Connection)
		{
			if (_pConnection->m_pHost)
			{
				auto &Host = *_pConnection->m_pHost;
				Host.m_nDiscardedBytes += _pMessage->f_GetLen();
				++Host.m_nDiscardedPackets;
			}
			return;
		}

		if (_pConnection->m_pHost)
		{
			auto &Host = *_pConnection->m_pHost;
			Host.m_nSentBytes += _pMessage->f_GetLen();
			++Host.m_nSentPackets;
		}

		_pConnection->m_Connection(&NWeb::CWebSocketActor::f_SendBinary, fg_Move(_pMessage), 0)
			> [this, pWeakConnection = NStorage::TCSharedPointer<CConnection, NStorage::CSupportWeakTag>(fg_Explicit(_pConnection)).f_Weak()](TCAsyncResult<void> &&_Result)
			{
				if (!_Result)
				{
					auto pConnection = pWeakConnection.f_Lock();
					if (pConnection)
						fp_OnInvalidConnection(pConnection.f_Get(), _Result.f_GetException());
				}
			}
		;
	}

	uint64 CActorDistributionManagerInternal::fp_QueuePacket(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, NContainer::CIOByteVector &&_Data)
	{
		DMibFastCheck(_Data.f_GetLen() <= m_WebsocketSettings.m_MaxMessageSize);

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
						if (!fp_ApplyRemoteCall(_pConnection, Stream, false))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid remote call").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_RemoteCallWithAuthHandler:
					{
						if (!fp_ApplyRemoteCall(_pConnection, Stream, true))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid remote call").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_RemoteCallResult:
					{
						if (!fp_ApplyRemoteCallResult(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid remote call result").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_Publish:
					{
						if (!fp_HandlePublishPacket(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid publish").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_PublishFinished:
					{
						if (!fp_HandlePublishFinishedPacket(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid publish finished").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_Unpublish:
					{
						if (!fp_HandleUnpublishPacket(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid unpublish").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_UnpublishFinished:
					{
						if (!fp_HandleUnpublishFinishedPacket(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid unpublish finished").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_DestroySubscription:
					{
						if (!fp_HandleDestroySubscription(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid destroy subscription").f_ExceptionPointer());
							return;
						}
					}
					break;
				case EDistributedActorCommand_SubscriptionDestroyed:
					{
						if (!fp_HandleSubscriptionDestroyed(_pConnection, Stream))
						{
							fp_OnInvalidConnection(_pConnection, DMibErrorInstance("Invalid subscription destroyed").f_ExceptionPointer());
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
				fp_OnInvalidConnection(_pConnection, NException::fg_CurrentException());
				return;
			}

			pPacket = pHost->m_Incoming_ReceivedPackets.f_GetFirst();
		}

		if (bAccPacket)
		{
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
			Stream << uint8(EDistributedActorCommand_Acknowledge);
			Stream << AckPacket;

			NStorage::TCSharedPointer<NContainer::CIOByteVector> pMessage = fg_Construct(Stream.f_MoveVector());

			fp_SendPacket(_pConnection, fg_Move(pMessage));
		}
	}
}
