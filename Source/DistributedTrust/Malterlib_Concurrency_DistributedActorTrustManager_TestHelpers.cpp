// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_TestHelpers.h"
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor_TestHelpers.h"

namespace NMib::NConcurrency
{
	using namespace NDistributedActorTrustManagerDatabase;
	using namespace NStr;
	
	TCContinuation<CBasicConfig> CTrustManagerDatabaseTestHelper::f_GetBasicConfig()
	{
		return fg_Explicit(m_BasicConfig);
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetBasicConfig(CBasicConfig const &_BasicConfig)
	{
		m_BasicConfig = _BasicConfig;
		return fg_Explicit();
	}
	
	TCContinuation<int32> CTrustManagerDatabaseTestHelper::f_GetNewCertificateSerial()
	{
		return fg_Explicit(++m_CertificateSerial);
	}
	
	TCContinuation<NContainer::TCMap<CStr, CServerCertificate>> CTrustManagerDatabaseTestHelper::f_EnumServerCertificates(bool _bIncludeFullInfo)
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
	
	TCContinuation<CServerCertificate> CTrustManagerDatabaseTestHelper::f_GetServerCertificate(CStr const &_HostName)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			return DMibErrorInstance("No server certificate for host");
		return fg_Explicit(*pServerCertificate);
	}

	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		if (!m_ServerCertificates(_HostName, _Certificate).f_WasCreated())
			return DMibErrorInstance("Server certificate already exists");
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
		if (!pServerCertificate)
			return DMibErrorInstance("No server certificate for host");
		*pServerCertificate = _Certificate;
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveServerCertificate(CStr const &_HostName)
	{
		if (!m_ServerCertificates.f_Remove(_HostName))
			return DMibErrorInstance("No server certificate for host");
		return fg_Explicit();
	}
	
	TCContinuation<NContainer::TCSet<CListenConfig>> CTrustManagerDatabaseTestHelper::f_EnumListenConfigs()
	{
		NContainer::TCSet<CListenConfig> Return;
		for (auto iListenConfig = m_ListenConfigs.f_GetIterator(); iListenConfig; ++iListenConfig)
			Return[iListenConfig.f_GetKey()];
		return fg_Explicit(Return);
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs(_Config).f_WasCreated())
			return DMibErrorInstance("Listen config already exists");
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveListenConfig(CListenConfig const &_Config)
	{
		if (!m_ListenConfigs.f_Remove(_Config))
			return DMibErrorInstance("Listen config does not exist");
		
		return fg_Explicit();
	}
	

	TCContinuation<NContainer::TCMap<CStr, CClient>> CTrustManagerDatabaseTestHelper::f_EnumClients(bool _bIncludeFullInfo)
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
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddClient(CStr const &_HostID, CClient const &_Client)
	{
		if (!m_Clients(_HostID, _Client).f_WasCreated())
			return DMibErrorInstance("Client already exists");
		return fg_Explicit();
	}
	
	TCContinuation<CClient> CTrustManagerDatabaseTestHelper::f_GetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return DMibErrorInstance("No client for host ID");
		return fg_Explicit(*pClient);
	}
	
	TCContinuation<NPtr::TCUniquePointer<CClient>> CTrustManagerDatabaseTestHelper::f_TryGetClient(CStr const &_HostID)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return fg_Explicit(nullptr);
		return fg_Explicit(fg_Construct(*pClient));
	}

	TCContinuation<bool> CTrustManagerDatabaseTestHelper::f_HasClient(CStr const &_HostID)
	{
		return fg_Explicit(m_Clients.f_FindEqual(_HostID) != nullptr);
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetClient(CStr const &_HostID, CClient const &_Client)
	{
		auto pClient = m_Clients.f_FindEqual(_HostID);
		if (!pClient)
			return DMibErrorInstance("No client for host ID");
		*pClient = _Client;
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveClient(CStr const &_HostID)
	{
		if (!m_Clients.f_Remove(_HostID))
			return DMibErrorInstance("No client for host ID");
		return fg_Explicit();
	}
	
	TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> CTrustManagerDatabaseTestHelper::f_EnumClientConnections(bool _bIncludeFullInfo)
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
	
	TCContinuation<CClientConnection> CTrustManagerDatabaseTestHelper::f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			return DMibErrorInstance("No client connection for address");
		return fg_Explicit(*pClientConnection);
	}

	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		if (!m_ClientConnections(_Address, _ClientConnection).f_WasCreated())
			return DMibErrorInstance("Client connection already exists");
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection)
	{
		auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			return DMibErrorInstance("No client connection for address");
		*pClientConnection = _ClientConnection;
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		if (!m_ClientConnections.f_Remove(_Address))
			return DMibErrorInstance("No client connection for address");
		return fg_Explicit();
	}
	
	TCContinuation<NContainer::TCMap<CStr, CNamespace>> CTrustManagerDatabaseTestHelper::f_EnumNamespaces(bool _bIncludeFullInfo)
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
	
	TCContinuation<CNamespace> CTrustManagerDatabaseTestHelper::f_GetNamespace(CStr const &_NamespaceName)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			return DMibErrorInstance("No namespace for name");
		return fg_Explicit(*pNamespace);
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		if (!m_Namespaces(_NamespaceName, _Namespace).f_WasCreated())
			return DMibErrorInstance("Namespace already exists");
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
		if (!pNamespace)
			return DMibErrorInstance("No namespace for name");
		*pNamespace = _Namespace;
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveNamespace(CStr const &_NamespaceName)
	{
		if (!m_Namespaces.f_Remove(_NamespaceName))
			return DMibErrorInstance("No namespace for name");
		return fg_Explicit();
	}
	
	TCContinuation<NContainer::TCMap<CStr, CHostPermissions>> CTrustManagerDatabaseTestHelper::f_EnumHostPermissions(bool _bIncludeFullInfo)
	{
		NContainer::TCMap<CStr, CHostPermissions> Return;
		for (auto iHostPermissions = m_HostPermissions.f_GetIterator(); iHostPermissions; ++iHostPermissions)
		{
			auto &HostPermissions = Return[iHostPermissions.f_GetKey()];
			if (_bIncludeFullInfo)
				HostPermissions = *iHostPermissions; 
		}
		return fg_Explicit(Return);
	}
	
	TCContinuation<CHostPermissions> CTrustManagerDatabaseTestHelper::f_GetHostPermissions(CStr const &_HostID)
	{
		auto pHostPermissions = m_HostPermissions.f_FindEqual(_HostID);
		if (!pHostPermissions)
			return DMibErrorInstance("No host permissions for host ID");
		return fg_Explicit(*pHostPermissions);
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_AddHostPermissions(CStr const &_HostID, CHostPermissions const &_HostPermissions)
	{
		if (!m_HostPermissions(_HostID, _HostPermissions).f_WasCreated())
			return DMibErrorInstance("Host permissions already exists for host ID");
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_SetHostPermissions(CStr const &_HostID, CHostPermissions const &_HostPermissions)
	{
		auto pHostPermissions = m_HostPermissions.f_FindEqual(_HostID);
		if (!pHostPermissions)
			return DMibErrorInstance("No host permissions for host ID");
		*pHostPermissions = _HostPermissions;
		return fg_Explicit();
	}
	
	TCContinuation<void> CTrustManagerDatabaseTestHelper::f_RemoveHostPermissions(CStr const &_HostID)
	{
		if (!m_HostPermissions.f_Remove(_HostID))
			return DMibErrorInstance("No host permissions for host ID");
		return fg_Explicit();
	}
	
	TCActor<CDistributedActorTrustManager> CTrustManagerTestHelper::f_TrustManager(NStr::CStr const &_FriendlyName, CStr const &_SessionID) const
	{
		return fg_ConstructActor<CDistributedActorTrustManager>
			(
				m_Database
				, [](CActorDistributionManagerInitSettings const &_Settings)
				{
					return fg_ConstructActor<CActorDistributionManager>(_Settings);
				}
				, CDistributedActorTestKeySettings{}
				, NNet::ENetFlag_None
				, _FriendlyName
				, _SessionID
				, NContainer::TCMap<NStr::CStr, NStr::CStr>{}
				, 1
			)
		;
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
