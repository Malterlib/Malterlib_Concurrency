// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Encoding/JSONShortcuts>
#include "Malterlib_Concurrency_DistributedTrust_TestHelpers.h"
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor_TestHelpers.h"

namespace NMib::NConcurrency
{
	using namespace NDistributedActorTrustManagerDatabase;
	using namespace NStr;
	
	TCFuture<CBasicConfig> CTrustManagerDatabaseTestHelper::f_GetBasicConfig()
	{
		co_return m_BasicConfig;
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetBasicConfig(CBasicConfig const &_BasicConfig)
	{
		m_BasicConfig = _BasicConfig;
		co_return {};
	}
	
	TCFuture<int32> CTrustManagerDatabaseTestHelper::f_GetNewCertificateSerial()
	{
		co_return ++m_CertificateSerial;
	}
	
	TCFuture<CDefaultUser> CTrustManagerDatabaseTestHelper::f_GetDefaultUser()
	{
		co_return m_DefaultUser;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetDefaultUser(CDefaultUser const &_DefaultUser)
	{
		m_DefaultUser = _DefaultUser;
		co_return {};
	}

	TCFuture<NContainer::TCMap<CStr, CServerCertificate>> CTrustManagerDatabaseTestHelper::f_EnumServerCertificates(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CStr, CServerCertificate> Return;
		for (auto iCertificate = m_ServerCertificates.f_GetIterator(); iCertificate; ++iCertificate)
		{
			auto &Certficate = Return[iCertificate.f_GetKey()];
			if (_bIncludeFullInfo)
				Certficate = *iCertificate; 
		}
		co_return Return;
	}
	
	TCFuture<CServerCertificate> CTrustManagerDatabaseTestHelper::f_GetServerCertificate(CStr const &_HostName)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			co_return DMibErrorInstance("No server certificate for host");
		co_return *pServerCertificate;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		if (!m_ServerCertificates(_HostName, _Certificate).f_WasCreated())
			co_return DMibErrorInstance("Server certificate already exists");
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			co_return DMibErrorInstance("No server certificate for host");
		*pServerCertificate = _Certificate;
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveServerCertificate(CStr const &_HostName)
	{
		if (!m_ServerCertificates.f_Remove(_HostName))
			co_return DMibErrorInstance("No server certificate for host");
		co_return {};
	}
	
	TCFuture<NContainer::TCSet<CListenConfig>> CTrustManagerDatabaseTestHelper::f_EnumListenConfigs()
	{
		NContainer::TCSet<CListenConfig> Return;
		for (auto iListenConfig = m_ListenConfigs.f_GetIterator(); iListenConfig; ++iListenConfig)
			Return[iListenConfig.f_GetKey()];
		co_return Return;
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs(_Config).f_WasCreated())
			co_return DMibErrorInstance("Listen config already exists");
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs.f_Remove(_Config))
			co_return DMibErrorInstance("Listen config does not exist");
		
		co_return {};
	}
	
	TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> CTrustManagerDatabaseTestHelper::f_GetPrimaryListen()
	{
		co_return m_PrimaryListen;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> const &_Address)
	{
		m_PrimaryListen = _Address;

		co_return {};
	}

	TCFuture<NContainer::TCMap<CStr, CClient>> CTrustManagerDatabaseTestHelper::f_EnumClients(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CStr, CClient> Return;
		for (auto iClient = m_Clients.f_GetIterator(); iClient; ++iClient)
		{
			auto &Client = Return[iClient.f_GetKey()];
			if (_bIncludeFullInfo)
				Client = *iClient; 
		}
		co_return Return;
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddClient(CStr const &_HostID, CClient const &_Client)
	{
		if (!m_Clients(_HostID, _Client).f_WasCreated())
			co_return DMibErrorInstance("Client already exists");
		co_return {};
	}
	
	TCFuture<CClient> CTrustManagerDatabaseTestHelper::f_GetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			co_return DMibErrorInstance("No client for host ID");
		co_return *pClient;
	}
	
	TCFuture<NStorage::TCUniquePointer<CClient>> CTrustManagerDatabaseTestHelper::f_TryGetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			co_return nullptr;
		co_return fg_Construct(*pClient);
	}

	TCFuture<NStr::CStr> CTrustManagerDatabaseTestHelper::f_GetClientLastFriendlyName(NStr::CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			co_return {};
		co_return pClient->m_LastFriendlyName;
	}

	TCFuture<bool> CTrustManagerDatabaseTestHelper::f_HasClient(CStr const &_HostID)
	{
		co_return m_Clients.f_FindEqual(_HostID) != nullptr;
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetClient(CStr const &_HostID, CClient const &_Client)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			co_return DMibErrorInstance("No client for host ID");
		*pClient = _Client;
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveClient(CStr const &_HostID)
	{
		if (!m_Clients.f_Remove(_HostID))
			co_return DMibErrorInstance("No client for host ID");
		co_return {};
	}
	
	TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> CTrustManagerDatabaseTestHelper::f_EnumClientConnections(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> Return;
		for (auto iClientConnection = m_ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
		{
			auto &Connection = Return[iClientConnection.f_GetKey()];
			if (_bIncludeFullInfo)
				Connection = *iClientConnection; 
		}
		co_return Return;
	}
	
	TCFuture<CClientConnection> CTrustManagerDatabaseTestHelper::f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			co_return DMibErrorInstance("No client connection for address");
		co_return *pClientConnection;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		if (!m_ClientConnections(_Address, _ClientConnection).f_WasCreated())
			co_return DMibErrorInstance("Client connection already exists");
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			co_return DMibErrorInstance("No client connection for address");
		*pClientConnection = _ClientConnection;
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!m_ClientConnections.f_Remove(_Address))
			co_return DMibErrorInstance("No client connection for address");
		co_return {};
	}
	
	TCFuture<NContainer::TCMap<CStr, CNamespace>> CTrustManagerDatabaseTestHelper::f_EnumNamespaces(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CStr, CNamespace> Return;
		for (auto iNamespace = m_Namespaces.f_GetIterator(); iNamespace; ++iNamespace)
		{
			auto &Namespace = Return[iNamespace.f_GetKey()];
			if (_bIncludeFullInfo)
				Namespace = *iNamespace; 
		}
		co_return Return;
	}
	
	TCFuture<CNamespace> CTrustManagerDatabaseTestHelper::f_GetNamespace(CStr const &_NamespaceName)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			co_return DMibErrorInstance("No namespace for name");
		co_return *pNamespace;
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		if (!m_Namespaces(_NamespaceName, _Namespace).f_WasCreated())
			co_return DMibErrorInstance("Namespace already exists");
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			co_return DMibErrorInstance("No namespace for name");
		*pNamespace = _Namespace;
		co_return {};
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveNamespace(CStr const &_NamespaceName)
	{
		if (!m_Namespaces.f_Remove(_NamespaceName))
			co_return DMibErrorInstance("No namespace for name");
		co_return {};
	}
	
	TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> CTrustManagerDatabaseTestHelper::f_EnumPermissions(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CPermissionIdentifiers, CPermissions> Return;
		for (auto iPermissions = m_Permissions.f_GetIterator(); iPermissions; ++iPermissions)
		{
			auto &Permissions = Return[iPermissions.f_GetKey()];
			if (_bIncludeFullInfo)
				Permissions = *iPermissions;
		}
		co_return Return;
	}

	TCFuture<CPermissions> CTrustManagerDatabaseTestHelper::f_GetPermissions(CPermissionIdentifiers const &_Identity)
	{
		auto pPermissions = m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			co_return DMibErrorInstance("No host permissions for host ID");
		co_return *pPermissions;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		if (!m_Permissions(_Identity, _Permissions).f_WasCreated())
			co_return DMibErrorInstance("Host permissions already exists for host ID");
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		auto pPermissions = m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			co_return DMibErrorInstance("No host permissions for host ID");
		*pPermissions = _Permissions;
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemovePermissions(CPermissionIdentifiers const &_Identity)
	{
		if (!m_Permissions.f_Remove(_Identity))
			co_return DMibErrorInstance("No host permissions for host ID");
		co_return {};
	}

	TCFuture<NContainer::TCMap<CStr, CUserInfo>> CTrustManagerDatabaseTestHelper::f_EnumUsers(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CStr, CUserInfo> Return;
		for (auto iUsers = m_Users.f_GetIterator(); iUsers; ++iUsers)
		{
			auto &User = Return[iUsers.f_GetKey()];
			User.m_UserName = iUsers->m_UserName;
			User.m_Metadata = iUsers->m_Metadata;
		}
		co_return Return;
	}

	TCFuture<CUserInfo> CTrustManagerDatabaseTestHelper::f_GetUserInfo(CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto pUsers = m_Users.f_FindEqual(_UserID);
		if (!pUsers)
			co_return DMibErrorInstance("No user with ID");
		co_return *pUsers;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddUser(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		if (!m_Users(_UserID, _UserInfo).f_WasCreated())
			co_return DMibErrorInstance("There is already a user with that user ID");
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetUserInfo(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto pUsers = m_Users.f_FindEqual(_UserID);
		if (!pUsers)
			co_return DMibErrorInstance("User '{}' does not exists"_f << _UserID);
		*pUsers = _UserInfo;
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveUser(CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		if (!m_Users.f_Remove(_UserID))
			co_return DMibErrorInstance("There is no user with that user ID");
		co_return {};
	}

	TCFuture<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> CTrustManagerDatabaseTestHelper::f_EnumAuthenticationFactor
		(
			bool _bIncludeFullInfo
		)
	{
		if (_bIncludeFullInfo)
			co_return m_UserAuthenticationFactors;

		auto FactorsWithoutPrivate = m_UserAuthenticationFactors;
		for (auto &Factors : FactorsWithoutPrivate)
		{
			for (auto &Factor : Factors)
				Factor.m_PrivateData.f_Clear();
		}

		co_return FactorsWithoutPrivate;
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CUserAuthenticationFactor const &_Factor)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		if (auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (pFactors->f_FindEqual(_FactorID))
				co_return DMibErrorInstance("User '{}' already has authentication factor ID '{}' registered"_f << _UserID << _FactorID);
		}
		m_UserAuthenticationFactors[_UserID][_FactorID] = _Factor;
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CUserAuthenticationFactor const &_Factor)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		if (auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (!pFactors->f_FindEqual(_FactorID))
				co_return DMibErrorInstance("User '{}' does not have authentication factor ID '{}' registered"_f << _UserID << _FactorID);
		}
		m_UserAuthenticationFactors[_UserID][_FactorID] = _Factor;
		co_return {};
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID);
		if (!pFactors)
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto &Factors = *pFactors;
		auto *pFactor = Factors.f_FindEqual(_FactorID);
		if (!pFactor)
			co_return DMibErrorInstance("User '{}' does not have an authentication factor of type '{}' registered"_f << _UserID << _FactorID);

		Factors.f_Remove(_FactorID);
		if (Factors.f_IsEmpty())
			m_UserAuthenticationFactors.f_Remove(_UserID);
		co_return {};
	}

	TCActor<CDistributedActorTrustManager> CTrustManagerTestHelper::f_TrustManager(NStr::CStr const &_FriendlyName, CStr const &_SessionID, bool _bSupportAuthentication) const
	{
		CDistributedActorTrustManager::COptions Options{.m_ReconnectDelay = 1_ms};
		
		Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
			{
				return fg_ConstructActor<CActorDistributionManager>(_Settings);
			}
		;
		Options.m_KeySetting = CDistributedActorTestKeySettings{};
		Options.m_ListenFlags = NNetwork::ENetFlag_None;
		Options.m_FriendlyName = _FriendlyName;
		Options.m_Enclave = _SessionID;
		Options.m_TranslateHostnames = {};
		Options.m_DefaultConnectionConcurrency = 1;
		Options.m_bSupportAuthentication = _bSupportAuthentication;

		return fg_ConstructActor<CDistributedActorTrustManager>(m_Database, fg_Move(Options));
	}
	
	CTrustManagerTestHelper::CTrustManagerTestHelper()
	{
		m_Database = fg_ConstructActor<CTrustManagerDatabaseTestHelper>();
	}
	
	CTrustManagerTestHelper::~CTrustManagerTestHelper()
	{
		if (m_Database)
			m_Database->f_BlockDestroy();
	}
	
	CTrustedSubscriptionTestHelper::CTrustedSubscriptionTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager, fp64 _Timeout)
		: mp_Internal(fg_ConstructActor<CInternal>(_TrustManager))
		, mp_Timeout(_Timeout)
	{
	}

	CTrustedSubscriptionTestHelper::~CTrustedSubscriptionTestHelper()
	{
		mp_Internal->f_BlockDestroy();
	}

	CTrustedSubscriptionTestHelper::CInternal::CInternal(TCActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_TrustManager(_TrustManager)
	{
	}
}

