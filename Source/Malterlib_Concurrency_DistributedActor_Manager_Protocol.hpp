// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void CDistributedActorCommand_Identify::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << uint8(EDistributedActorCommand_Identify);
			_Stream << m_ProtocolVersion;
			_Stream << m_ExecutionID;
			_Stream << m_LastSeenExecutionID;
			_Stream << m_MissingPacketIDs;
			_Stream << m_AckedPacketID;
			_Stream << m_HighestSeenPacketID;
			_Stream << m_AllowedNamespaces;
			_Stream << m_bAllowAllNamespaces;
		}
			
		template <typename tf_CStream>
		void CDistributedActorCommand_Identify::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_ProtocolVersion;
			_Stream >> m_ExecutionID;
			_Stream >> m_LastSeenExecutionID;
			_Stream >> m_MissingPacketIDs;
			_Stream >> m_AckedPacketID;
			_Stream >> m_HighestSeenPacketID;
			_Stream >> m_AllowedNamespaces;
			_Stream >> m_bAllowAllNamespaces;
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
		void CDistributedActorCommand_RemoteCallResult::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << uint8(EDistributedActorCommand_RemoteCallResult);
			_Stream << m_PacketID;
			_Stream << m_ReplyToPacketID;
		}
		
		template <typename tf_CStream>
		void CDistributedActorCommand_RemoteCallResult::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PacketID;
			_Stream >> m_ReplyToPacketID;
		}
		
		
		template <typename tf_CStream>
		void CDistributedActorCommand_Publish::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << uint8(EDistributedActorCommand_Publish);
			_Stream << m_PacketID;
			_Stream << m_ActorID;
			_Stream << m_Namespace;
			_Stream << m_Hierarchy;
		}
		
		template <typename tf_CStream>
		void CDistributedActorCommand_Publish::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PacketID;
			_Stream >> m_ActorID;
			_Stream >> m_Namespace;
			_Stream >> m_Hierarchy;
		}
		
		template <typename tf_CStream>
		void CDistributedActorCommand_Unpublish::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << uint8(EDistributedActorCommand_Unpublish);
			_Stream << m_PacketID;
			_Stream << m_ActorID;
			_Stream << m_Namespace;
		}
		
		template <typename tf_CStream>
		void CDistributedActorCommand_Unpublish::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PacketID;
			_Stream >> m_ActorID;
			_Stream >> m_Namespace;
		}
	}
}
