// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/TestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Network/SSL>

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NStr;

namespace NMib::NConcurrency::NTestHelpers
{
	struct CTrustManagerDatabase : public ICDistributedActorTrustManagerDatabase
	{
		enum
		{
			mc_bAllowInternalAccess = true
		};
		
		TCContinuation<CBasicConfig> f_GetBasicConfig() override
		{
			return fg_Explicit(m_BasicConfig);
		}
		
		TCContinuation<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) override
		{
			m_BasicConfig = _BasicConfig;
			return fg_Explicit();
		}
		
		TCContinuation<int32> f_GetNewCertificateSerial() override
		{
			return fg_Explicit(++m_CertificateSerial);
		}
		
		
		TCContinuation<NContainer::TCMap<CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override
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
		
		TCContinuation<CServerCertificate> f_GetServerCertificate(CStr const &_HostName) override
		{
			auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
			if (!pServerCertificate)
				return DMibErrorInstance("No server certificate for host");
			return fg_Explicit(*pServerCertificate);
		}

		TCContinuation<void> f_AddServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate) override
		{
			if (!m_ServerCertificates(_HostName, _Certificate).f_WasCreated())
				return DMibErrorInstance("Server certificate already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate) override
		{
			auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
			if (!pServerCertificate)
				return DMibErrorInstance("No server certificate for host");
			*pServerCertificate = _Certificate;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveServerCertificate(CStr const &_HostName) override
		{
			if (!m_ServerCertificates.f_Remove(_HostName))
				return DMibErrorInstance("No server certificate for host");
			return fg_Explicit();
		}
		
		
		TCContinuation<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() override
		{
			NContainer::TCSet<CListenConfig> Return;
			for (auto iListenConfig = m_ListenConfigs.f_GetIterator(); iListenConfig; ++iListenConfig)
				Return[iListenConfig.f_GetKey()];
			return fg_Explicit(Return);
		}
		
		TCContinuation<void> f_AddListenConfig(CListenConfig const &_Config) override
		{
			if (!m_ListenConfigs(_Config).f_WasCreated())
				return DMibErrorInstance("Listen config already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveListenConfig(CListenConfig const &_Config) override
		{
			if (!m_ListenConfigs.f_Remove(_Config))
				return DMibErrorInstance("Listen config does not exist");
			
			return fg_Explicit();
		}
		

		TCContinuation<NContainer::TCMap<CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override
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
		
		TCContinuation<void> f_AddClient(CStr const &_HostID, CClient const &_Client) override
		{
			if (!m_Clients(_HostID, _Client).f_WasCreated())
				return DMibErrorInstance("Client already exists");
			return fg_Explicit();
		}
		
		TCContinuation<CClient> f_GetClient(CStr const &_HostID) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return DMibErrorInstance("No client for host ID");
			return fg_Explicit(*pClient);
		}
		
		TCContinuation<NPtr::TCUniquePointer<CClient>> f_TryGetClient(CStr const &_HostID) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return fg_Explicit(nullptr);
			return fg_Explicit(fg_Construct(*pClient));
		}

		TCContinuation<bool> f_HasClient(CStr const &_HostID) override
		{
			return fg_Explicit(m_Clients.f_FindEqual(_HostID) != nullptr);
		}
		
		TCContinuation<void> f_SetClient(CStr const &_HostID, CClient const &_Client) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return DMibErrorInstance("No client for host ID");
			*pClient = _Client;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveClient(CStr const &_HostID) override
		{
			if (!m_Clients.f_Remove(_HostID))
				return DMibErrorInstance("No client for host ID");
			return fg_Explicit();
		}
		
		
		TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> f_EnumClientConnections(bool _bIncludeFullInfo) override
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
		
		TCContinuation<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address) override
		{
			auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
			if (!pClientConnection)
				return DMibErrorInstance("No client connection for address");
			return fg_Explicit(*pClientConnection);
		}

		TCContinuation<void> f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override
		{
			if (!m_ClientConnections(_Address, _ClientConnection).f_WasCreated())
				return DMibErrorInstance("Client connection already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override
		{
			auto pClientConnection = m_ClientConnections.f_FindEqual(_Address);
			if (!pClientConnection)
				return DMibErrorInstance("No client connection for address");
			*pClientConnection = _ClientConnection;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) override
		{
			if (!m_ClientConnections.f_Remove(_Address))
				return DMibErrorInstance("No client connection for address");
			return fg_Explicit();
		}
		
		TCContinuation<NContainer::TCMap<CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) override
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
		
		TCContinuation<CNamespace> f_GetNamespace(CStr const &_NamespaceName) override
		{
			auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
			if (!pNamespace)
				return DMibErrorInstance("No namespace for name");
			return fg_Explicit(*pNamespace);
		}
		
		TCContinuation<void> f_AddNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace) override
		{
			if (!m_Namespaces(_NamespaceName, _Namespace).f_WasCreated())
				return DMibErrorInstance("Namespace already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace) override
		{
			auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
			if (!pNamespace)
				return DMibErrorInstance("No namespace for name");
			*pNamespace = _Namespace;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveNamespace(CStr const &_NamespaceName) override
		{
			if (!m_Namespaces.f_Remove(_NamespaceName))
				return DMibErrorInstance("No namespace for name");
			return fg_Explicit();
		}
		
		TCContinuation<NContainer::TCMap<CStr, CHostPermissions>> f_EnumHostPermissions(bool _bIncludeFullInfo) override
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
		
 		TCContinuation<CHostPermissions> f_GetHostPermissions(CStr const &_HostID) override
		{
			auto pHostPermissions = m_HostPermissions.f_FindEqual(_HostID);
			if (!pHostPermissions)
				return DMibErrorInstance("No host permissions for host ID");
			return fg_Explicit(*pHostPermissions);
		}
		
		TCContinuation<void> f_AddHostPermissions(CStr const &_HostID, CHostPermissions const &_HostPermissions) override
		{
			if (!m_HostPermissions(_HostID, _HostPermissions).f_WasCreated())
				return DMibErrorInstance("Host permissions already exists for host ID");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetHostPermissions(CStr const &_HostID, CHostPermissions const &_HostPermissions) override
		{
			auto pHostPermissions = m_HostPermissions.f_FindEqual(_HostID);
			if (!pHostPermissions)
				return DMibErrorInstance("No host permissions for host ID");
			*pHostPermissions = _HostPermissions;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveHostPermissions(CStr const &_HostID) override
		{
			if (!m_HostPermissions.f_Remove(_HostID))
				return DMibErrorInstance("No host permissions for host ID");
			return fg_Explicit();
		}
		
		CBasicConfig m_BasicConfig;
		int32 m_CertificateSerial = 0;
		
		TCMap<CStr, CServerCertificate> m_ServerCertificates;
		NContainer::TCSet<CListenConfig> m_ListenConfigs;
		TCMap<CStr, CClient> m_Clients;
		TCMap<CDistributedActorTrustManager_Address, CClientConnection> m_ClientConnections;
		TCMap<CStr, CNamespace> m_Namespaces;
		TCMap<CStr, CHostPermissions> m_HostPermissions;
	};
}
	
