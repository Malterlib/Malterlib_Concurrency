// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency
{
	enum EDistributedActorCommand : uint8
	{
		EDistributedActorCommand_Identify = 0
		, EDistributedActorCommand_Acknowledge = 1
		, EDistributedActorCommand_RemoteCall = 2
		, EDistributedActorCommand_RemoteCallResult = 3
		, EDistributedActorCommand_Publish = 4
		, EDistributedActorCommand_Unpublish = 5
		, EDistributedActorCommand_DestroySubscription = 6
		, EDistributedActorCommand_InitialPublishFinished = 7
		, EDistributedActorCommand_SubscriptionDestroyed = 8
		, EDistributedActorCommand_NotifyConnectionLost = 9
		, EDistributedActorCommand_RequestMissingPackets = 10
		, EDistributedActorCommand_InitialPublishFinishedProcessing = 11
		, EDistributedActorCommand_PublishFinished = 12
		, EDistributedActorCommand_UnpublishFinished = 13
		, EDistributedActorCommand_RemoteCallWithAuthHandler = 14
		, EDistributedActorCommand_RemoteCallWithPriority = 15
		, EDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler = 16
		, EDistributedActorCommand_RemoteCallResultWithPriority = 17
		, EDistributedActorCommand_AcknowledgeWithPriority = 18
	};

	// Per-priority queue state for protocol messages
	struct CPriorityQueueState
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NContainer::TCVector<uint64> m_MissingPacketIDs;
		uint64 m_AckedPacketID = 0;
		uint64 m_HighestSeenPacketID = 0;
	};

	struct CDistributedActorCommand_Identify
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint32 m_ProtocolVersion = EDistributedActorProtocolVersion_Current;
		uint8 m_bAllowAllNamespaces;
		NStr::CStr m_ExecutionID;
		NStr::CStr m_LastSeenExecutionID;
		NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;
		NStr::CStr m_FriendlyName;
		NContainer::TCMap<uint8, CPriorityQueueState> m_PriorityQueueStates;
	};

	struct CDistributedActorCommand_Acknowledge
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_LastInOrderPacketID;
	};

	// In packet queue:

	struct CDistributedActorCommand_RemoteCall
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		NStr::CStr m_ActorID;
		// Packet data
	};

	struct CDistributedActorCommand_RemoteCallWithAuthHandler : public CDistributedActorCommand_RemoteCall
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint32 m_AuthenticationHandlerID = 0;
	};

	struct CDistributedActorCommand_RemoteCallWithPriority : public CDistributedActorCommand_RemoteCall
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint8 m_Priority = 128;
	};

	struct CDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler : public CDistributedActorCommand_RemoteCallWithAuthHandler
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint8 m_Priority = 128;
	};

	struct CResultSubscriptionData
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::TCVector<NStr::CStr> m_ClaimedSubscriptionIDs;
	};

	struct CDistributedActorCommand_RemoteCallResult
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		uint64 m_ReplyToPacketID;
		CResultSubscriptionData m_SubscriptionData;
		// Result data
	};

	struct CDistributedActorCommand_RemoteCallResultWithPriority : public CDistributedActorCommand_RemoteCallResult
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint8 m_Priority = 128;
	};

	struct CDistributedActorCommand_Publish
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		NStr::CStr m_ActorID;
		NStr::CStr m_Namespace;
		NContainer::TCVector<uint32> m_Hierarchy;
		CDistributedActorProtocolVersions m_ProtocolVersions;
		uint32 m_WaitPublicationID = 0;
	};

	struct CDistributedActorCommand_PublishFinished
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		uint32 m_WaitPublicationID = 0;
	};

	struct CDistributedActorCommand_Unpublish
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		NStr::CStr m_ActorID;
		NStr::CStr m_Namespace;
		uint32 m_WaitPublicationID = 0;
	};

	struct CDistributedActorCommand_UnpublishFinished
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		uint32 m_WaitPublicationID = 0;
	};

	struct CDistributedActorCommand_DestroySubscription
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_PacketID;
		NStr::CStr m_SubscriptionID;
	};

	struct CDistributedActorCommand_InitialPublishFinished
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
	};

	struct CDistributedActorCommand_InitialPublishFinishedProcessing
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
	};

	struct CDistributedActorCommand_SubscriptionDestroyed
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream, uint32 _ActorProtocolVersion);

		uint64 m_PacketID;
		NStr::CStr m_SubscriptionID;
		TCAsyncResult<void> m_Result;
	};

	struct CDistributedActorCommand_NotifyConnectionLost
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint64 m_NotifyLostSequence = 0;
	};

	// Per-priority missing packet info (no AckedPacketID needed for RequestMissingPackets)
	struct CPriorityMissingInfo
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NContainer::TCVector<uint64> m_MissingPacketIDs;
		uint64 m_HighestSeenPacketID = 0;
	};

	struct CDistributedActorCommand_RequestMissingPackets
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::TCMap<uint8, CPriorityMissingInfo> m_PriorityQueueStates;
	};
}

#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.hpp"
