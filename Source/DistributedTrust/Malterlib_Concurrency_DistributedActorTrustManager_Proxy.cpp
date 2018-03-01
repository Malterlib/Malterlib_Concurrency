// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_Proxy.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NEncoding;

	namespace
	{
		bool fg_AllAllowedInSet(TCSet<CStr> const &_ToCheck, TCSet<CStr> const &_Allowed)
		{
			if (_Allowed.f_IsEmpty())
				return true;
			for (auto iToCheck = _ToCheck.f_GetIterator(); iToCheck; ++iToCheck)
			{
				if (!_Allowed.f_Exists(iToCheck.f_GetKey()))
					return false;
			}
			return true;
		}

		bool fg_AllAllowedInSet(CStr const &_ToCheck, TCSet<CStr> const &_Allowed)
		{
			if (_Allowed.f_IsEmpty())
				return true;
			return _Allowed.f_Exists(_ToCheck);
		}
	}
	
	CDistributedActorTrustManagerProxy::CDistributedActorTrustManagerProxy(TCActor<CDistributedActorTrustManager> const &_Actor, CPermissions const &_Permissions)
		: mp_TrustManager(_Actor)
		, mp_Permissions(_Permissions)
	{
	}
	
	CDistributedActorTrustManagerProxy::~CDistributedActorTrustManagerProxy() = default;

	NException::CException CDistributedActorTrustManagerProxy::fp_AccessDenied() const
	{
		return DMibErrorInstance("Access Denied");
	}
	
	bool CDistributedActorTrustManagerProxy::fp_CheckPermissions(EPermission _Permissions) const
	{
		if ((mp_Permissions.m_Permissions & _Permissions) != _Permissions)
			return false;
		return true;
	}
	
	TCContinuation<TCSet<CDistributedActorTrustManager_Address>> CDistributedActorTrustManagerProxy::f_EnumListens()
	{
		if (!fp_CheckPermissions(EPermission_Listen_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumListens);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_AddListen(CDistributedActorTrustManager_Address const &_Address) 
	{
		if (!fp_CheckPermissions(EPermission_Listen_Add))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AddListen, _Address);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_RemoveListen(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Remove))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, _Address);
	}
	
	TCContinuation<bool> CDistributedActorTrustManagerProxy::f_HasListen(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_HasListen, _Address);
	}

	TCContinuation<TCMap<CStr, CHostInfo>> CDistributedActorTrustManagerProxy::f_EnumClients()
	{
		if (!fp_CheckPermissions(EPermission_Client_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumClients);
	}
	
	auto CDistributedActorTrustManagerProxy::f_GenerateConnectionTicket
		(
			CDistributedActorTrustManager_Address const &_Address
			, TCActorFunctorWithID
			<
				TCContinuation<void> 
				(
					CStr const &_HostID
					, CCallingHostInfo const &_HostInfo
					, TCVector<uint8> const &_CertificateRequest
				)
				, 0
			> 
			&&_fOnUseTicket
		)
		-> TCContinuation<CTrustGenerateConnectionTicketResult> 
	{
		if (!fp_CheckPermissions(EPermission_Client_Add))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, _Address, fg_Move(_fOnUseTicket));
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_RemoveClient(CStr const &_HostID)
	{
		if (!fp_CheckPermissions(EPermission_Client_Remove))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID);
	}
	
	TCContinuation<bool> CDistributedActorTrustManagerProxy::f_HasClient(CStr const &_HostID)
	{
		if (!fp_CheckPermissions(EPermission_Client_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_HasClient, _HostID);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumClientConnections() -> TCContinuation<TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>>
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections);
	}
	
	TCContinuation<CHostInfo> CDistributedActorTrustManagerProxy::f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, _TrustTicket, _Timeout, _ConnectionConcurrency);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, _Address, _ConnectionConcurrency);
	}
	
	TCContinuation<CHostInfo> CDistributedActorTrustManagerProxy::f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, _Address, _ConnectionConcurrency);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Remove))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, _Address);
	}
	
	TCContinuation<bool> CDistributedActorTrustManagerProxy::f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_HasClientConnection, _Address);
	}

	TCContinuation<CStr> CDistributedActorTrustManagerProxy::f_GetHostID() const
	{
		if (!fp_CheckPermissions(EPermission_HostID_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_GetHostID);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumNamespacePermissions(bool _bIncludeHostInfo) -> TCContinuation<TCMap<CStr, CNamespacePermissions>> 
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeHostInfo);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_AllowHostsForNamespace(CChangeNamespaceHosts const &_Command)
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Add))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Namespace, mp_Permissions.m_AllowedNamespaces))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Hosts, mp_Permissions.m_AllowedHosts))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Command.m_Namespace, _Command.m_Hosts, _Command.m_OrderingFlags);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_DisallowHostsForNamespace(CChangeNamespaceHosts const &_Command)
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Remove))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Namespace, mp_Permissions.m_AllowedNamespaces))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Hosts, mp_Permissions.m_AllowedHosts))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Command.m_Namespace, _Command.m_Hosts, _Command.m_OrderingFlags);
	}

	TCContinuation<TCMap<CStr, TCMap<CStr, CHostInfo>>> CDistributedActorTrustManagerProxy::f_EnumHostPermissions(bool _bIncludeHostInfo)
	{
		if (!fp_CheckPermissions(EPermission_HostPermissions_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, _bIncludeHostInfo);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_AddHostPermissions(CChangeHostPermissions const &_Command)
	{
		if (!fp_CheckPermissions(EPermission_HostPermissions_Add))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_HostID, mp_Permissions.m_AllowedHosts))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Permissions, mp_Permissions.m_AllowedPermissions))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AddHostPermissions, _Command.m_HostID, _Command.m_Permissions, _Command.m_OrderingFlags);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_RemoveHostPermissions(CChangeHostPermissions const &_Command)
	{
		if (!fp_CheckPermissions(EPermission_HostPermissions_Remove))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_HostID, mp_Permissions.m_AllowedHosts))
			return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Permissions, mp_Permissions.m_AllowedPermissions))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_RemoveHostPermissions, _Command.m_HostID, _Command.m_Permissions, _Command.m_OrderingFlags);
	}

	TCContinuation<TCMap<CStr, CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManagerProxy::f_EnumUsers(bool _bIncludeFullInfo)
	{
		if (!fp_CheckPermissions(EPermission_User_Read))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_EnumUsers, _bIncludeFullInfo);
	}

	TCContinuation<void> CDistributedActorTrustManagerProxy::f_AddUser(CStr const &_UserID, CStr const &_UserName)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_AddUser, _UserID, _UserName);
	}

	TCContinuation<void> CDistributedActorTrustManagerProxy::f_RemoveUser(CStr const &_UserID)
	{
		if (!fp_CheckPermissions(EPermission_User_Remove))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_RemoveUser, _UserID);
	}
	
	TCContinuation<void> CDistributedActorTrustManagerProxy::f_SetUserInfo
		(
			CStr const &_UserID
			, TCOptional<CStr> const &_UserName
			, TCSet<CStr> const &_RemoveMetadata
			, TCMap<CStr, CEJSON> const &_AddMetadata
		)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			return fp_AccessDenied();
		return mp_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo, _UserID, _UserName, _RemoveMetadata, _AddMetadata);
	}
}
