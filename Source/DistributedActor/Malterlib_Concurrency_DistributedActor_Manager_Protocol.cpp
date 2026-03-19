// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	using namespace NActorDistributionManagerInternal;

	namespace
	{
		[[maybe_unused]] NContainer::TCVector<uint64> fg_GetSentPacketIDs(CHost &_Host)
		{
			NContainer::TCVector<uint64> Packets;

			for (auto &Packet : _Host.m_Outgoing_SentPackets)
				Packets.f_Insert(Packet.f_GetPacketID());

			return Packets;
		}
	}

	bool CActorDistributionManagerInternal::fp_QueueIncomingPacket
		(
			CConnection *_pConnection
			, NStorage::TCSharedPointer<NContainer::CIOByteVector> const &_pMessage
			, NStream::CBinaryStreamMemoryPtr<> &_Stream
			, uint8 _Priority
		)
	{
		if (!_pConnection->m_bIdentified)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Not identified", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
			DMibCheck(false);
			return false;
		}

		auto &pHost = _pConnection->m_pHost;
		auto &PriorityQueues = pHost->m_PriorityQueues[_Priority];

		uint64 PacketID;
		_Stream >> PacketID;

		// Validate packet ID from network (must fit in 56 bits)
		if (!fg_IsValidPacketID(PacketID))
			return false; // Malicious client

		if (PacketID < PriorityQueues.m_IncomingNextPacketID)
		{
			DMibLog(DebugVerbose2, " ---- {} {} IGNORING PACKET {} (priority {})", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID, _Priority);
			return true; // Already received
		}

		auto iPacket = PriorityQueues.m_IncomingPackets.f_GetIterator();
		iPacket.f_Reverse(PriorityQueues.m_IncomingPackets);
		for (; iPacket; --iPacket)
		{
			if (iPacket->f_GetPacketID() == PacketID)
			{
				DMibLog(DebugVerbose2, " ---- {} {} ALREADY RECEIVED PACKET {} (priority {})", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID, _Priority);
				return true; // Already received
			}
			else if (iPacket->f_GetPacketID() < PacketID)
			{
				DMibLog(DebugVerbose2, " ---- {} {} Receive packet {} (priority {})", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID, _Priority);
				NStorage::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage, _Priority);
				PriorityQueues.m_IncomingPackets.f_InsertAfter(pPacket.f_Detach(), &*iPacket);
				break;
			}
		}

		if (!iPacket)
		{
			NStorage::TCUniquePointer<CPacket> pPacket = fg_Construct(_pMessage, _Priority);
			DMibLog(DebugVerbose2, " ---- {} {} Receive2 packet {} (priority {})", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID, _Priority);
			PriorityQueues.m_IncomingPackets.f_InsertFirst(pPacket.f_Detach());
		}

		if (_pConnection->m_Link.f_IsInList()) // Only active connections
			fp_ProcessPacketQueue(_pConnection, PriorityQueues, _Priority);
		else
			DMibLog(DebugVerbose2, " ---- {} {} NOT ACTIVE {} (priority {})", pHost->m_bIncoming, _pConnection->f_GetConnectionID(), PacketID, _Priority);

		return true;
	}

	void CActorDistributionManagerInternal::fp_RemoveAcknowledgedPackets(CHost &_Host, uint8 _Priority, uint64 _LastInOrderPacketID)
	{
		// Remove all acknowledged packets with the specified priority
		// Tree is sorted by packed key (priority:8, packetID:56), so packets for each priority are contiguous
		// Note: With 56-bit packet IDs, wrap-around is theoretically possible but would require
		// 72 quadrillion packets - at 1M packets/sec that's billions of years
		uint64 EndKey = fg_MakePriorityPacketKey(_Priority, _LastInOrderPacketID);

		for (;;)
		{
			// Find first packet in this priority's range (start at 0 to handle wrap-around case)
			auto *pPacket = _Host.m_Outgoing_SentPackets.f_FindSmallestGreaterThanEqual(fg_MakePriorityPacketKey(_Priority, 0));
			if (!pPacket)
				break;

			// Check if still within the acknowledged range and correct priority
			if (fg_MakePriorityPacketKey(pPacket->m_Priority, pPacket->f_GetPacketID()) > EndKey)
				break;

			_Host.m_Outgoing_SentPackets.f_Remove(pPacket);
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pPacket);
		}
	}

	bool CActorDistributionManagerInternal::fp_HandleProtocolIncoming
		(
			CConnection *_pConnection
			, NStorage::TCSharedPointer<NContainer::CIOByteVector> const &_pMessage
		)
	{
		umint Length = _pMessage->f_GetLen();
		if (Length < 1)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Too small", _pConnection->m_pHost ? _pConnection->m_pHost->m_bIncoming : false, _pConnection->f_GetConnectionID());
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
						DMibLog(DebugVerbose2, " ---- {} {} Invalid protocol: 0x{nfh}", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID(), Identify.m_ProtocolVersion);
						if (!_pConnection->m_IdentifyPromise.f_IsSet())
							_pConnection->m_IdentifyPromise.f_SetException(DMibErrorInstance("Invalid protocol version"));
						return false;
					}

					// Validate packet IDs from network (must fit in 56 bits)
					for (auto &PriorityState : Identify.m_PriorityQueueStates)
					{
						if (!fg_IsValidPacketID(PriorityState.m_AckedPacketID) || !fg_IsValidPacketID(PriorityState.m_HighestSeenPacketID))
							return false; // Malicious client

						for (auto &MissingPacketID : PriorityState.m_MissingPacketIDs)
						{
							if (!fg_IsValidPacketID(MissingPacketID))
								return false; // Malicious client
						}
					}

					auto &pHost = _pConnection->m_pHost;
					DMibFastCheck(pHost);

					auto &Host = *pHost;

#if (DMibSysLogSeverities) & DMibLogSeverity_DebugVerbose2
					uint64 DefaultAckedPacketID = 0;
#endif

					for (auto &PriorityStateEntry : Identify.m_PriorityQueueStates.f_Entries())
					{
						auto Priority = PriorityStateEntry.f_Key();
						auto &PriorityState = PriorityStateEntry.f_Value();
#if (DMibSysLogSeverities) & DMibLogSeverity_DebugVerbose2
						if (Priority == 128)
							DefaultAckedPacketID = PriorityState.m_AckedPacketID;
#endif
						fp_RemoveAcknowledgedPackets(Host, Priority, PriorityState.m_AckedPacketID);
					}

					DMibLog
						(
							DebugVerbose2
							, " ---- {} {} Acked until (Identify) {} Left {vs}"
							, _pConnection->m_pHost->m_bIncoming
							, _pConnection->f_GetConnectionID()
							, DefaultAckedPacketID
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

					uint32 HostActorProtocolVersion = Host.m_ActorProtocolVersion.f_Load(NAtomic::gc_MemoryOrder_Relaxed);

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
								, " ---- {} {} Reset actor protocol version 0x{nfh} -> 0x{nfh}"
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

					for (auto &PriorityState : Identify.m_PriorityQueueStates)
					{
						uint8 Priority = Identify.m_PriorityQueueStates.fs_GetKey(PriorityState);

						for (auto &MissingPacketID : PriorityState.m_MissingPacketIDs)
						{
							auto pPacket = Host.m_Outgoing_SentPackets.f_FindEqual(fg_MakePriorityPacketKey(Priority, MissingPacketID));
							if (pPacket)
								fp_SendPacket(_pConnection, fg_TempCopy(pPacket->m_pData), pPacket->m_Priority);
						}

						decltype(Host.m_Outgoing_SentPackets)::CIteratorConst iSentPackets;
						iSentPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
						iSentPackets.f_FindSmallestGreaterThanEqualForward(fg_MakePriorityPacketKey(Priority, PriorityState.m_HighestSeenPacketID));

						while (iSentPackets && iSentPackets->m_Priority == Priority)
						{
							fp_SendPacket(_pConnection, fg_TempCopy(iSentPackets->m_pData), iSentPackets->m_Priority);
							++iSentPackets;
						}
					}

					// Resend packets at priorities the client has never seen
					{
						decltype(Host.m_Outgoing_SentPackets)::CIteratorConst iPackets;
						iPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
						iPackets.f_FindSmallestGreaterThanEqualForward(uint64(0u));

						while (iPackets)
						{
							uint8 Priority = iPackets->m_Priority;

							if (Identify.m_PriorityQueueStates.f_FindEqual(Priority) != nullptr)
							{
								if (Priority == 255)
									break; // No higher priorities possible

								iPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
								iPackets.f_FindSmallestGreaterThanEqualForward(fg_MakePriorityPacketKey(Priority + 1, 0));
								continue;
							}

							while (iPackets && iPackets->m_Priority == Priority)
							{
								fp_SendPacket(_pConnection, fg_TempCopy(iPackets->m_pData), iPackets->m_Priority);
								++iPackets;
							}
						}
					}

					DMibLog
						(
							SubscriptionLogVerbosity, " ---- {} {} Identify AllowAll {} Allowed {vs} Host {}"
							, Host.m_bIncoming
							, _pConnection->f_GetConnectionID()
							, Identify.m_bAllowAllNamespaces
							, Identify.m_AllowedNamespaces
							, Host.f_GetHostInfo()
						)
					;

					_pConnection->m_bIdentified = true;
					Host.m_ActiveConnections.f_Insert(*_pConnection);
					_pConnection->m_bFirstConnection = _pConnection->m_Link.f_IsAloneInList();
					fp_CleanupMarkActive(Host);
					fp_SendPacketQueue(pHost);
					for (auto &PriorityQueuesEntry : pHost->m_PriorityQueues.f_Entries())
						fp_ProcessPacketQueue(_pConnection, PriorityQueuesEntry.f_Value(), PriorityQueuesEntry.f_Key());

					if (Host.f_CanSendPublish())
					{
						for (auto &NamespaceActors : m_LocalNamespaces)
						{
							auto &Namespace = NamespaceActors.f_GetNamespace();
							if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(Namespace))
							{
								DMibLogWithCategory
									(
										Mib/Concurrency/Distribution
										, SubscriptionLogVerbosity
										, "   Cannot publish namespace '{}' to '{}{}{}': bAllowAll: {} Allowed: {vs}"
										, Namespace
										, Host.m_HostInfo.m_RealHostID
										, Host.m_bIncoming ? " in" : ""
										, Host.m_bOutgoing ? " out" : ""
										, Host.m_bAllowAllNamespaces
										, Host.m_AllowedNamespaces
									)
								;
								continue;
							}

							if (Host.m_HostInfo.m_bAnonymous && !fp_NamespaceAllowedForAnonymous(Namespace))
							{
								DMibLogWithCategory
									(
										Mib/Concurrency/Distribution
										, SubscriptionLogVerbosity
										, "   Cannot publish namespace '{}' to '{}{}{}': Anonymous"
										, Namespace
										, Host.m_HostInfo.m_RealHostID
										, Host.m_bIncoming ? " in" : ""
										, Host.m_bOutgoing ? " out" : ""
									)
								;
								continue;
							}

							for (auto &Actor : NamespaceActors.m_Actors)
							{
								CDistributedActorCommand_Publish Publish;
								Publish.m_ActorID = Actor.f_GetActorID();
								Publish.m_Namespace = Namespace;
								Publish.m_Hierarchy = Actor.m_Hierarchy;
								Publish.m_ProtocolVersions = Actor.m_ProtocolVersions;

								NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
								auto VersionScope = Host.f_StreamVersion(Stream);

								Stream << Publish;
								auto Data = Stream.f_MoveVector();

								DMibLogWithCategory
									(
										Mib/Concurrency/Distribution
										, SubscriptionLogVerbosity
										, "   Sending publish packet for namespace '{}' to '{}{}{}': {}"
										, Namespace
										, Host.m_HostInfo.m_RealHostID
										, Host.m_bIncoming ? " in" : ""
										, Host.m_bOutgoing ? " out" : ""
										, Publish.m_ActorID
									)
								;
								fp_QueuePacket(pHost, fg_TempCopy(Data));
							}
						}
					}
					else
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Distribution
								, SubscriptionLogVerbosity
								, "   Cannot send publish to '{}{}{}': Anon {}"
								, Host.m_HostInfo.m_RealHostID
								, Host.m_bIncoming ? " in" : ""
								, Host.m_bOutgoing ? " out" : ""
								, Host.m_HostInfo.m_bAnonymous
							)
						;
					}

					if (Identify.m_ProtocolVersion >= EDistributedActorProtocolVersion_InitialPublishFinishedSupported)
					{
						CDistributedActorCommand_InitialPublishFinished Identify;
						NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
						Stream << Identify;
						NStorage::TCSharedPointer<NContainer::CIOByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());
						// InitialPublishFinished must stay ordered with Publish packets - use priority 128
						fp_SendPacket(_pConnection, fg_Move(pPacketData), 128);
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

					fg_AllDone(PublishResults).f_Timeout(30.0, "")
						> [pConnection = NStorage::TCSharedPointer<CConnection, NStorage::CSupportWeakTag>(fg_Explicit(_pConnection)), this](auto &&_Result)
						{
							if (!pConnection->m_pHost)
								return;

							DMibLog
								(
									SubscriptionLogVerbosity
									, " ---- {} Identify publish finished: {}"
									, pConnection->f_GetConnectionID()
									, (_Result ? NStr::CStr("Success") : _Result.f_GetExceptionStr())
								)
							;

							auto &Host = *pConnection->m_pHost;
							if (Host.m_ActorProtocolVersion >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
							{
								CDistributedActorCommand_InitialPublishFinishedProcessing Packet;
								NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
								Stream << Packet;
								NStorage::TCSharedPointer<NContainer::CIOByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());
								// InitialPublishFinishedProcessing must stay ordered with data flow - use priority 128
								fp_SendPacket(pConnection.f_Get(), fg_Move(pPacketData), 128);
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

					DMibLog(SubscriptionLogVerbosity, " ---- {} {} Identify publish finished processing", Host.m_bIncoming, _pConnection->f_GetConnectionID());

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

					if (!fg_IsValidPacketID(Acknowledge.m_LastInOrderPacketID))
						return false; // Malicious client

					auto &pHost = _pConnection->m_pHost;

					fp_RemoveAcknowledgedPackets(*pHost, 128, Acknowledge.m_LastInOrderPacketID);

					DMibLog
						(
							DebugVerbose2
							, " ---- {} {} Acked until {} (priority 128) Left {vs}"
							, _pConnection->m_pHost->m_bIncoming
							, _pConnection->f_GetConnectionID()
							, Acknowledge.m_LastInOrderPacketID
							, fg_GetSentPacketIDs(*pHost)
						)
					;
				}
				break;
			case EDistributedActorCommand_AcknowledgeWithPriority:
				{
					if (!_pConnection->m_Link.f_IsInList()) // Not an active connection
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not active ack with priority", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						return false;
					}
					if (!_pConnection->m_bIdentified)
					{
						DMibLog(DebugVerbose2, " ---- {} {} Not identified", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
						DMibCheck(false);
						return false;
					}

					uint8 Priority;
					Stream >> Priority;
					uint64 LastInOrderPacketID;
					Stream >> LastInOrderPacketID;

					if (!fg_IsValidPacketID(LastInOrderPacketID))
						return false; // Malicious client

					auto &pHost = _pConnection->m_pHost;

					fp_RemoveAcknowledgedPackets(*pHost, Priority, LastInOrderPacketID);

					DMibLog
						(
							DebugVerbose2
							, " ---- {} {} Acked until {} (priority {}) Left {vs}"
							, _pConnection->m_pHost->m_bIncoming
							, _pConnection->f_GetConnectionID()
							, LastInOrderPacketID
							, Priority
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

					for (auto &PriorityQueuesEntry : Host.m_PriorityQueues.f_Entries())
					{
						uint8 Priority = PriorityQueuesEntry.f_Key();
						auto &PriorityQueues = PriorityQueuesEntry.f_Value();
						uint64 HighestSeenPacketID = PriorityQueues.m_IncomingNextPacketID - 1;
						uint64 NextExpectedPacketID = PriorityQueues.m_IncomingNextPacketID;

						CPriorityMissingInfo &PriorityState = RequestPackets.m_PriorityQueueStates[Priority];

						for (auto &ReceivedPacket : PriorityQueues.m_IncomingPackets)
						{
							uint64 PacketID = ReceivedPacket.f_GetPacketID();
							while (NextExpectedPacketID < PacketID)
							{
								PriorityState.m_MissingPacketIDs.f_Insert(NextExpectedPacketID);
								++NextExpectedPacketID;
							}
							HighestSeenPacketID = fg_Max(HighestSeenPacketID, PacketID);
							++NextExpectedPacketID;
						}

						PriorityState.m_HighestSeenPacketID = HighestSeenPacketID;
					}

					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
					auto VersionScope = Host.f_StreamVersion(Stream);
					Stream << RequestPackets;

					NStorage::TCSharedPointer<NContainer::CIOByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());

					// RequestMissingPackets is a recovery control packet - use priority 0 (highest)
					fp_SendPacket(_pConnection, fg_Move(pPacketData), 0);
				}
				break;
			case EDistributedActorCommand_RequestMissingPackets:
				{
					auto &Host = *_pConnection->m_pHost;

					if (Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_LostConnectionNotificationsSupported)
						return false;

					CDistributedActorCommand_RequestMissingPackets RequestPackets;
					auto VersionScope = Host.f_StreamVersion(Stream);
					Stream >> RequestPackets;

					for (auto &PriorityState : RequestPackets.m_PriorityQueueStates)
					{
						if (!fg_IsValidPacketID(PriorityState.m_HighestSeenPacketID))
							return false; // Malicious client

						for (auto &MissingPacketID : PriorityState.m_MissingPacketIDs)
						{
							if (!fg_IsValidPacketID(MissingPacketID))
								return false; // Malicious client
						}
					}

					for (auto &PriorityState : RequestPackets.m_PriorityQueueStates)
					{
						uint8 Priority = RequestPackets.m_PriorityQueueStates.fs_GetKey(PriorityState);

						for (auto &MissingPacketID : PriorityState.m_MissingPacketIDs)
						{
							auto pPacket = Host.m_Outgoing_SentPackets.f_FindEqual(fg_MakePriorityPacketKey(Priority, MissingPacketID));
							if (pPacket)
								fp_SendPacket(_pConnection, fg_TempCopy(pPacket->m_pData), pPacket->m_Priority);
						}

						decltype(Host.m_Outgoing_SentPackets)::CIteratorConst iSentPackets;
						iSentPackets.f_InitForSearch(Host.m_Outgoing_SentPackets);
						iSentPackets.f_FindSmallestGreaterThanEqualForward(fg_MakePriorityPacketKey(Priority, PriorityState.m_HighestSeenPacketID));

						while (iSentPackets && iSentPackets->m_Priority == Priority)
						{
							fp_SendPacket(_pConnection, fg_TempCopy(iSentPackets->m_pData), iSentPackets->m_Priority);
							++iSentPackets;
						}
					}
				}
				break;
			case EDistributedActorCommand_RemoteCall:
			case EDistributedActorCommand_RemoteCallWithAuthHandler:
			case EDistributedActorCommand_RemoteCallResult:
			case EDistributedActorCommand_Publish:
			case EDistributedActorCommand_PublishFinished:
			case EDistributedActorCommand_Unpublish:
			case EDistributedActorCommand_UnpublishFinished:
			case EDistributedActorCommand_DestroySubscription:
			case EDistributedActorCommand_SubscriptionDestroyed:
				{
					// Legacy commands use default priority 128
					if (!fp_QueueIncomingPacket(_pConnection, _pMessage, Stream, 128))
						return false;
				}
				break;
			case EDistributedActorCommand_RemoteCallWithPriority:
			case EDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler:
			case EDistributedActorCommand_RemoteCallResultWithPriority:
				{
					// Priority commands have format: [cmd:1][priority:1][packetID:8]...
					uint8 Priority;
					Stream >> Priority;
					if (!fp_QueueIncomingPacket(_pConnection, _pMessage, Stream, Priority))
						return false;
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
			DMibLog
				(
					SubscriptionLogVerbosity
					, " ---- {} {} Exception processing data {}"
					, _pConnection->m_pHost ? _pConnection->m_pHost->m_bIncoming : false
					, _pConnection->f_GetConnectionID()
					, _Exception.f_GetErrorStr()
				)
			;
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

		// Build acked packet info for all priority queues
		for (auto &PriorityQueuesEntry : pHost->m_PriorityQueues.f_Entries())
		{
			uint8 Priority = PriorityQueuesEntry.f_Key();
			auto &PriorityQueues = PriorityQueuesEntry.f_Value();
			uint64 AckedPacketID = PriorityQueues.m_IncomingNextPacketID - 1;
			uint64 HighestSeenPacketID = AckedPacketID;
			uint64 NextExpectedPacketID = PriorityQueues.m_IncomingNextPacketID;

			// Get or create the priority state entry for non-128 priorities
			CPriorityQueueState &PriorityState = Identify.m_PriorityQueueStates[Priority];

			for (auto &ReceivedPacket : PriorityQueues.m_IncomingPackets)
			{
				uint64 PacketID = ReceivedPacket.f_GetPacketID();
				while (NextExpectedPacketID < PacketID)
				{
					PriorityState.m_MissingPacketIDs.f_Insert(NextExpectedPacketID);
					++NextExpectedPacketID;
				}
				HighestSeenPacketID = fg_Max(HighestSeenPacketID, PacketID);
				++NextExpectedPacketID;
			}

			PriorityState.m_AckedPacketID = AckedPacketID;
			PriorityState.m_HighestSeenPacketID = HighestSeenPacketID;
		}

		bool bAllowAllNamespaces = false;
		auto &AllowedNamespaces = fp_GetAllowedNamespacesForHost(pHost, bAllowAllNamespaces);

		Identify.m_bAllowAllNamespaces = bAllowAllNamespaces;
		if (!bAllowAllNamespaces)
			Identify.m_AllowedNamespaces = AllowedNamespaces;

		DMibLogWithCategory
			(
				Mib/Concurrency/Distribution
				, SubscriptionLogVerbosity
				, "   Sending identify packet to '{}{}{}{}': bAllowAllNamespaces {} AllowedNamespaces {vs}"
				, pHost->m_HostInfo.m_RealHostID
				, _pConnection->m_bIncoming ? " connin" : " connout"
				, pHost->m_bIncoming ? " in" : ""
				, pHost->m_bOutgoing ? " out" : ""
				, Identify.m_bAllowAllNamespaces
				, Identify.m_AllowedNamespaces
			)
		;

		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
		Stream << Identify;

		NStorage::TCSharedPointer<NContainer::CIOByteVector> pPacketData = fg_Construct(Stream.f_MoveVector());

		// Identify is the first packet on a connection - use priority 0 (highest)
		fp_SendPacket(_pConnection, fg_Move(pPacketData), 0);
	}

	void CActorDistributionManagerInternal::fp_NotifyDisconnect(CHost &_Host)
	{
		if (_Host.m_ActorProtocolVersion < EDistributedActorProtocolVersion_LostConnectionNotificationsSupported)
			return;

		if (_Host.m_ActiveConnections.f_IsEmpty())
			return;

		CDistributedActorCommand_NotifyConnectionLost Notification;
		Notification.m_NotifyLostSequence = ++_Host.m_LastNotifyConnectionLostSequenceSent;

		NContainer::CIOByteVector PacketData;
		{
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
			Stream << Notification;
			PacketData = Stream.f_MoveVector();
		}

		NStorage::TCSharedPointer<NContainer::CIOByteVector> pPacketData = fg_Construct(PacketData);

		for (auto &Connection : _Host.m_ActiveConnections)
		{
			DMibLog(DebugVerbose2, " ---- {} {} Sending notify disconnect packet {}", _Host.m_bIncoming, Connection.f_GetConnectionID(), Notification.m_NotifyLostSequence);
			// NotifyConnectionLost is a control notification with its own sequence number - use priority 0 (highest)
			fp_SendPacket(&Connection, fg_TempCopy(pPacketData), 0);
		}
	}
}
