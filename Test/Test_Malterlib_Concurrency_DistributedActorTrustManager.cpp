// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/TestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;


namespace
{
	class CTrustManagerDatabase : public ICDistributedActorTrustManagerDatabase
	{
		CBasicConfig m_BasicConfig;
		int32 m_CertificateSerial = 0;
		
		TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;
		NContainer::TCSet<CListenConfig> m_ListenConfigs;
		TCMap<NStr::CStr, CClient> m_Clients;
		TCMap<CDistributedActorTrustManager_Address, CClientConnection> m_ClientConnections;
		
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
		
		
		TCContinuation<NContainer::TCSet<NStr::CStr>> f_EnumServerCertificates() override
		{
			NContainer::TCSet<NStr::CStr> Return;
			for (auto iCertificate = m_ServerCertificates.f_GetIterator(); iCertificate; ++iCertificate)
				Return[iCertificate.f_GetKey()];
			return fg_Explicit(Return);
		}
		
		TCContinuation<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) override
		{
			auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
			if (!pServerCertificate)
				return DMibErrorInstance("No server certificate for host");
			return fg_Explicit(*pServerCertificate);
		}

		TCContinuation<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override
		{
			if (!m_ServerCertificates(_HostName, _Certificate).f_WasCreated())
				return DMibErrorInstance("Server certificate already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override
		{
			auto pServerCertificate = m_ServerCertificates.f_FindEqual(_HostName);
			if (!pServerCertificate)
				return DMibErrorInstance("No server certificate for host");
			*pServerCertificate = _Certificate;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) override
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
		

		TCContinuation<NContainer::TCSet<NStr::CStr>> f_EnumClients() override
		{
			NContainer::TCSet<NStr::CStr> Return;
			for (auto iClient = m_Clients.f_GetIterator(); iClient; ++iClient)
				Return[iClient.f_GetKey()];
			return fg_Explicit(Return);
		}
		
		TCContinuation<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) override
		{
			if (!m_Clients(_HostID, _Client).f_WasCreated())
				return DMibErrorInstance("Client already exists");
			return fg_Explicit();
		}
		
		TCContinuation<CClient> f_GetClient(NStr::CStr const &_HostID) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return DMibErrorInstance("No client for host ID");
			return fg_Explicit(*pClient);
		}
		
		TCContinuation<void> f_SetClient(NStr::CStr const &_HostID, CClient const &_Client) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return DMibErrorInstance("No client for host ID");
			*pClient = _Client;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) override
		{
			if (!m_Clients.f_Remove(_HostID))
				return DMibErrorInstance("No client for host ID");
			return fg_Explicit();
		}
		
		
		TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumClientConnections() override
		{
			NContainer::TCSet<CDistributedActorTrustManager_Address> Return;
			for (auto iClientConnection = m_ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
				Return[iClientConnection.f_GetKey()];
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
	};
	
	class CDistributedActorTrustManager_Tests : public NMib::NTest::CTest
	{
	public:
		struct CTestActor : public CActor
		{
			uint32 f_Test()
			{
				return 5;
			}
		};
		
		void fp_DoBasicTests
			(
				NFunction::TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (NStr::CStr const &_Name)> const &_fDatabaseFactory
				, NFunction::TCFunction<void ()> const &_fCleanup
			)
		{
			DMibTestSuite("Basic")
			{
				_fCleanup();
				
				TCActor<ICDistributedActorTrustManagerDatabase> ServerDatabase = _fDatabaseFactory("Server");
				TCActor<ICDistributedActorTrustManagerDatabase> ClientDatabase = _fDatabaseFactory("Client");
				
				auto fCreateServerTrustManager = [&] 
					{
						return fg_ConstructActor<CDistributedActorTrustManager>
							(
								ServerDatabase
								, [](NStr::CStr const &_HostID)
								{
									return fg_ConstructActor<CActorDistributionManager>(_HostID);
								}
								, 1024 
							)
						;
					}
				;
				
				auto fCreateClientTrustManager = [&] 
					{
						return fg_ConstructActor<CDistributedActorTrustManager>
							(
								ClientDatabase
								, [](NStr::CStr const &_HostID)
								{
									return fg_ConstructActor<CActorDistributionManager>(_HostID);
								}
								, 1024 
							)
						;
					}
				;

				auto fDoTests = [&](TCActor<CDistributedActorTrustManager> const &_ServerTrustManager, TCActor<CDistributedActorTrustManager> const &_ClientTrustManager)
					{
						CDistributedActorTestHelper ServerHelper{_ServerTrustManager};
						CDistributedActorTestHelper ClientHelper{_ClientTrustManager};

						ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test");
						NStr::CStr Subscription = ClientHelper.f_Subscribe("Test");

						TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

						uint32 Result = DMibCallActor(Actor, CTestActor::f_Test).f_CallSync(60.0);
						DMibExpect(Result, ==, 5);
					}
				;
				
				{
					DMibTestPath("Initial");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = fCreateServerTrustManager();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31392/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
					
					fDoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}
				
				{
					DMibTestPath("From database");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = fCreateServerTrustManager();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();

					fDoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Change listen");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = fCreateServerTrustManager();
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31393/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

					CDistributedActorTrustManager_Address OldServerAddress;
					OldServerAddress.m_URL = "wss://localhost:31392/";

					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveListen, OldServerAddress).f_CallSync(60.0);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, OldServerAddress).f_CallSync(60.0);
					
					fDoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Remove client");
					NStr::CStr HostID;
					{
						TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();
						HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
						ClientTrustManager->f_BlockDestroy();
					}
					TCActor<CDistributedActorTrustManager> ServerTrustManager = fCreateServerTrustManager();
					
					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(60.0);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();
					
					auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
					
					DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
					DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());
					
					auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();
					
					DMibExpectFalse(Address.m_bConnected);
					DMibExpect(Address.m_Error, !=, "");
					
					DMibExpectExceptionType(fDoTests(ServerTrustManager, ClientTrustManager), NException::CException);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Remove client while connected");
					ServerDatabase = fg_ConstructActor<CTrustManagerDatabase>();
					ClientDatabase = fg_ConstructActor<CTrustManagerDatabase>();

					TCActor<CDistributedActorTrustManager> ServerTrustManager = fCreateServerTrustManager();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = fCreateClientTrustManager();
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31392/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
					
					fDoTests(ServerTrustManager, ClientTrustManager);
					
					auto HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(60.0);;
					
					auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
					
					DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
					DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());
					
					auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();
					
					DMibExpectFalse(Address.m_bConnected);
					DMibExpect(Address.m_Error, !=, "");
					
					DMibExpectExceptionType(fDoTests(ServerTrustManager, ClientTrustManager), NException::CException);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}
				
				ServerDatabase->f_BlockDestroy();
				ClientDatabase->f_BlockDestroy();
				
				_fCleanup();
			};
		}
		void f_DoTests()
		{
			DMibTestCategory("Dummy Database")
			{
				fp_DoBasicTests
					(
						[](NStr::CStr const &_Name)
						{
							return fg_ConstructActor<CTrustManagerDatabase>();
						}
						, []
						{
						}
					)
				;
			};
			DMibTestCategory("JSON Directory Database")
			{
				NStr::CStr BaseDirectory = NFile::CFile::fs_GetProgramDirectory() + "/TestTrustManager";
				fp_DoBasicTests
					(
						[BaseDirectory](NStr::CStr const &_Name)
						{
							return fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(BaseDirectory + "/" + _Name);
						}
						, [BaseDirectory]
						{
							if (NFile::CFile::fs_FileExists(BaseDirectory))
								NFile::CFile::fs_DeleteDirectoryRecursive(BaseDirectory);
						}
					)
				;
			};
		}
	};

	DMibTestRegister(CDistributedActorTrustManager_Tests, Malterlib::Concurrency);
}
