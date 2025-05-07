// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust_Proxy.h"
#include "Malterlib_Concurrency_DistributedTrust.h"
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>

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
			if (_ToCheck.f_IsEmpty())
				return true;
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
	
	TCFuture<TCSet<CDistributedActorTrustManager_Address>> CDistributedActorTrustManagerProxy::f_EnumListens()
	{
		if (!fp_CheckPermissions(EPermission_Listen_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumListens);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_AddListen(CDistributedActorTrustManager_Address _Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Add))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddListen, _Address);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemoveListen(CDistributedActorTrustManager_Address _Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Remove))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, _Address);
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_SetPrimaryListen, _Address);
	}

	TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> CDistributedActorTrustManagerProxy::f_GetPrimaryListen()
	{
		if (!fp_CheckPermissions(EPermission_Listen_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_GetPrimaryListen);
	}

	TCFuture<bool> CDistributedActorTrustManagerProxy::f_HasListen(CDistributedActorTrustManager_Address _Address)
	{
		if (!fp_CheckPermissions(EPermission_Listen_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_HasListen, _Address);
	}

	TCFuture<TCMap<CStr, CHostInfo>> CDistributedActorTrustManagerProxy::f_EnumClients()
	{
		if (!fp_CheckPermissions(EPermission_Client_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumClients);
	}
	
	auto CDistributedActorTrustManagerProxy::f_GenerateConnectionTicket(CGenerateConnectionTicket _Command)
		-> TCFuture<CTrustGenerateConnectionTicketResult>
	{
		if (!fp_CheckPermissions(EPermission_Client_Add))
			co_return fp_AccessDenied();

		co_return co_await mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, _Command.m_Address
				, _Command.m_fOnUseTicket
				? TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest)>
				{
					g_ActorFunctor
					(fg_Move(_Command.m_fOnUseTicket.f_GetSubscription()))
					(fg_Move(_Command.m_fOnUseTicket.f_GetActor()))
					/ [fOnUseTicket = fg_Move(_Command.m_fOnUseTicket.f_GetFunctor())]
					(NStr::CStr &&_HostID, CCallingHostInfo &&_HostInfo, NContainer::CByteVector &&_CertificateRequest) mutable -> TCFuture<void>
					{
						return fOnUseTicket(fg_Move(_HostID), fg_Move(_HostInfo), fg_Move(_CertificateRequest));
					}
				}
				: nullptr
				, _Command.m_fOnCertificateSigned ? TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo)>
				{
					g_ActorFunctor
					(fg_Move(_Command.m_fOnCertificateSigned.f_GetSubscription()))
					(fg_Move(_Command.m_fOnCertificateSigned.f_GetActor()))
					/ [fOnCertificateSigned = fg_Move(_Command.m_fOnCertificateSigned.f_GetFunctor())]
					(NStr::CStr &&_HostID, CCallingHostInfo &&_HostInfo) mutable -> TCFuture<void>
					{
						return fOnCertificateSigned(fg_Move(_HostID), fg_Move(_HostInfo));
					}
				}
				: nullptr
			)
		;
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemoveClient(CStr _HostID)
	{
		if (!fp_CheckPermissions(EPermission_Client_Remove))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID);
	}
	
	TCFuture<bool> CDistributedActorTrustManagerProxy::f_HasClient(CStr _HostID)
	{
		if (!fp_CheckPermissions(EPermission_Client_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_HasClient, _HostID);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumClientConnections() -> TCFuture<TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>>
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections);
	}
	
	TCFuture<CHostInfo> CDistributedActorTrustManagerProxy::f_AddClientConnection(CTrustTicket _TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, _TrustTicket, _Timeout, _ConnectionConcurrency);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, _Address, _ConnectionConcurrency);
	}
	
	TCFuture<CHostInfo> CDistributedActorTrustManagerProxy::f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, _Address, _ConnectionConcurrency);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemoveClientConnection(CRemoveClientConnection _Command)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Remove))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, _Command.m_Address, _Command.m_bPreserveHost);
	}
	
	TCFuture<bool> CDistributedActorTrustManagerProxy::f_HasClientConnection(CDistributedActorTrustManager_Address _Address)
	{
		if (!fp_CheckPermissions(EPermission_ClientConnection_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_HasClientConnection, _Address);
	}

	TCFuture<CStr> CDistributedActorTrustManagerProxy::f_GetHostID() const
	{
		if (!fp_CheckPermissions(EPermission_HostID_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_GetHostID);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumNamespacePermissions(bool _bIncludeHostInfo) -> TCFuture<TCMap<CStr, CNamespacePermissions>>
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeHostInfo);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_AllowHostsForNamespace(CChangeNamespaceHosts _Command)
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Add))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Namespace, mp_Permissions.m_AllowedNamespaces))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Hosts, mp_Permissions.m_AllowedHosts))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Command.m_Namespace, _Command.m_Hosts, _Command.m_OrderingFlags);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_DisallowHostsForNamespace(CChangeNamespaceHosts _Command)
	{
		if (!fp_CheckPermissions(EPermission_NamespacePermissions_Remove))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Namespace, mp_Permissions.m_AllowedNamespaces))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Hosts, mp_Permissions.m_AllowedHosts))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Command.m_Namespace, _Command.m_Hosts, _Command.m_OrderingFlags);
	}

	TCFuture<CDistributedActorTrustManagerInterface::CEnumPermissionsResult> CDistributedActorTrustManagerProxy::f_EnumPermissions(bool _bIncludeHostInfo)
	{
		if (!fp_CheckPermissions(EPermission_Permissions_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumPermissions, _bIncludeHostInfo);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_AddPermissions(CAddPermissions _Command)
	{
		if (!fp_CheckPermissions(EPermission_Permissions_Add))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Identity.f_GetHostID(), mp_Permissions.m_AllowedHosts))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Identity.f_GetUserID(), mp_Permissions.m_AllowedUsers))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Permissions, mp_Permissions.m_AllowedPermissions))
			co_return fp_AccessDenied();

		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddPermissions, _Command.m_Identity, _Command.m_Permissions, _Command.m_OrderingFlags);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemovePermissions(CRemovePermissions _Command)
	{
		if (!fp_CheckPermissions(EPermission_Permissions_Remove))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Identity.f_GetHostID(), mp_Permissions.m_AllowedHosts))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Identity.f_GetUserID(), mp_Permissions.m_AllowedUsers))
			co_return fp_AccessDenied();
		if (!fg_AllAllowedInSet(_Command.m_Permissions, mp_Permissions.m_AllowedPermissions))
			co_return fp_AccessDenied();

		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemovePermissions, _Command.m_Identity, _Command.m_Permissions, _Command.m_OrderingFlags);
	}

	TCFuture<TCMap<CStr, CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManagerProxy::f_EnumUsers(bool _bIncludeFullInfo)
	{
		if (!fp_CheckPermissions(EPermission_User_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumUsers, _bIncludeFullInfo);
	}

	TCFuture<TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManagerProxy::f_TryGetUser(CStr _UserID)
	{
		if (!fp_CheckPermissions(EPermission_User_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_TryGetUser, _UserID);
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_AddUser(CStr _UserID, CStr _UserName)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddUser, _UserID, _UserName);
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemoveUser(CStr _UserID)
	{
		if (!fp_CheckPermissions(EPermission_User_Remove))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemoveUser, _UserID);
	}
	
	TCFuture<void> CDistributedActorTrustManagerProxy::f_SetUserInfo
		(
			CStr _UserID
			, TCOptional<CStr> _UserName
			, TCSet<CStr> _RemoveMetadata
			, TCMap<CStr, CEJsonSorted> _AddMetadata
		)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo, _UserID, _UserName, _RemoveMetadata, _AddMetadata);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumAuthenticationActors() const -> TCFuture<TCMap<CStr, CLocalAuthenticationActorInfo>>
	{
		if (!fp_CheckPermissions(EPermission_User_Read))
			co_return fp_AccessDenied();

		auto Factors = co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors);

		TCMap<CStr, CLocalAuthenticationActorInfo> OutFactors;
		for (auto &Factor : Factors)
			OutFactors[Factors.fs_GetKey(Factor)].m_Category = Factor.m_Category;

		co_return fg_Move(OutFactors);
	}

	auto CDistributedActorTrustManagerProxy::f_EnumUserAuthenticationFactors(NStr::CStr _UserID) const -> TCFuture<NContainer::TCMap<NStr::CStr, CLocalAuthenticationData>>
	{
		if (!fp_CheckPermissions(EPermission_User_Read))
			co_return fp_AccessDenied();

		auto Factors = co_await mp_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _UserID);

		if (!fp_CheckPermissions(EPermission_UserPrivate_Read))
		{
			for (auto &Factor : Factors)
				Factor.m_PrivateData.f_Clear();
		}
		co_return fg_Move(Factors);
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_AddUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CLocalAuthenticationData _Data)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_AddUserAuthenticationFactor, _UserID, _FactorID, fg_Move(_Data));
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_SetUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CLocalAuthenticationData _Data)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_SetUserAuthenticationFactor, _UserID, _FactorID, fg_Move(_Data));
	}

	TCFuture<void> CDistributedActorTrustManagerProxy::f_RemoveUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID)
	{
		if (!fp_CheckPermissions(EPermission_User_Write))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, _UserID, _FactorID);
	}

	auto CDistributedActorTrustManagerProxy::f_GetConnectionsDebugStats() -> TCFuture<CConnectionsDebugStats>
	{
		if (!fp_CheckPermissions(EPermission_ConnectionsDebugStats_Read))
			co_return fp_AccessDenied();
		co_return co_await mp_TrustManager(&CDistributedActorTrustManager::f_GetConnectionsDebugStats);
	}
}
