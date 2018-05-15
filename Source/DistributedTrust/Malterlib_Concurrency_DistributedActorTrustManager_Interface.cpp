// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.h"
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

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
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemovePermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumUsers);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_TryGetUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetUserInfo);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumUserAuthenticationFactors);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumAuthenticationActors);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddUserAuthenticationFactor);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetUserAuthenticationFactor);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveUserAuthenticationFactor);
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
		_Stream << fg_Move(m_NotificationsSubscription);
	}
	
	void CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult::f_Consume(CDistributedActorReadStream &_Stream)
	{
		_Stream >> m_Ticket;
		_Stream >> m_NotificationsSubscription;
	}
		
	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CClientConnectionInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostInfo;
		_Stream % m_ConnectionConcurrency;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CClientConnectionInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CChangeNamespaceHosts::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Namespace;
		_Stream % m_Hosts;
		if (_Stream.f_GetVersion() >= 0x103)
			_Stream % m_OrderingFlags;
	};
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CChangeNamespaceHosts);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CAddPermissions::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Identity;
		_Stream % m_Permissions;
		_Stream % m_OrderingFlags;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CAddPermissions);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CRemovePermissions::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Identity;
		_Stream % m_Permissions;
		_Stream % m_OrderingFlags;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CRemovePermissions);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CGenerateConnectionTicket::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Address;
		_Stream % fg_Move(m_fOnUseTicket);
		if (_Stream.f_GetVersion() >= 0x104)
			_Stream % fg_Move(m_fOnCertificateSigned);
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CGenerateConnectionTicket);

	bool CDistributedActorTrustManagerInterface::CClientConnectionInfo::operator == (CClientConnectionInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_HostInfo, m_ConnectionConcurrency) == NContainer::fg_TupleReferences(_Right.m_HostInfo, _Right.m_ConnectionConcurrency);
	}
		
	bool CDistributedActorTrustManagerInterface::CClientConnectionInfo::operator < (CClientConnectionInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_HostInfo, m_ConnectionConcurrency) < NContainer::fg_TupleReferences(_Right.m_HostInfo, _Right.m_ConnectionConcurrency);
	}

	bool CDistributedActorTrustManagerInterface::CUserInfo::operator == (CDistributedActorTrustManagerInterface::CUserInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_UserName, m_Metadata) == NContainer::fg_TupleReferences(_Right.m_UserName, _Right.m_Metadata);
	}

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CUserInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UserName;
		_Stream % m_Metadata;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo, NStream::CBinaryStreamMemory<>, Feed);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo, NStream::CBinaryStreamMemoryPtr<>, Consume);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalCallingHostInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % mp_UniqueHostID;
		_Stream % mp_HostInfo;
		_Stream % mp_LastExecutionID;
		_Stream % mp_ProtocolVersion;
		if (_Stream.f_GetVersion() >= 0x105)
			_Stream % mp_ClaimedUserID;
		if constexpr (tf_CStream::mc_Direction == NStream::EStreamDirection_Consume)
			mp_DistributionManager = _Stream.f_GetState().m_DistributionManager;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalCallingHostInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalAuthenticationData::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Category;
		_Stream % m_Name;
		_Stream % m_PublicData;
		_Stream % m_PrivateData;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData, NStream::CBinaryStreamMemory<>, Feed);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData, NStream::CBinaryStreamMemoryPtr<>, Consume);

	CPermissionIdentifiers const &CDistributedActorTrustManagerInterface::CPermissionInfo::f_GetIdentifiers() const
	{
		return NContainer::TCMap<CPermissionIdentifiers, CPermissionInfo>::fs_GetKey(*this);
	}

	NStr::CStr CDistributedActorTrustManagerInterface::CPermissionInfo::f_GetDesc() const
	{
		using namespace NStr;
		CStr Result;

		if (m_HostInfo)
			Result += "Host: {}"_f << m_HostInfo->f_GetDesc();
		if (m_UserInfo)
			fg_AddStrSep(Result, "User: {} [{}]"_f << f_GetIdentifiers().f_GetUserID() << m_UserInfo->m_UserName, " ");
		if (!m_AuthenticationFactors.f_IsEmpty())
		{
			fg_AddStrSep(Result, "Authentication methods:", " ");
			for (auto const &Factors : m_AuthenticationFactors)
			fg_AddStrSep(Result, "{vs}"_f << Factors, " ");
		}

		return Result;
	}

	bool CDistributedActorTrustManagerInterface::CPermissionInfo::operator == (CPermissionInfo const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_HostInfo, m_UserInfo) == NContainer::fg_TupleReferences(_Right.m_HostInfo, _Right.m_UserInfo);
	}

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CPermissionInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostInfo;
		_Stream % m_UserInfo;
		_Stream % m_AuthenticationFactors;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CPermissionInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalPermissionRequirements::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_AuthenticationFactors;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalPermissionRequirements);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CEnumPermissionsResult::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Permissions;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CEnumPermissionsResult);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalAuthenticationActorInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Category;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationActorInfo);
}
