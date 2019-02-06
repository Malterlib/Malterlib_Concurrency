// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Encoding/JSONShortcuts>
#include "Malterlib_Concurrency_DistributedActorTrustManager_TestHelpers.h"
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor_TestHelpers.h"

namespace NMib::NConcurrency
{
	using namespace NDistributedActorTrustManagerDatabase;
	using namespace NStr;
	
	TCFuture<CBasicConfig> CTrustManagerDatabaseTestHelper::f_GetBasicConfig()
	{
		return fg_Explicit(m_BasicConfig);
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetBasicConfig(CBasicConfig const &_BasicConfig)
	{
		m_BasicConfig = _BasicConfig;
		return fg_Explicit();
	}
	
	TCFuture<int32> CTrustManagerDatabaseTestHelper::f_GetNewCertificateSerial()
	{
		return fg_Explicit(++m_CertificateSerial);
	}
	
	TCFuture<CDefaultUser> CTrustManagerDatabaseTestHelper::f_GetDefaultUser()
	{
		return fg_Explicit(m_DefaultUser);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetDefaultUser(CDefaultUser const &_DefaultUser)
	{
		m_DefaultUser = _DefaultUser;
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}
	
	TCFuture<CServerCertificate> CTrustManagerDatabaseTestHelper::f_GetServerCertificate(CStr const &_HostName)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			return DMibErrorInstance("No server certificate for host");
		return fg_Explicit(*pServerCertificate);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		if (!m_ServerCertificates(_HostName, _Certificate).f_WasCreated())
			return DMibErrorInstance("Server certificate already exists");
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			return DMibErrorInstance("No server certificate for host");
		*pServerCertificate = _Certificate;
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveServerCertificate(CStr const &_HostName)
	{
		if (!m_ServerCertificates.f_Remove(_HostName))
			return DMibErrorInstance("No server certificate for host");
		return fg_Explicit();
	}
	
	TCFuture<NContainer::TCSet<CListenConfig>> CTrustManagerDatabaseTestHelper::f_EnumListenConfigs()
	{
		NContainer::TCSet<CListenConfig> Return;
		for (auto iListenConfig = m_ListenConfigs.f_GetIterator(); iListenConfig; ++iListenConfig)
			Return[iListenConfig.f_GetKey()];
		return fg_Explicit(Return);
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs(_Config).f_WasCreated())
			return DMibErrorInstance("Listen config already exists");
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs.f_Remove(_Config))
			return DMibErrorInstance("Listen config does not exist");
		
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddClient(CStr const &_HostID, CClient const &_Client)
	{
		if (!m_Clients(_HostID, _Client).f_WasCreated())
			return DMibErrorInstance("Client already exists");
		return fg_Explicit();
	}
	
	TCFuture<CClient> CTrustManagerDatabaseTestHelper::f_GetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return DMibErrorInstance("No client for host ID");
		return fg_Explicit(*pClient);
	}
	
	TCFuture<NStorage::TCUniquePointer<CClient>> CTrustManagerDatabaseTestHelper::f_TryGetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return fg_Explicit(nullptr);
		return fg_Explicit(fg_Construct(*pClient));
	}

	TCFuture<bool> CTrustManagerDatabaseTestHelper::f_HasClient(CStr const &_HostID)
	{
		return fg_Explicit(m_Clients.f_FindEqual(_HostID) != nullptr);
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetClient(CStr const &_HostID, CClient const &_Client)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return DMibErrorInstance("No client for host ID");
		*pClient = _Client;
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveClient(CStr const &_HostID)
	{
		if (!m_Clients.f_Remove(_HostID))
			return DMibErrorInstance("No client for host ID");
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}
	
	TCFuture<CClientConnection> CTrustManagerDatabaseTestHelper::f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			return DMibErrorInstance("No client connection for address");
		return fg_Explicit(*pClientConnection);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		if (!m_ClientConnections(_Address, _ClientConnection).f_WasCreated())
			return DMibErrorInstance("Client connection already exists");
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			return DMibErrorInstance("No client connection for address");
		*pClientConnection = _ClientConnection;
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!m_ClientConnections.f_Remove(_Address))
			return DMibErrorInstance("No client connection for address");
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}
	
	TCFuture<CNamespace> CTrustManagerDatabaseTestHelper::f_GetNamespace(CStr const &_NamespaceName)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			return DMibErrorInstance("No namespace for name");
		return fg_Explicit(*pNamespace);
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		if (!m_Namespaces(_NamespaceName, _Namespace).f_WasCreated())
			return DMibErrorInstance("Namespace already exists");
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			return DMibErrorInstance("No namespace for name");
		*pNamespace = _Namespace;
		return fg_Explicit();
	}
	
	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveNamespace(CStr const &_NamespaceName)
	{
		if (!m_Namespaces.f_Remove(_NamespaceName))
			return DMibErrorInstance("No namespace for name");
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}

	TCFuture<CPermissions> CTrustManagerDatabaseTestHelper::f_GetPermissions(CPermissionIdentifiers const &_Identity)
	{
		auto pPermissions = m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			return DMibErrorInstance("No host permissions for host ID");
		return fg_Explicit(*pPermissions);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		if (!m_Permissions(_Identity, _Permissions).f_WasCreated())
			return DMibErrorInstance("Host permissions already exists for host ID");
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		auto pPermissions = m_Permissions.f_FindEqual(_Identity);
		if (!pPermissions)
			return DMibErrorInstance("No host permissions for host ID");
		*pPermissions = _Permissions;
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemovePermissions(CPermissionIdentifiers const &_Identity)
	{
		if (!m_Permissions.f_Remove(_Identity))
			return DMibErrorInstance("No host permissions for host ID");
		return fg_Explicit();
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
		return fg_Explicit(Return);
	}

	TCFuture<CUserInfo> CTrustManagerDatabaseTestHelper::f_GetUserInfo(CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto pUsers = m_Users.f_FindEqual(_UserID);
		if (!pUsers)
			return DMibErrorInstance("No user with ID");
		return fg_Explicit(*pUsers);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddUser(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		if (!m_Users(_UserID, _UserInfo).f_WasCreated())
			return DMibErrorInstance("There is already a user with that user ID");
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetUserInfo(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto pUsers = m_Users.f_FindEqual(_UserID);
		if (!pUsers)
			return DMibErrorInstance("User '{}' does not exists"_f << _UserID);
		*pUsers = _UserInfo;
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveUser(CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		if (!m_Users.f_Remove(_UserID))
			return DMibErrorInstance("There is no user with that user ID");
		return fg_Explicit();
	}

	TCFuture<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> CTrustManagerDatabaseTestHelper::f_EnumAuthenticationFactor
		(
			bool _bIncludeFullInfo
		)
	{
		if (_bIncludeFullInfo)
			return fg_Explicit(m_UserAuthenticationFactors);

		auto FactorsWithoutPrivate = m_UserAuthenticationFactors;
		for (auto &Factors : FactorsWithoutPrivate)
		{
			for (auto &Factor : Factors)
				Factor.m_PrivateData.f_Clear();
		}

		return fg_Explicit(FactorsWithoutPrivate);
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_AddUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CUserAuthenticationFactor const &_Factor)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		if (auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (pFactors->f_FindEqual(_FactorID))
				return DMibErrorInstance("User '{}' already has authentication factor ID '{}' registered"_f << _UserID << _FactorID);
		}
		m_UserAuthenticationFactors[_UserID][_FactorID] = _Factor;
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_SetUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CUserAuthenticationFactor const &_Factor)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		if (auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (!pFactors->f_FindEqual(_FactorID))
				return DMibErrorInstance("User '{}' does not have authentication factor ID '{}' registered"_f << _UserID << _FactorID);
		}
		m_UserAuthenticationFactors[_UserID][_FactorID] = _Factor;
		return fg_Explicit();
	}

	TCFuture<void> CTrustManagerDatabaseTestHelper::f_RemoveUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto *pFactors = m_UserAuthenticationFactors.f_FindEqual(_UserID);
		if (!pFactors)
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto &Factors = *pFactors;
		auto *pFactor = Factors.f_FindEqual(_FactorID);
		if (!pFactor)
			return DMibErrorInstance("User '{}' does not have an authentication factor of type '{}' registered"_f << _UserID << _FactorID);

		Factors.f_Remove(_FactorID);
		if (Factors.f_IsEmpty())
			m_UserAuthenticationFactors.f_Remove(_UserID);
		return fg_Explicit();
	}

	TCActor<CDistributedActorTrustManager> CTrustManagerTestHelper::f_TrustManager(NStr::CStr const &_FriendlyName, CStr const &_SessionID, bool _bSupportAuthentication) const
	{
		CDistributedActorTrustManager::COptions Options;
		
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
		m_Database->f_BlockDestroy();
	}
	
	CTrustedSubscriptionTestHelper::CTrustedSubscriptionTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_Internal(fg_ConstructActor<CInternal>(_TrustManager))
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

