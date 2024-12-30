// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	namespace
	{
		[[maybe_unused]] NContainer::TCVector<uint64> fg_GetSentPacketIDs(NActorDistributionManagerInternal::CHost &_Host)
		{
			NContainer::TCVector<uint64> Packets;

			for (auto &Packet : _Host.m_Outgoing_SentPackets)
				Packets.f_Insert(Packet.f_GetPacketID());

			return Packets;
		}
	}

	bool CActorDistributionManagerInternal::fp_HandleProtocolIncoming
		(
			CConnection *_pConnection
			, NStorage::TCSharedPointer<NContainer::CSecureByteVector> const &_pMessage
		)
	{
		mint Length = _pMessage->f_GetLen();
		if (Length < 1)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Too small", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
			return false;
		}

		if (_pConnection->m_pHost)
		{
			auto &Host = *_pConnection->m_pHost;
			Host.m_nReceivedBytes += _pMessage->f_GetLen();
			++Host.m_nReceivedPackets;
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

					if (Identify.m_ProtocolVersion < EDistributedActorProtocolVersion_Min)
					{
						DMibLog(DebugVerbose2, " ---- {} {} Invalid protocol", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						if (!_pConnection->m_IdentifyPromise.f_IsSet())
							_pConnection->m_IdentifyPromise.f_SetException(DMibErrorInstance("Invalid protocol version"));
						return false;
					}

					auto &pHost = _pConnection->m_pHost;
					DMibFastCheck(pHost);

					auto &Host = *pHost;

					// Remove packets that remote already knows about
					while (auto *pPacket = Host.m_Outgoing_SentPackets.f_FindSmallest())
					{
						if (pPacket->f_GetPacketID() > Identify.m_AckedPacketID)
							break;

						Host.m_Outgoing_SentPackets.f_Remove(pPacket);
						fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pPacket);
					}

					DMibLog
						(
							DebugVerbose2
							, " ---- {} {} Acked until (Identify) {} Left {vs}"
							, _pConnection->m_pHost->m_bIncoming
							, _pConnection->f_GetConnectionID()
							, Identify.m_AckedPacketID
							, fg_GetSentPacketIDs(*pHost)
						)
					;

					bool bShouldReset = false;

					if (Host.m_LastExecutionID != Identify.m_ExecutionID)
						bShouldReset = true;
#if 0
					// This seems to not be needed
					if (!Identify.m_LastSeenExecutionID.f_IsEmpty() && Identify.m_LastSeenExecutionID != m_ExecutionID)
					{
						bShouldReset = true;
					}
#endif
					if (bShouldReset && !Host.m_LastExecutionID.f_IsEmpty())
						fp_ResetHostState(*pHost, _pConnection, true, "Identify with new execution ID");

					Host.m_LastExecutionID = Identify.m_ExecutionID;

					Host.m_bAllowAllNamespaces = Host.m_bAllowAllNamespaces || Identify.m_bAllowAllNamespaces;

					if (Host.m_bAllowAllNamespaces)
						Host.m_AllowedNamespaces.f_Clear();
					else
						Host.m_AllowedNamespaces += Identify.m_AllowedNamespaces;

					uint32 HostActorProtocolVersion = Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed);

					if (!bShouldReset && HostActorProtocolVersion)
					{
						if (Identify.m_ProtocolVersion != HostActorProtocolVersion)
						{
							DMibLog
								(
									DebugVerbose2
									, " ---- {} {} Inconsistent protocol version {} != {}"
									, _pConnection->m_pHost->m_bIncoming
									, _pConnection->f_GetConnectionID()
									, Identify.m_ProtocolVersion
									, HostActorProtocolVersion
								);
							if (!_pConnection->m_IdentifyPromise.f_IsSet())
								_pConnection->m_IdentifyPromise.f_SetException(DMibErrorInstance("Inconsistent protocol version"));
							return false;
						}
					}
					else
					{
						DMibLog
							(
								DebugVerbose1
								, " ---- {} {} Reset actor protocol version {} -> {}"
								, _pConnection->m_pHost->m_bIncoming
								, _pConnection->f_GetConnectionID()
								, HostActorProtocolVersion
								, Identify.m_ProtocolVersion
							)
						;

						Host.m_ActorProtocolVersion = Identify.m_ProtocolVersion;
					}

					if (!Identify.m_FriendlyName.f_IsEmpty() && Identify.m_FriendlyName != Host.m_FriendlyName)
					{
						Host.m_FriendlyName = Identify.m_FriendlyName;
						CHostInfo HostInfo;
						HostInfo.m_HostID = Host.m_HostInfo.m_RealHostID;
						HostInfo.m_FriendlyName = Host.m_FriendlyName;
						for (auto &OnHostInfoChanged : m_OnHostInfoChanged)
							OnHostInfoChanged.m_fOnHostInfoChanged.f_CallDiscard(HostInfo);
					}

					// Resend packets that remote thinks are missing
					for (auto &MissingPacketID : Identify.m_MissingPacketIDs)
					{
						auto pPacket = Host.m_Outgoing_SentPackets.f_FindEqual(MissingPacketID);
						if (pPacket)
							fp_SendPacket(_pConnection, fg_TempCopy(pPacket->m_pData));
					}

					{
						decltype(Host.m_Outgoing_SentPackets)::CIteratorConst iSentPackets;
						iSentPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
						iSentPackets.f_FindSmallestGreaterThanEqualForward(Identify.m_HighestSeenPacketID);

						while (iSentPackets)
						{
							fp_SendPacket(_pConnection, fg_TempCopy(iSentPackets->m_pData));
							++iSentPackets;
						}
					}

					DMibLog(DebugVerbose2, " ---- {} {} Identify", Host.m_bIncoming, _pConnection->f_GetConnectionID());

					_pConnection->m_bIdentified = true;
					Host.m_ActiveConnections.f_Insert(*_pConnection);
					_pConnection->m_bFirstConnection = _pConnection->m_Link.f_IsAloneInList();
					fp_CleanupMarkActive(Host);
					fp_SendPacketQueue(pHost);
					fp_ProcessPacketQueue(_pConnection);

					if (Host.f_CanSendPublish())
					{
						for (auto &NamespaceActors : m_LocalNamespaces)
						{
							auto &Namespace = NamespaceActors.f_GetNamespace();
							if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(Namespace))
								continue;
							if (Host.m_HostInfo.m_bAnonymous && !fp_NamespaceAllowedForAnonymous(Namespace))
								continue;

							for (auto &Actor : NamespaceActors.m_Actors)
							{
								CDistributedActorCommand_Publish Publish;
								Publish.m_ActorID = Actor.f_GetActorID();
								Publish.m_Namespace = Namespace;
								Publish.m_Hierarchy = Actor.m_Hierarchy;
								Publish.m_ProtocolVersions = Actor.m_ProtocolVersions;

								NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
								auto VersionScope = Host.f_StreamVersion(Stream);

								Stream << Publish;
								auto Data = Stream.f_MoveVector();

								fp_QueuePacket(pHost, fg_TempCopy(Data));
							}
						}
					}
					if (Identify.m_ProtocolVersion >= EDistributedActorProtocolVersion_InitialPublishFinishedSupported)
					{
						CDistributedActorCommand_InitialPublishFinished Identify;
						NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
						Stream << Identify;
						NStorage::TCSharedPointer<NContainer::CSecureByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());
						fp_SendPacket(_pConnection, fg_Move(pPacketData));
					}
					else
					{
						if (!_pConnection->m_IdentifyPromise.f_IsSet())
						{
							_pConnection->m_IdentifyPromise.f_SetResult(_pConnection->m_bFirstConnection);
							_pConnection->m_bPulishFinished = true;
						}
					}
				}
				break;
			case EDistributedActorCommand_InitialPublishFinished:
				{
#if 0
					// Enable after all old versions have been updated
					if (!_pConnection->m_bIdentified)
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not identified", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						DMibCheck(false);
						return false;
					}
#endif
					CDistributedActorCommand_InitialPublishFinished InitialPublishFinished;
					Stream >> InitialPublishFinished;

					_pConnection->m_bPulishFinished = true;

					TCFutureVector<void> PublishResults;
					for (auto &PublishFinished : _pConnection->m_PublishFinished)
						PublishFinished.f_Future() > PublishResults;

					fg_AllDoneWrapped(PublishResults).f_Timeout(30.0, "")
						> [pConnection = NStorage::TCSharedPointer<CConnection, NStorage::CSupportWeakTag>(fg_Explicit(_pConnection)), this](auto &&)
						{
							if (!pConnection->m_pHost)
								return;

							auto &Host = *pConnection->m_pHost;
							if (Host.m_ActorProtocolVersion >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
							{
								CDistributedActorCommand_InitialPublishFinishedProcessing Packet;
								NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
								Stream << Packet;
								NStorage::TCSharedPointer<NContainer::CSecureByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());
								fp_SendPacket(pConnection.f_Get(), fg_Move(pPacketData));
							}
							else
							{
								if (!pConnection->m_IdentifyPromise.f_IsSet())
									pConnection->m_IdentifyPromise.f_SetResult(pConnection->m_bFirstConnection);
							}
						}
					;
				}
				break;
			case EDistributedActorCommand_InitialPublishFinishedProcessing:
				{
					auto &Host = *_pConnection->m_pHost;
					if (Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
						return false;

					CDistributedActorCommand_InitialPublishFinishedProcessing InitialPublishFinishedProcessing;
					Stream >> InitialPublishFinishedProcessing;

					if (!_pConnection->m_IdentifyPromise.f_IsSet())
						_pConnection->m_IdentifyPromise.f_SetResult(_pConnection->m_bFirstConnection);
				}
				break;
			case EDistributedActorCommand_Acknowledge:
				{
					if (!_pConnection->m_Link.f_IsInList()) // Not an active connection
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not active ack", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						return false;
					}
					if (!_pConnection->m_bIdentified)
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not identified", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						DMibCheck(false);
						return false;
					}

					CDistributedActorCommand_Acknowledge Acknowledge;
					Stream >> Acknowledge;

					auto &pHost = _pConnection->m_pHost;

					// Remove packets that remote already knows about
					while (auto *pPacket = pHost->m_Outgoing_SentPackets.f_FindSmallest())
					{
						if (pPacket->f_GetPacketID() > Acknowledge.m_LastInOrderPacketID)
							break;

						pHost->m_Outgoing_SentPackets.f_Remove(pPacket);
						fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pPacket);
					}

					DMibLog
						(
							DebugVerbose2
							, " ---- {} {} Acked until {} Left {vs}"
							, _pConnection->m_pHost->m_bIncoming
							, _pConnection->f_GetConnectionID()
							, Acknowledge.m_LastInOrderPacketID
							, fg_GetSentPacketIDs(*pHost)
						)
					;
				}
				break;
			case EDistributedActorCommand_NotifyConnectionLost:
				{
					auto &Host = *_pConnection->m_pHost;

					if (Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_LostConnectionNotificationsSupported)
						return false;

					CDistributedActorCommand_NotifyConnectionLost Notification;
					Stream >> Notification;

					if (Notification.m_NotifyLostSequence <= Host.m_LastNotifyConnectionLostSequenceReceived)
						break;

					Host.m_LastNotifyConnectionLostSequenceReceived = Notification.m_NotifyLostSequence;

					CDistributedActorCommand_RequestMissingPackets RequestPackets;

					RequestPackets.m_HighestSeenPacketID = Host.m_Incoming_NextPacketID - 1;
					uint64 NextExpectedPacketID = Host.m_Incoming_NextPacketID;
					for (auto &ReceivedPacket : Host.m_Incoming_ReceivedPackets)
					{
						uint64 PacketID = ReceivedPacket.f_GetPacketID();
						while (NextExpectedPacketID < PacketID)
						{
							RequestPackets.m_MissingPacketIDs.f_Insert(NextExpectedPacketID);
							++NextExpectedPacketID;
						}
						RequestPackets.m_HighestSeenPacketID = fg_Max(RequestPackets.m_HighestSeenPacketID, PacketID);
					}

					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
					Stream << RequestPackets;

					NStorage::TCSharedPointer<NContainer::CSecureByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());

					fp_SendPacket(_pConnection, fg_Move(pPacketData));
				}
				break;
			case EDistributedActorCommand_RequestMissingPackets:
				{
					auto &Host = *_pConnection->m_pHost;

					if (Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_LostConnectionNotificationsSupported)
						return false;

					CDistributedActorCommand_RequestMissingPackets RequestPackets;
					Stream >> RequestPackets;

					// Resend packets that remote thinks are missing
					for (auto &MissingPacketID : RequestPackets.m_MissingPacketIDs)
					{
						auto pPacket = Host.m_Outgoing_SentPackets.f_FindEqual(MissingPacketID);
						if (pPacket)
							fp_SendPacket(_pConnection, fg_TempCopy(pPacket->m_pData));
					}

					{
						decltype(Host.m_Outgoing_SentPackets)::CIteratorConst iSentPackets;
						iSentPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
						iSentPackets.f_FindSmallestGreaterThanEqualForward(RequestPackets.m_HighestSeenPacketID);

						while (iSentPackets)
						{
							fp_SendPacket(_pConnection, fg_TempCopy(iSentPackets->m_pData));
							++iSentPackets;
						}
					}
					
				}
				break;
			case EDistributedActorCommand_RemoteCall:
			case EDistributedActorCommand_RemoteCallResult:
			case EDistributedActorCommand_Publish:
			case EDistributedActorCommand_PublishFinished:
			case EDistributedActorCommand_Unpublish:
			case EDistributedActorCommand_UnpublishFinished:
			case EDistributedActorCommand_DestroySubscription:
			case EDistributedActorCommand_SubscriptionDestroyed:
				{
					if (!_pConnection->m_bIdentified)
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not identified", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						DMibCheck(false);
						return false;
					}
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
							NStorage::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage);
							pHost->m_Incoming_ReceivedPackets.f_InsertAfter(pPacket.f_Detach(), &*iPacket);
							break;
						}
					}
					if (!iPacket)
					{
						NStorage::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage);
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
			(void)_Exception;
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
		Identify.m_ExecutionID = pHost->m_ExecutionID;
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

		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		Stream << Identify;

		NStorage::TCSharedPointer<NContainer::CSecureByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());

		fp_SendPacket(_pConnection, fg_Move(pPacketData));
	}

	void CActorDistributionManagerInternal::fp_NotifyDisconnect(CHost &_Host)
	{
		if (_Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_LostConnectionNotificationsSupported)
			return;

		if (_Host.m_ActiveConnections.f_IsEmpty())
			return;

		CDistributedActorCommand_NotifyConnectionLost Notification;
		Notification.m_NotifyLostSequence = ++_Host.m_LastNotifyConnectionLostSequenceSent;

		NContainer::CSecureByteVector PacketData;
		{
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
			Stream << Notification;
			PacketData = Stream.f_MoveVector();
		}

		NStorage::TCSharedPointer<NContainer::CSecureByteVector> pPacketData = fg_Construct(PacketData);

		for (auto &Connection : _Host.m_ActiveConnections)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Sending notify disconnect packet {}", _Host.m_bIncoming, Connection.f_GetConnectionID(), Notification.m_NotifyLostSequence);
			fp_SendPacket(&Connection, fg_TempCopy(pPacketData));
		}
	}
}
