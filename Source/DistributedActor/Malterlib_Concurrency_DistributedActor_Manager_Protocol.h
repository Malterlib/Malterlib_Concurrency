// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency
{
	enum EDistributedActorCommand : uint8
	{
		EDistributedActorCommand_Identify
		, EDistributedActorCommand_Acknowledge
		, EDistributedActorCommand_RemoteCall
		, EDistributedActorCommand_RemoteCallResult
		, EDistributedActorCommand_Publish
		, EDistributedActorCommand_Unpublish
		, EDistributedActorCommand_DestroySubscription
		, EDistributedActorCommand_InitialPublishFinished
		, EDistributedActorCommand_SubscriptionDestroyed
		, EDistributedActorCommand_NotifyConnectionLost
		, EDistributedActorCommand_RequestMissingPackets
		, EDistributedActorCommand_InitialPublishFinishedProcessing
		, EDistributedActorCommand_PublishFinished
		, EDistributedActorCommand_UnpublishFinished
	};

	struct CDistributedActorCommand_Identify
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint32 m_ProtocolVersion = EDistributedActorProtocolVersion_Current;
		NStr::CStr m_ExecutionID;
		NStr::CStr m_LastSeenExecutionID;
		NContainer::TCVector<uint64> m_MissingPacketIDs;
		uint64 m_AckedPacketID = 0;
		uint64 m_HighestSeenPacketID = 0;
		NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;
		uint8 m_bAllowAllNamespaces;
		NStr::CStr m_FriendlyName;
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

	struct CDistributedActorCommand_RequestMissingPackets
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::TCVector<uint64> m_MissingPacketIDs;
		uint64 m_HighestSeenPacketID = 0;
	};
}

#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.hpp"
