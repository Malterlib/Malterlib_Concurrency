// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.h"

namespace NMib::NConcurrency
{
	CDistributedActorTrustManagerInterface::CDistributedActorTrustManagerInterface()
	{
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumListens);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumClients);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveClient);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasClient);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumClientConnections);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddAdditionalClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetClientConnectionConcurrency);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_GetHostID);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumNamespacePermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_DisallowHostsForNamespace);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumHostPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddHostPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveHostPermissions);
	}
	
	CDistributedActorTrustManagerInterface::~CDistributedActorTrustManagerInterface() = default;

	void CDistributedActorTrustManagerInterface::CNamespacePermissions::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		_Stream << m_AllowedHosts;
		_Stream << m_DisallowedHosts;
	}
	
	void CDistributedActorTrustManagerInterface::CNamespacePermissions::f_Consume(CDistributedActorReadStream &_Stream)
	{
		_Stream >> m_AllowedHosts;
		_Stream >> m_DisallowedHosts;
	}		

	void CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult::f_Feed(CDistributedActorWriteStream &_Stream) &&
	{
		_Stream << m_Ticket;
		_Stream << fg_Move(m_OnUseTicketSubscription);
	}
	
	void CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult::f_Consume(CDistributedActorReadStream &_Stream)
	{
		_Stream >> m_Ticket;
		_Stream >> m_OnUseTicketSubscription;
	}
		
	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CClientConnectionInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostInfo;
		_Stream % m_ConnectionConcurrency;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CClientConnectionInfo);
		
	bool CDistributedActorTrustManagerInterface::CClientConnectionInfo::operator == (CClientConnectionInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_HostInfo, m_ConnectionConcurrency) == NContainer::fg_TupleReferences(_Right.m_HostInfo, _Right.m_ConnectionConcurrency);
	}
		
	bool CDistributedActorTrustManagerInterface::CClientConnectionInfo::operator < (CClientConnectionInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_HostInfo, m_ConnectionConcurrency) < NContainer::fg_TupleReferences(_Right.m_HostInfo, _Right.m_ConnectionConcurrency);
	}
}
