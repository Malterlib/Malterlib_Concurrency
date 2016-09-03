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

namespace
{
	class CTrustManagerDatabase : public ICDistributedActorTrustManagerDatabase
	{
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
		
		
		TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override
		{
			NContainer::TCMap<NStr::CStr, CServerCertificate> Return;
			for (auto iCertificate = m_ServerCertificates.f_GetIterator(); iCertificate; ++iCertificate)
			{
				auto &Certficate = Return[iCertificate.f_GetKey()];
				if (_bIncludeFullInfo)
					Certficate = *iCertificate; 
			}
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
		

		TCContinuation<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override
		{
			NContainer::TCMap<NStr::CStr, CClient> Return;
			for (auto iClient = m_Clients.f_GetIterator(); iClient; ++iClient)
			{
				auto &Client = Return[iClient.f_GetKey()];
				if (_bIncludeFullInfo)
					Client = *iClient; 
			}
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
		
		TCContinuation<NPtr::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) override
		{
			auto pClient = m_Clients.f_FindEqual(_HostID);
			if (!pClient)
				return fg_Explicit(nullptr);
			return fg_Explicit(fg_Construct(*pClient));
		}

		TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) override
		{
			return fg_Explicit(m_Clients.f_FindEqual(_HostID) != nullptr);
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
		
		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) override
		{
			NContainer::TCMap<NStr::CStr, CNamespace> Return;
			for (auto iNamespace = m_Namespaces.f_GetIterator(); iNamespace; ++iNamespace)
			{
				auto &Namespace = Return[iNamespace.f_GetKey()];
				if (_bIncludeFullInfo)
					Namespace = *iNamespace; 
			}
			return fg_Explicit(Return);
		}
		
		TCContinuation<CNamespace> f_GetNamespace(NStr::CStr const &_NamespaceName) override
		{
			auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
			if (!pNamespace)
				return DMibErrorInstance("No namespace for name");
			return fg_Explicit(*pNamespace);
		}
		
		TCContinuation<void> f_AddNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override
		{
			if (!m_Namespaces(_NamespaceName, _Namespace).f_WasCreated())
				return DMibErrorInstance("Namespace already exists");
			return fg_Explicit();
		}
		
		TCContinuation<void> f_SetNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override
		{
			auto pNamespace = m_Namespaces.f_FindEqual(_NamespaceName);
			if (!pNamespace)
				return DMibErrorInstance("No namespace for name");
			*pNamespace = _Namespace;
			return fg_Explicit();
		}
		
		TCContinuation<void> f_RemoveNamespace(NStr::CStr const &_NamespaceName) override
		{
			if (!m_Namespaces.f_Remove(_NamespaceName))
				return DMibErrorInstance("No namespace for name");
			return fg_Explicit();
		}
		
		CBasicConfig m_BasicConfig;
		int32 m_CertificateSerial = 0;
		
		TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;
		NContainer::TCSet<CListenConfig> m_ListenConfigs;
		TCMap<NStr::CStr, CClient> m_Clients;
		TCMap<CDistributedActorTrustManager_Address, CClientConnection> m_ClientConnections;
		TCMap<NStr::CStr, CNamespace> m_Namespaces;
		
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
		struct CTestActor2 : public CActor
		{
			uint32 f_Test()
			{
				return 5;
			}
		};
		struct CTestActor3 : public CActor
		{
			uint32 f_Test()
			{
				return 5;
			}
		};

		struct CState
		{
			auto f_CreateServerTrustManager() const
			{
				return fg_ConstructActor<CDistributedActorTrustManager>
					(
						m_ServerDatabase
						, [](NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
						{
							return fg_ConstructActor<CActorDistributionManager>(_HostID, _FriendlyName);
						}
						, 1024 
						, NNet::ENetFlag_None
						, "TestServer" 
					)
				;
			}
			
			auto f_CreateClientTrustManager() const 
			{
				return fg_ConstructActor<CDistributedActorTrustManager>
					(
						m_ClientDatabase
						, [](NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
						{
							return fg_ConstructActor<CActorDistributionManager>(_HostID, _FriendlyName);
						}
						, 1024
						, NNet::ENetFlag_None
						, "TestClient"
					)
				;
			}
			
			CState
				(
					NFunction::TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (NStr::CStr const &_Name)> const &_fDatabaseFactory
					, NFunction::TCFunction<void ()> const &_fCleanup
					, NStr::CStr const &_NameDistinguish = {} 
				)
				: m_fCleanup(_fCleanup)
			{
				if (_fCleanup)
					_fCleanup();
				m_ServerDatabase = _fDatabaseFactory("Server" + _NameDistinguish);
				m_ClientDatabase = _fDatabaseFactory("Client" + _NameDistinguish);
			}
			~CState()
			{
				m_ServerDatabase->f_BlockDestroy();
				m_ClientDatabase->f_BlockDestroy();
				if (m_fCleanup)
					m_fCleanup();
			}
			
			
			TCActor<ICDistributedActorTrustManagerDatabase> m_ServerDatabase;
			TCActor<ICDistributedActorTrustManagerDatabase> m_ClientDatabase;
			NFunction::TCFunction<void ()> m_fCleanup;
		};

		void fp_DoTests(TCActor<CDistributedActorTrustManager> const &_ServerTrustManager, TCActor<CDistributedActorTrustManager> const &_ClientTrustManager)
		{
			CDistributedActorTestHelper ServerHelper{_ServerTrustManager};
			CDistributedActorTestHelper ClientHelper{_ClientTrustManager};

			ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test");
			NStr::CStr Subscription = ClientHelper.f_Subscribe("Test");

			TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

			uint32 Result = DMibCallActor(Actor, CTestActor::f_Test).f_CallSync(60.0);
			DMibExpect(Result, ==, 5);
		}
		
		void fp_DoBasicTests
			(
				NFunction::TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (NStr::CStr const &_Name)> const &_fDatabaseFactory
				, NFunction::TCFunction<void ()> const &_fCleanup
			)
		{
			DMibTestSuite("Basic")
			{
				CState State{_fDatabaseFactory, _fCleanup};
				
				{
					DMibTestPath("Initial");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					NStr::CStr ServerHostID = ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					NStr::CStr ClientHostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31394/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(60.0));

					auto AllListens = ServerTrustManager(&CDistributedActorTrustManager::f_EnumListens).f_CallSync(60.0);
					NContainer::TCSet<CDistributedActorTrustManager_Address> ExpectedListens;
					ExpectedListens[ServerAddress];
					DMibExpect(AllListens, ==, ExpectedListens);
					
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
					DMibExpectTrue(ClientTrustManager(&CDistributedActorTrustManager::f_HasClientConnection, TrustTicket.m_ServerAddress).f_CallSync(60.0));
					
					auto AllClientConnections = ClientTrustManager(&CDistributedActorTrustManager::f_EnumClientConnections).f_CallSync(60.0);
					NContainer::TCMap<CDistributedActorTrustManager_Address, CHostInfo> ExpectedClientConnections;
					{
						auto &ExpectedHostInfo = ExpectedClientConnections[TrustTicket.m_ServerAddress];
						ExpectedHostInfo.m_FriendlyName = "TestServer";
						ExpectedHostInfo.m_HostID = ServerHostID;
					}
					DMibExpect(AllClientConnections, ==, ExpectedClientConnections);
					
					fp_DoTests(ServerTrustManager, ClientTrustManager);
					
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasClient, ClientHostID).f_CallSync(60.0));
					
					auto AllClients = ServerTrustManager(&CDistributedActorTrustManager::f_EnumClients).f_CallSync(60.0);
					NContainer::TCMap<NStr::CStr, CHostInfo> ExpectedClients;
					{
						auto &ExpectedHostInfo = ExpectedClients[ClientHostID];
						ExpectedHostInfo.m_FriendlyName = "TestClient";
						ExpectedHostInfo.m_HostID = ClientHostID;
					}
					DMibExpect(AllClients, ==, ExpectedClients);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}
				
				{
					DMibTestPath("From database");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31394/";
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(60.0));
					
					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					fp_DoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Change listen");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31395/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

					CDistributedActorTrustManager_Address OldServerAddress;
					OldServerAddress.m_URL = "wss://localhost:31394/";

					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveListen, OldServerAddress).f_CallSync(60.0);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, OldServerAddress).f_CallSync(60.0);
					
					fp_DoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Remove client");
					NStr::CStr HostID;
					{
						TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
						HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
						ClientTrustManager->f_BlockDestroy();
					}
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
					
					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(60.0);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
					
					auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
					
					DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
					DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());
					
					auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();
					
					DMibExpectFalse(Address.m_bConnected);
					DMibExpect(Address.m_Error, !=, "");
					
					DMibExpectExceptionType(fp_DoTests(ServerTrustManager, ClientTrustManager), NException::CException);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

			};
			
			DMibTestSuite("Remove client while connected")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31396/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
				
				ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
				
				fp_DoTests(ServerTrustManager, ClientTrustManager);
				
				auto HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
				ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(60.0);
				
				auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
				
				DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
				DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());
				
				auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();
				
				DMibExpectFalse(Address.m_bConnected);
				DMibExpect(Address.m_Error, !=, "");
				
				DMibExpectExceptionType(fp_DoTests(ServerTrustManager, ClientTrustManager), NException::CException);
				
				ClientTrustManager->f_BlockDestroy();
				ServerTrustManager->f_BlockDestroy();
			};

			DMibTestSuite("Disconnects")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31397/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test");
				NStr::CStr Subscription = ClientHelper.f_Subscribe("Test");

				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);
				
				TCActor<CSeparateThreadActor> DispatchActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Dispatch"));

				NStr::CStr DispatchError;
				mint nCalls = 0;
				fg_Dispatch
					(
						DispatchActor
						, [&]
						{
							NTime::CClock Clock;
							Clock.f_Start();
							while (Clock.f_GetTime() < 1.5)
							{
								try
								{
									DMibCallActor(Actor, CTestActor::f_Test).f_CallSync(10.0);
									++nCalls;
								}
								catch (NException::CException const &_Exception)
								{
									DispatchError = _Exception.f_GetErrorStr();
									break;
								}
							}
						}
					)
					> fg_DiscardResult();
				;

				NTime::CClock Clock;
				Clock.f_Start();
				while (Clock.f_GetTime() < 1.5)
				{
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, ServerAddress).f_CallSync(60.0);
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
					NSys::fg_Thread_Sleep(0.05);
				}

				DispatchActor->f_BlockDestroy();
				
				DMibExpect(DispatchError, ==, "");
				DMibExpect(nCalls, >, 1);
				
				ServerTrustManager->f_BlockDestroy();
				ClientTrustManager->f_BlockDestroy();

			};
			
			DMibTestSuite("Security")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31398/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);

					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, ServerAddress).f_CallSync(60.0);
					
					// Check that you cannot reuse old ticket
					DMibExpectExceptionType(ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0), NException::CException);
					
					// Check that you can add trust to server, even with old client in database
					TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
				}

				{
					DMibTestPath("Before fraudulent try");
					fp_DoTests(ServerTrustManager, ClientTrustManager);
				}

				TCActor<ICDistributedActorTrustManagerDatabase> Client2Database = _fDatabaseFactory("Client2");

				{
					NDistributedActorTrustManagerDatabase::CBasicConfig BasicConfig;
					BasicConfig.m_HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					
					NNet::CSSLContext::CCertificateOptions Options;
					Options.m_KeyLength = 1024;
					Options.m_Subject = fg_Format("Malterlib Distributed Actors Root - {}", BasicConfig.m_HostID).f_Left(64); 
					auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
					Extension.m_bCritical = false; 
					Extension.m_Value = BasicConfig.m_HostID;
					
					NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey(Options, BasicConfig.m_CACertificate, BasicConfig.m_CAPrivateKey, 1, 100*365);
						
					Client2Database(&ICDistributedActorTrustManagerDatabase::f_SetBasicConfig, BasicConfig).f_CallSync(60.0);
				}
				
				TCActor<CDistributedActorTrustManager> Client2TrustManager = fg_ConstructActor<CDistributedActorTrustManager>
					(
						Client2Database
						, [](NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
						{
							return fg_ConstructActor<CActorDistributionManager>(_HostID, _FriendlyName);
						}
						, 1024 
						, NNet::ENetFlag_None
					)
				;
				
				auto TrustTicket2 = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
				// Test fraudulent client add
				DMibExpectExceptionType(Client2TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket2, 30.0).f_CallSync(60.0), NException::CException);
				
				{
					DMibTestPath("After fraudulent try");
					fp_DoTests(ServerTrustManager, ClientTrustManager);
				}
				Client2TrustManager->f_BlockDestroy();
				Client2Database->f_BlockDestroy();
				ClientTrustManager->f_BlockDestroy();
				ServerTrustManager->f_BlockDestroy();
			};
			DMibTestSuite("Subscriptions")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
				
				CStr ServerHostID = ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31405/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};
				
				TCSet<CStr> ServerHosts;
				ServerHosts[ServerHostID];
				
				TCSet<CStr> Published;
				
				auto TestActor = fg_ConcurrentActor();
				{
					DMibTestPath("General");
					auto TrustedSubscription = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("Before publish");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
					}
					Published[ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test")];
					NStr::CStr Subscription = ClientHelper.f_Subscribe("Test");

					// Make sure that queue has been processed on test actor
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After publish");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
					}
					
					NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnum;
					{
						auto &Host = ExpectedAllowedEnum["Test"].m_AllowedHosts[ServerHostID];
						Host.m_HostID = ServerHostID;
						Host.m_FriendlyName = "TestServer";
					}
					NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnumEmpty;
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After allow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					{
						DMibTestPath("After allow again");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", NonExistantHosts).f_CallSync(60.0);
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", NonExistantHosts).f_CallSync(60.0);
					}
					
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After disallow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnum;
						{
							auto &Host = ExpectedAllowedEnum["Test"].m_DisallowedHosts[ServerHostID];
							Host.m_HostID = ServerHostID;
							Host.m_FriendlyName = "TestServer";
						}
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}
					
					TrustedSubscription.f_Clear();
					{
						DMibTestPath("After clear subscription");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnumEmpty);
					}
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					{
						DMibTestPath("After allow without subscription");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}
					
					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", NonExistantHosts).f_CallSync(60.0);
					}
					{
						DMibTestPath("After nonexistant disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}
					
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0); // Test disallow without namespace
					{
						DMibTestPath("After double disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnumEmpty);
					}
				}
				{
					DMibTestPath("Double");
					Published[ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test")];
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					
					NStr::CStr Subscription0 = ClientHelper.f_Subscribe("Test");
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor).f_CallSync(60.0);
					auto TrustedSubscription1 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
						DMibExpect(TrustedSubscription1.m_Actors.f_GetLen(), ==, 2);
					}
				}
				auto fWaitForSubscribed = [&](auto &_Subscription, mint _nSubScribed)
					{
						NTime::CClock Clock{true};
						mint nSubscribed = 0;
						while (Clock.f_GetTime() < 10.0)
						{
							nSubscribed = fg_Dispatch
								(
									TestActor
									, [&]
									{ 
										return _Subscription.m_Actors.f_GetLen();
									}
								).f_CallSync(60.0)
							;
							if (nSubscribed == _nSubScribed)
								break;
						}
						return nSubscribed;
					}
				;
				{
					DMibTestPath("Concurrent subscribe");
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					
					NStr::CStr Subscription0 = ClientHelper.f_Subscribe("Test");
					auto Subscriptions =
						(
							ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor)
							+ ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor)
						).f_CallSync()
					;
					auto &TrustedSubscription0 = fg_Get<0>(Subscriptions);
					auto &TrustedSubscription1 = fg_Get<1>(Subscriptions);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
						DMibExpect(TrustedSubscription1.m_Actors.f_GetLen(), ==, 2);
					}
				}
				{
					DMibTestPath("Publish while subscribed");
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					
					NStr::CStr Subscription0 = ClientHelper.f_Subscribe("Test");
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
					}
					NStr::CStr NewPublish = ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test");
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 3), ==, 3);
					
					{
						DMibTestPath("2 types");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "Test", TestActor).f_CallSync(60.0);
						NStr::CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor2>(), "Test");
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						ServerHelper.f_Unpublish(NewPublish2);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
					}
					{
						DMibTestPath("2 types no sub unpublish");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "Test", TestActor).f_CallSync(60.0);
						NStr::CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor2>(), "Test");
						NStr::CStr Subscription = ClientHelper.f_Subscribe("Test");
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CActor>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						TrustedSubscription2.f_Clear();
						ServerHelper.f_Unpublish(NewPublish2);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CActor>(Subscription))
								break;
						}
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ClientHelper.f_Unsubscribe(Subscription);
					}
					{
						DMibTestPath("2 types not allowed");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "Test2", TestActor).f_CallSync(60.0);
						auto TrustedSubscription3 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test2", TestActor).f_CallSync(60.0);
						NStr::CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor2>(), "Test2");
						NStr::CStr NewPublish3 = ServerHelper.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor>(), "Test2");
						NStr::CStr Subscription = ClientHelper.f_Subscribe("Test2");
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CActor>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ServerHelper.f_Unpublish(NewPublish2);
						ServerHelper.f_Unpublish(NewPublish3);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CActor>(Subscription))
								break;
						}
						DMibExpectFalse(ClientHelper.f_GetRemoteActor<CActor>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ClientHelper.f_Unsubscribe(Subscription);
					}
					
					ServerHelper.f_Unpublish(NewPublish);
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 2), ==, 2);
					auto ToUnpublish = *Published.f_FindSmallest();
					ServerHelper.f_Unpublish(ToUnpublish);
					Published.f_Remove(ToUnpublish);
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 1), ==, 1);
					ToUnpublish = *Published.f_FindSmallest();
					ServerHelper.f_Unpublish(ToUnpublish);
					Published.f_Remove(ToUnpublish);
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 0), ==, 0);
				}
				{
					DMibTestPath("2 hosts no sub");
					CState State2{_fDatabaseFactory, nullptr, "2"};
					TCActor<CDistributedActorTrustManager> ServerTrustManager2 = State2.f_CreateServerTrustManager();
					CDistributedActorTestHelper ServerHelper2{ServerTrustManager};
					{
						CDistributedActorTrustManager_Address ServerAddress;
						ServerAddress.m_URL = "wss://localhost:31406/";
						ServerTrustManager2(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

						auto TrustTicket = ServerTrustManager2(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress).f_CallSync(60.0);
						ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket, 30.0).f_CallSync(60.0);
					}
					CStr ServerHostID2 = ServerTrustManager2(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					TCSet<CStr> ServerHosts2;
					ServerHosts2[ServerHostID];
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test3", ServerHosts).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test3", ServerHosts2).f_CallSync(60.0);
					auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test3", TestActor).f_CallSync(60.0);
					NStr::CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor2>(), "Test3");
					NStr::CStr NewPublish3 = ServerHelper2.f_Publish<CTestActor2>(fg_ConstructDistributedActor<CTestActor2>(), "Test3");
					NStr::CStr Subscription = ClientHelper.f_Subscribe("Test3");
					DMibExpectTrue(ClientHelper.f_GetRemoteActor<CActor>(Subscription));
					ServerHelper.f_Unpublish(NewPublish2);
					ServerHelper2.f_Unpublish(NewPublish3);
					NTime::CClock Clock{true};
					while (Clock.f_GetTime() < 10.0)
					{
						if (!ClientHelper.f_GetRemoteActor<CActor>(Subscription))
							break;
					}
					DMibExpectFalse(ClientHelper.f_GetRemoteActor<CActor>(Subscription));
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test3", ServerHosts).f_CallSync(60.0);
					ServerTrustManager2->f_BlockDestroy();
				}
				{
					DMibTestPath("Subscribe stress");
					TCActorResultVector<void> Dispatches;
					for (mint i = 0; i < 100000; ++i)
					{
						fg_ConcurrentDispatch
							(
								[&]
								{
									ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor) > fg_DiscardResult();
								}
							)
							> Dispatches.f_AddResult(); 
						;
					}
					Dispatches.f_GetResults().f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
				}
				{
					DMibTestPath("Notifications");
					NAtomic::TCAtomic<mint> nActors{0};
					CStr Published = ServerHelper.f_Publish<CTestActor>(fg_ConstructDistributedActor<CTestActor>(), "Test");
					NStr::CStr Subscription0 = ClientHelper.f_Subscribe("Test");
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 0);
					}
					
					TrustedSubscription0.f_OnNewActor
						(
							[&](TCDistributedActor<CTestActor> const &_Actor)
							{
								++nActors;
							}
						)
					;
					
					TrustedSubscription0.f_OnRemoveActor
						(
							[&](TCWeakDistributedActor<CActor> const &_Actor)
							{
								--nActors;
							}
						)
					;

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					
					{
						DMibTestPath("After allow");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 1);
						DMibExpect(nActors.f_Load(), ==, 1);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "Test", ServerHosts).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					
					{
						DMibTestPath("After disallow");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 0);
						DMibExpect(nActors.f_Load(), ==, 0);
					}
				}

				ClientTrustManager->f_BlockDestroy();
				ServerTrustManager->f_BlockDestroy();
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
