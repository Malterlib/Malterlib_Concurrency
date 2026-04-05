// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include <Mib/Concurrency/RuntimeTypeRegistry>

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedActorCommand_Identify::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_Identify);
		_Stream << m_ProtocolVersion;
		_Stream << m_ExecutionID;
		_Stream << m_LastSeenExecutionID;

		auto *pDefaultState = m_PriorityQueueStates.f_FindEqual(128);
		if (pDefaultState)
		{
			_Stream << pDefaultState->m_MissingPacketIDs;
			_Stream << pDefaultState->m_AckedPacketID;
			_Stream << pDefaultState->m_HighestSeenPacketID;
		}
		else
		{
			_Stream << NContainer::TCVector<uint64>();
			_Stream << uint64(0);
			_Stream << uint64(0);
		}

		_Stream << m_AllowedNamespaces;
		_Stream << m_bAllowAllNamespaces;
		_Stream << m_FriendlyName;

		if (m_ProtocolVersion >= EDistributedActorProtocolVersion_PrioritySupport)
		{
			if (pDefaultState)
			{
				auto nStates = m_PriorityQueueStates.f_GetLen() - 1;
				NStream::fg_FeedLenToStream(_Stream, nStates);
				for (auto &Entry : m_PriorityQueueStates.f_Entries())
				{
					if (Entry.f_Key() == 128)
						continue;

					_Stream << Entry.f_Key();
					_Stream << Entry.f_Value();
				}
			}
			else
				_Stream << m_PriorityQueueStates;
		}
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Identify::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_ProtocolVersion;
		if (m_ProtocolVersion < EDistributedActorProtocolVersion_Min)
			return;
		_Stream >> m_ExecutionID;
		_Stream >> m_LastSeenExecutionID;

		auto &State = m_PriorityQueueStates[128];
		_Stream >> State.m_MissingPacketIDs;
		_Stream >> State.m_AckedPacketID;
		_Stream >> State.m_HighestSeenPacketID;

		_Stream >> m_AllowedNamespaces;
		_Stream >> m_bAllowAllNamespaces;
		_Stream >> m_FriendlyName;

		if (m_ProtocolVersion >= EDistributedActorProtocolVersion_PrioritySupport)
		{
			uint64 nStates;
			NStream::fg_ConsumeLenFromStream(_Stream, nStates);
			fg_CheckLengthLimit(_Stream, nStates);
			while (nStates)
			{
				uint8 Priority;
				_Stream >> Priority;
				_Stream >> m_PriorityQueueStates[Priority];
				--nStates;
			}
		}
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Acknowledge::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_Acknowledge);
		_Stream << m_LastInOrderPacketID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Acknowledge::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_LastInOrderPacketID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCall::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCall);
		_Stream << m_PacketID;
		_Stream << m_ActorID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCall::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithAuthHandler::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCallWithAuthHandler);
		_Stream << m_PacketID;
		_Stream << m_ActorID;
		_Stream << m_AuthenticationHandlerID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithAuthHandler::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
		_Stream >> m_AuthenticationHandlerID;
	}

	template <typename tf_CStream>
	void CPriorityQueueState::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_MissingPacketIDs;
		_Stream % m_AckedPacketID;
		_Stream % m_HighestSeenPacketID;
	}

	template <typename tf_CStream>
	void CPriorityMissingInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_MissingPacketIDs;
		_Stream % m_HighestSeenPacketID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithPriority::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCallWithPriority);
		_Stream << m_Priority;
		_Stream << m_PacketID;
		_Stream << m_ActorID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithPriority::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Priority;
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler);
		_Stream << m_Priority;
		_Stream << m_PacketID;
		_Stream << m_ActorID;
		_Stream << m_AuthenticationHandlerID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Priority;
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
		_Stream >> m_AuthenticationHandlerID;
	}

	template <typename tf_CStream>
	void CResultSubscriptionData::f_Feed(tf_CStream &_Stream) const
	{
		if (m_ClaimedSubscriptionIDs.f_IsEmpty())
		{
			_Stream << uint8(0);
			return;
		}
		_Stream << uint8(1);
		_Stream << m_ClaimedSubscriptionIDs;
	}

	template <typename tf_CStream>
	void CResultSubscriptionData::f_Consume(tf_CStream &_Stream)
	{
		uint8 bEnabled;
		_Stream >> bEnabled;
		if (!bEnabled)
			return;
		_Stream >> m_ClaimedSubscriptionIDs;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallResult::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCallResult);
		_Stream << m_PacketID;
		_Stream << m_ReplyToPacketID;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_ClaimedSubscriptionsSupported)
			_Stream << m_SubscriptionData;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallResult::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_ReplyToPacketID;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_ClaimedSubscriptionsSupported)
			_Stream >> m_SubscriptionData;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallResultWithPriority::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RemoteCallResultWithPriority);
		_Stream << m_Priority;
		_Stream << m_PacketID;
		_Stream << m_ReplyToPacketID;
		_Stream << m_SubscriptionData;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RemoteCallResultWithPriority::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Priority;
		_Stream >> m_PacketID;
		_Stream >> m_ReplyToPacketID;
		_Stream >> m_SubscriptionData;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Publish::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_Publish);
		_Stream << m_PacketID;
		_Stream << m_ActorID;
		_Stream << m_Namespace;
		_Stream << m_Hierarchy;
		_Stream << m_ProtocolVersions.m_MinSupported;
		_Stream << m_ProtocolVersions.m_MaxSupported;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			_Stream << m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Publish::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
		_Stream >> m_Namespace;
		_Stream >> m_Hierarchy;
		_Stream >> m_ProtocolVersions.m_MinSupported;
		_Stream >> m_ProtocolVersions.m_MaxSupported;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			_Stream >> m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_PublishFinished::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_PublishFinished);
		_Stream << m_PacketID;
		_Stream << m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_PublishFinished::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_UnpublishFinished::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_UnpublishFinished);
		_Stream << m_PacketID;
		_Stream << m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_UnpublishFinished::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Unpublish::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_Unpublish);
		_Stream << m_PacketID;
		_Stream << m_ActorID;
		_Stream << m_Namespace;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			_Stream << m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_Unpublish::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_ActorID;
		_Stream >> m_Namespace;
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_WaitForRemotePublishProcessing)
			_Stream >> m_WaitPublicationID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_DestroySubscription::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_DestroySubscription);
		_Stream << m_PacketID;
		_Stream << m_SubscriptionID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_DestroySubscription::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PacketID;
		_Stream >> m_SubscriptionID;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_InitialPublishFinished::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_InitialPublishFinished);
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_InitialPublishFinished::f_Consume(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_InitialPublishFinishedProcessing::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_InitialPublishFinishedProcessing);
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_InitialPublishFinishedProcessing::f_Consume(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_SubscriptionDestroyed::f_Feed(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const
	{
		_Stream << uint8(EDistributedActorCommand_SubscriptionDestroyed);
		_Stream << m_PacketID;
		_Stream << m_SubscriptionID;
		if (m_Result)
			_Stream << uint8(0);
		else
			NPrivate::fg_StreamAsyncResultException(_Stream, m_Result, _ActorProtocolVersion);
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_SubscriptionDestroyed::f_Consume(tf_CStream &_Stream, uint32 _ActorProtocolVersion)
	{
		_Stream >> m_PacketID;
		_Stream >> m_SubscriptionID;
		if (!NPrivate::fg_CopyReplyToAsyncResultShared(_Stream, m_Result, _ActorProtocolVersion))
			m_Result.f_SetResult();
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_NotifyConnectionLost::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_NotifyConnectionLost);
		_Stream << m_NotifyLostSequence;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_NotifyConnectionLost::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_NotifyLostSequence;
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RequestMissingPackets::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << uint8(EDistributedActorCommand_RequestMissingPackets);

		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_PrioritySupport)
			_Stream << m_PriorityQueueStates;
		else
		{
			auto *pDefaultStates = m_PriorityQueueStates.f_FindEqual(128);
			if (pDefaultStates)
				_Stream << *pDefaultStates;
			else
			{
				_Stream << NContainer::TCVector<uint64>();
				_Stream << uint64(0);
			}
		}
	}

	template <typename tf_CStream>
	void CDistributedActorCommand_RequestMissingPackets::f_Consume(tf_CStream &_Stream)
	{
		if (_Stream.f_GetVersion() >= EDistributedActorProtocolVersion_PrioritySupport)
			_Stream >> m_PriorityQueueStates;
		else
			_Stream >> m_PriorityQueueStates[128];
	}
}
