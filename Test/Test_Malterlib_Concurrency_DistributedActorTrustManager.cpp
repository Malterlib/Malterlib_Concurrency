// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Concurrency/DistributedTrustTestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Network/SSL>
#include <Mib/Encoding/JSONShortcuts>


using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NStr;

namespace
{
	class CDistributedActorTrustManager_Tests : public NMib::NTest::CTest
	{
	public:
		struct CTestActor : public CActor
		{
			enum : uint32
			{
				EMinProtocolVersion = 0x101
				, EProtocolVersion = 0x101
			};

			CTestActor()
			{
				DMibPublishActorFunction(CTestActor::f_Test);
				DMibPublishActorFunction(CTestActor::f_TestVoid);
			}

			void f_TestVoid()
			{
			}

			uint32 f_Test()
			{
				return 5;
			}
		};
		struct CTestActor2 : public CActor
		{
			enum : uint32
			{
				EMinProtocolVersion = 0x101
				, EProtocolVersion = 0x101
			};
			
			CTestActor2()
			{
				DMibPublishActorFunction(CTestActor2::f_Test);
			}
			
			uint32 f_Test()
			{
				return 5;
			}
		};
		struct CTestActor3 : public CActor
		{
			enum : uint32
			{
				EMinProtocolVersion = 0x101
				, EProtocolVersion = 0x101
			};
			
			CTestActor3()
			{
				DMibPublishActorFunction(CTestActor3::f_Test);
			}
			
			uint32 f_Test()
			{
				return 5;
			}
		};

		struct CState
		{
			auto f_CreateServerTrustManager(CStr const &_SessionID = {}) const
			{
				CDistributedActorTrustManager::COptions Options;
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNet::ENetFlag_None;
				Options.m_FriendlyName = "TestServer";
				Options.m_Enclave = _SessionID;
				Options.m_InitialConnectionTimeout = 30.0;
				Options.m_bRetryOnListenFailureDuringInit = false;

				return fg_ConstructActor<CDistributedActorTrustManager>(m_ServerDatabase, fg_Move(Options));
			}
			
			auto f_CreateClientTrustManager(CStr const &_SessionID = {}) const 
			{
				CDistributedActorTrustManager::COptions Options;
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNet::ENetFlag_None;
				Options.m_FriendlyName = "TestClient";
				Options.m_Enclave = _SessionID;
				Options.m_InitialConnectionTimeout = 30.0;
				Options.m_bRetryOnListenFailureDuringInit = false;

				return fg_ConstructActor<CDistributedActorTrustManager>(m_ClientDatabase, fg_Move(Options));
			}
			
			CState
				(
					NFunction::TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (CStr const &_Name)> const &_fDatabaseFactory
					, NFunction::TCFunction<void ()> const &_fCleanup
					, CStr const &_NameDistinguish = {} 
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

		struct CPermissionTestState
		{
			CPermissionTestState(CState &_State, uint16 _Port)
				: m_Port{_Port}
				, m_ServerTrustManager{f_CreateServerTrustManager(_State)}
				, m_ClientTrustManager{f_CreateClientTrustManager(_State)}
				, m_ServerHelper{f_InitServerTrustManager()}
				, m_ClientHelper{f_InitClientTrustManager()}
				, m_ServerHostInfo({}, "", CHostInfo{m_ServerHostID, m_ServerHostID}, "", 0)
			{
				m_ExpectedPermissions[m_ServerHostID]["com.malterlib/Test"];
				m_ExpectedEnumHostPermissions["com.malterlib/Test"][m_ServerHostID] = CHostInfo(m_ServerHostID, "TestServer");
				m_ExpectedEnumHostPermissionsNoHostInfo["com.malterlib/Test"][m_ServerHostID];
			}

			TCActor<CDistributedActorTrustManager> f_CreateServerTrustManager(CState &_State)
			{
				TCActor<CDistributedActorTrustManager> TrustManager = _State.f_CreateServerTrustManager();
				TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(60.0);
				return TrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_CreateClientTrustManager(CState &_State)
			{
				TCActor<CDistributedActorTrustManager> TrustManager = _State.f_CreateClientTrustManager();
				TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(60.0);
				return TrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_InitServerTrustManager()
			{
				m_ServerHostID = m_ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:{}/"_f << m_Port;
				if (!m_ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(60.0))
					m_ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				return m_ServerTrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_InitClientTrustManager()
			{
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:{}/"_f << m_Port;

				if (!m_ClientTrustManager(&CDistributedActorTrustManager::f_HasClientConnection, ServerAddress).f_CallSync(60.0))
				{
					auto TrustTicket = m_ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					m_ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
				}

				return m_ClientTrustManager;
			}

			~CPermissionTestState()
			{
				m_ClientTrustManager->f_BlockDestroy();
				m_ServerTrustManager->f_BlockDestroy();
			}

			template <typename ...tfp_CPermission>
			void f_AddHostPermissions(tfp_CPermission &&...p_Permissions)
			{
				m_ClientTrustManager
					(
						 &CDistributedActorTrustManager::f_AddHostPermissions
						 , m_ServerHostID
						 , fg_CreateSet<CStr>(p_Permissions...)
						 , EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(60.0)
				;
			}

			template <typename ...tfp_CPermission>
			void f_RemoveHostPermissions(tfp_CPermission &&...p_Permissions)
			{
				m_ClientTrustManager
					(
						 &CDistributedActorTrustManager::f_RemoveHostPermissions
						 , m_ServerHostID
						 , fg_CreateSet<CStr>(p_Permissions...)
						 , EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(60.0)
				;
			}

			uint16 m_Port = 0;

			TCActor<CDistributedActorTrustManager> m_ServerTrustManager;
			TCActor<CDistributedActorTrustManager> m_ClientTrustManager;
			CStr m_ServerHostID;
			CDistributedActorTestHelper m_ServerHelper;
			CDistributedActorTestHelper m_ClientHelper;
			CCallingHostInfo m_ServerHostInfo;

			TCActor<> m_TestActor = fg_ConcurrentActor();

			NContainer::TCMap<CStr, NContainer::TCSet<CStr>> m_ExpectedPermissionsEmpty;
			NContainer::TCMap<CStr, NContainer::TCSet<CStr>> m_ExpectedPermissions;
			NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> m_ExpectedEnumHostPermissionsEmpty;
			NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> m_ExpectedEnumHostPermissions;
			NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> m_ExpectedEnumHostPermissionsNoHostInfo;
		};

		void fp_DoTests(TCActor<CDistributedActorTrustManager> const &_ServerTrustManager, TCActor<CDistributedActorTrustManager> const &_ClientTrustManager)
		{
			CDistributedActorTestHelper ServerHelper{_ServerTrustManager};
			CDistributedActorTestHelper ClientHelper{_ClientTrustManager};

			ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
			CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

			TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

			DMibCallActor(Actor, CTestActor::f_TestVoid).f_CallSync(60.0);

			uint32 Result = DMibCallActor(Actor, CTestActor::f_Test).f_CallSync(60.0);
			DMibExpect(Result, ==, 5);
		}
		
		void fp_DoBasicTests
			(
				NFunction::TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (CStr const &_Name)> const &_fDatabaseFactory
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

					CStr ServerHostID = ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					CStr ClientHostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					
					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "wss://localhost:31394/";
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(60.0));

					auto AllListens = ServerTrustManager(&CDistributedActorTrustManager::f_EnumListens).f_CallSync(60.0);
					NContainer::TCSet<CDistributedActorTrustManager_Address> ExpectedListens;
					ExpectedListens[ServerAddress];
					DMibExpect(AllListens, ==, ExpectedListens);
					
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
					DMibExpectTrue(ClientTrustManager(&CDistributedActorTrustManager::f_HasClientConnection, TrustTicket.m_Ticket.m_ServerAddress).f_CallSync(60.0));
					
					auto AllClientConnections = ClientTrustManager(&CDistributedActorTrustManager::f_EnumClientConnections).f_CallSync(60.0);
					NContainer::TCMap<CDistributedActorTrustManager_Address, CDistributedActorTrustManager::CClientConnectionInfo> ExpectedClientConnections;
					{
						auto &ExpectedHostInfo = ExpectedClientConnections[TrustTicket.m_Ticket.m_ServerAddress];
						ExpectedHostInfo.m_HostInfo.m_FriendlyName = "TestServer";
						ExpectedHostInfo.m_HostInfo.m_HostID = ServerHostID;
						ExpectedHostInfo.m_ConnectionConcurrency = -1;
					}
					DMibExpect(AllClientConnections, ==, ExpectedClientConnections);
					
					fp_DoTests(ServerTrustManager, ClientTrustManager);
					
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasClient, ClientHostID).f_CallSync(60.0));
					
					auto AllClients = ServerTrustManager(&CDistributedActorTrustManager::f_EnumClients).f_CallSync(60.0);
					NContainer::TCMap<CStr, CHostInfo> ExpectedClients;
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
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, ServerAddress, -1).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, OldServerAddress).f_CallSync(60.0);
					
					fp_DoTests(ServerTrustManager, ClientTrustManager);
					
					ClientTrustManager->f_BlockDestroy();
					ServerTrustManager->f_BlockDestroy();
				}

				{
					DMibTestPath("Remove client");
					CStr HostID;
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

					DMibAssertTrue(Address.m_States.f_GetLen() == 1);
					
					auto &ConcurrentConnectionState = Address.m_States[0];
					
					DMibExpectFalse(ConcurrentConnectionState.m_bConnected);
					DMibExpect(ConcurrentConnectionState.m_Error, !=, "");
					
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

				auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
				
				ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
				
				fp_DoTests(ServerTrustManager, ClientTrustManager);
				
				auto HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
				ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(60.0);
				
				auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
				
				DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
				DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());
				
				auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();
				
				DMibAssertTrue(Address.m_States.f_GetLen() == 1);
				
				auto &ConcurrentConnectionState = Address.m_States[0];
				
				DMibExpectFalse(ConcurrentConnectionState.m_bConnected);
				DMibExpect(ConcurrentConnectionState.m_Error, !=, "");
				
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
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
				CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);
				
				TCActor<CSeparateThreadActor> DispatchActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Dispatch"));

				CStr DispatchError;
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
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
					NSys::fg_Thread_Sleep(0.05f);
				}

				DispatchActor->f_BlockDestroy();
				
				DMibExpect(DispatchError, ==, "");
				DMibExpect(nCalls, >, 1u);
				
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
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);

					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, ServerAddress).f_CallSync(60.0);
					
					// Check that you cannot reuse old ticket
					DMibExpectExceptionType(ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0), NException::CException);
					
					// Check that you can add trust to server, even with old client in database
					TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
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
					Options.m_KeySetting = CDistributedActorTestKeySettings{};
					Options.m_CommonName = fg_Format("Malterlib Distributed Actors Root - {}", BasicConfig.m_HostID).f_Left(64); 
					auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
					Extension.m_bCritical = false; 
					Extension.m_Value = BasicConfig.m_HostID;
					
					NNet::CSignOptions SignOptions;
					SignOptions.m_Days = 100*365;
					
					NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey(Options, BasicConfig.m_CACertificate, BasicConfig.m_CAPrivateKey, SignOptions);
						
					Client2Database(&ICDistributedActorTrustManagerDatabase::f_SetBasicConfig, BasicConfig).f_CallSync(60.0);
				}
				
				CDistributedActorTrustManager::COptions Options;
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNet::ENetFlag_None;
				Options.m_FriendlyName = "ClientTrustManager2";
				
				TCActor<CDistributedActorTrustManager> Client2TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(Client2Database, fg_Move(Options));
				
				auto TrustTicket2 = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
				// Test fraudulent client add
				DMibExpectExceptionType(Client2TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket2.m_Ticket, 30.0, -1).f_CallSync(60.0), NException::CException);
				
				{
					DMibTestPath("After fraudulent try");
					fp_DoTests(ServerTrustManager, ClientTrustManager);
				}
				Client2TrustManager->f_BlockDestroy();
				Client2Database->f_BlockDestroy();
				ClientTrustManager->f_BlockDestroy();
				ServerTrustManager->f_BlockDestroy();
			};
			DMibTestSuite("Multiple Enclaves")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31408/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(60.0);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};


				ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
				CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");
				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

				TCVector<TCActor<CDistributedActorTrustManager>> ClientTrustManagers;
				for (mint i = 0; i < 64; ++i)
					ClientTrustManagers.f_Insert() = State.f_CreateClientTrustManager(NCryptography::fg_RandomID());
				
				TCLinkedList<CDistributedActorTestHelper> ClientHelpers;
				TCLinkedList<TCDistributedActor<CTestActor>> ClientActors;
				
				for (auto &ClientManager : ClientTrustManagers)
				{
					auto &ClientHelper = ClientHelpers.f_Insert(fg_Construct(ClientManager));
					
					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");
					ClientActors.f_Insert(ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
				}
				
				TCActorResultVector<uint32> Results;

				DMibCallActor(Actor, CTestActor::f_Test) > Results.f_AddResult();
				
				for (auto &ClientActor : ClientActors)
					DMibCallActor(ClientActor, CTestActor::f_Test) > Results.f_AddResult();
				
				auto ResultsVector = Results.f_GetResults().f_CallSync(10.0);
				for (auto &Result : ResultsVector)
					DMibExpect(*Result, ==, 5)(ETestFlag_Aggregated);

				ServerTrustManager->f_BlockDestroy();
				ClientTrustManager->f_BlockDestroy();
				ClientActors.f_Clear();
				ClientHelpers.f_Clear();
				for (auto &ClientManager : ClientTrustManagers)
					ClientManager->f_BlockDestroy();
			};
			static constexpr auto c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
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
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};
				
				TCSet<CStr> ServerHosts;
				ServerHosts[ServerHostID];
				
				TCSet<CStr> Published;
				
				auto TestActor = fg_ConcurrentActor();
				{
					DMibTestPath("Namespace names");
					
					auto fPublish = [&](CStr const &_Namespace)
						{
							auto Publication = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), _Namespace);
							ServerHelper.f_Unpublish(Publication);
						}
					;

					DMibExpectException(fPublish("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fPublish("Test/55");

					auto fSubscribe = [&](CStr const &_Namespace)
						{
							auto Subscription = ClientHelper.f_Subscribe(_Namespace, 0);
							ServerHelper.f_Unsubscribe(Subscription);
						}
					;

					DMibExpectException(fSubscribe("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fSubscribe("Test/55");

					auto fSubscribeTrusted = [&](CStr const &_Namespace)
						{
							ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, _Namespace, TestActor).f_CallSync(60.0);
						}
					;
					
					DMibExpectException(fSubscribeTrusted("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fSubscribeTrusted("Test/55");

					auto fAllowNamespace = [&](CStr const &_Namespace)
						{
							ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
						}
					;
					auto fDisallowNamespace = [&](CStr const &_Namespace)
						{
							ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
						}
					;
					
					DMibExpectException(fAllowNamespace("Test$55"), DMibErrorInstance("Invalid namespace name"));
					DMibExpectException(fDisallowNamespace("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fAllowNamespace("Test/55");
					fDisallowNamespace("Test/55");
					
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
				}
				{
					DMibTestPath("General");
					auto TrustedSubscription = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("Before publish");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
					}
					Published[ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test")];
					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

					// Make sure that queue has been processed on test actor
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After publish");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
					}
					
					NContainer::TCMap<CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnum;
					{
						auto &Host = ExpectedAllowedEnum["com.malterlib/Test"].m_AllowedHosts[ServerHostID];
						Host.m_HostID = ServerHostID;
						Host.m_FriendlyName = "TestServer";
					}
					NContainer::TCMap<CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnumEmpty;
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After allow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					{
						DMibTestPath("After allow again");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(60.0);
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					}
					
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					{
						DMibTestPath("After disallow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						NContainer::TCMap<CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnum;
						{
							auto &Host = ExpectedAllowedEnum["com.malterlib/Test"].m_DisallowedHosts[ServerHostID];
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
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					{
						DMibTestPath("After allow without subscription");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}
					
					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					}
					{
						DMibTestPath("After nonexistant disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}
					
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					 // Test disallow without namespace
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					{
						DMibTestPath("After double disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(60.0);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnumEmpty);
					}
				}
				{
					DMibTestPath("Double");
					Published[ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test")];
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					
					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
					auto TrustedSubscription1 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
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
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					
					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto Subscriptions =
						(
							ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor)
							+ ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor)
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
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					
					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
					}
					CStr NewPublish = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
					Published[NewPublish];
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 3), ==, 3);
					
					{
						DMibTestPath("2 types");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test");
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						ServerHelper.f_Unpublish(NewPublish2);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
					}
					{
						DMibTestPath("2 types no sub unpublish");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test");
						CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen() + 1);
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						TrustedSubscription2.f_Clear();
						ServerHelper.f_Unpublish(NewPublish2);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription))
								break;
						}
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ClientHelper.f_Unsubscribe(Subscription);
					}
					{
						DMibTestPath("2 types not allowed");
						auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "com.malterlib/Test2", TestActor).f_CallSync(60.0);
						auto TrustedSubscription3 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test2", TestActor).f_CallSync(60.0);
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test2");
						CStr NewPublish3 = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test2");
						CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test2", 2);
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ServerHelper.f_Unpublish(NewPublish2);
						ServerHelper.f_Unpublish(NewPublish3);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && !ClientHelper.f_GetRemoteActor<CTestActor>(Subscription))
								break;
						}
						DMibExpectTrue(!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && !ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
						fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
						ClientHelper.f_Unsubscribe(Subscription);
					}
					
					ServerHelper.f_Unpublish(NewPublish);
					Published.f_Remove(NewPublish);
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

						auto TrustTicket = ServerTrustManager2(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr).f_CallSync(60.0);
						ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(60.0);
					}
					CStr ServerHostID2 = ServerTrustManager2(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0);
					TCSet<CStr> ServerHosts2;
					ServerHosts2[ServerHostID];
					
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test3", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test3", ServerHosts2, c_WaitForSubscriptions).f_CallSync(60.0);
					auto TrustedSubscription2 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>, "com.malterlib/Test3", TestActor).f_CallSync(60.0);
					CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test3");
					CStr NewPublish3 = ServerHelper2.f_Publish<CTestActor2>(ServerHelper2.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test3");
					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test3", 2);
					DMibExpectTrue(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription));
					ServerHelper.f_Unpublish(NewPublish2);
					ServerHelper2.f_Unpublish(NewPublish3);
					NTime::CClock Clock{true};
					while (Clock.f_GetTime() < 10.0)
					{
						if (!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription))
							break;
					}
					DMibExpectFalse(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription));
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test3", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
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
									ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor) > fg_DiscardResult();
								}
							)
							> Dispatches.f_AddResult(); 
						;
					}
					Dispatches.f_GetResults().f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
				}
				{
					DMibTestPath("Notifications");
					NAtomic::TCAtomic<mint> nActors{0};
					CStr Published = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test");
					auto TrustedSubscription0 = ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", TestActor).f_CallSync(60.0);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 0);
					}
					
					TrustedSubscription0.f_OnActor
						(
							[&](TCDistributedActor<CTestActor> const &_Actor, CTrustedActorInfo const &_ActorInfo)
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

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
					fg_Dispatch(TestActor, []{}).f_CallSync(60.0);
					
					{
						DMibTestPath("After allow");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 1);
						DMibExpect(nActors.f_Load(), ==, 1);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(60.0);
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
			DMibTestSuite("Permissions")
			{
				CState State{_fDatabaseFactory, _fCleanup};

				CPermissionTestState TestState{State, 31407};

				{
					DMibTestPath("General");
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test", "com.malterlib/Test2")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;
					{
						DMibTestPath("Before add permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Before add permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumHostPermissionsEmpty);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumHostPermissionsEmpty);
					}
					
					TestState.f_AddHostPermissions("com.malterlib/Test");
					TestState.f_AddHostPermissions("com.malterlib/Test");
					{
						DMibTestPath("After add permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissions);
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumHostPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumHostPermissionsNoHostInfo);
					}

					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					{
						DMibTestPath("After remove permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After remove permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumHostPermissionsEmpty);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumHostPermissionsEmpty);
					}
					
					TestState.f_AddHostPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						[[maybe_unused]] auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					}
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test2");
					{
						DMibTestPath("After remove double permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
					}
					TestState.f_AddHostPermissions("com.malterlib/Test3");
					{
						DMibTestPath("After add irrelevant permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
					}
					TestState.f_RemoveHostPermissions("com.malterlib/Test3");
				}
				{
					DMibTestPath("Double wildcards");
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("*", "*", "com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;

					TestState.f_AddHostPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						[[maybe_unused]] auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					}
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test2");
					{
						DMibTestPath("After remove double permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
					}
				}
				{
					DMibTestPath("Notifications");
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("*", "*", "com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;
					
					NAtomic::TCAtomic<mint> nPermissions{0};
					TrustedSubscription.f_OnPermissionsAdded
						(
							[&](CStr const &, TCSet<CStr> const &_Permissions)
							{
								nPermissions.f_FetchAdd(_Permissions.f_GetLen());
							}
						)
					;
					TrustedSubscription.f_OnPermissionsRemoved
						(
							[&](CStr const &, TCSet<CStr> const &_Permissions)
							{
								nPermissions.f_FetchSub(_Permissions.f_GetLen());
							}
						)
					;

					TestState.f_AddHostPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						DMibExpectTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibExpectTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibExpect(nPermissions.f_Load(), ==, 2);
						
					}
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
					TestState.f_RemoveHostPermissions("com.malterlib/Test2");
					{
						DMibTestPath("After remove double permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibExpect(nPermissions.f_Load(), ==, 0);
					}
				}
				{
					DMibTestPath("Registered permissions");
					TestState.f_AddHostPermissions("com.malterlib/Test");
					{
						DMibTestPath("After add permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumHostPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumHostPermissionsNoHostInfo);
					}

					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RegisterPermissions, fg_CreateSet<CStr>("com.malterlib/Test5")).f_CallSync(60.0);
					{
						DMibTestPath("After register permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						auto ExpectedEnumHostPermissions2 = TestState.m_ExpectedEnumHostPermissions;
						ExpectedEnumHostPermissions2["com.malterlib/Test5"];
						DMibAssert(EnumPermissions, ==, ExpectedEnumHostPermissions2);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						auto ExpectedEnumHostPermissionsNoHostInfo2 = TestState.m_ExpectedEnumHostPermissionsNoHostInfo;
						ExpectedEnumHostPermissionsNoHostInfo2["com.malterlib/Test5"];
						DMibAssert(EnumPermissionsNoHostInfo, ==, ExpectedEnumHostPermissionsNoHostInfo2);
					}

					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_UnregisterPermissions, fg_CreateSet<CStr>("com.malterlib/Test5")).f_CallSync(60.0);
					{
						DMibTestPath("After unregister permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, true).f_CallSync(60.0);
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumHostPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, false).f_CallSync(60.0);
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumHostPermissionsNoHostInfo);
					}
					TestState.f_RemoveHostPermissions("com.malterlib/Test");
				}
			};
			DMibTestSuite("Permissions Database")
			{
				CState State{_fDatabaseFactory, _fCleanup};
				{
					DMibTestPath("Setup");
					CPermissionTestState TestState{State, 31411};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;
					TestState.f_AddHostPermissions("com.malterlib/Test", "com.malterlib/Test2", "com.malterlib/Test3");
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
				}
				{
					DMibTestPath("After Reload");
					CPermissionTestState TestState{State, 31411};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(60.0));

					TestState.f_RemoveHostPermissions("com.malterlib/Test2", "com.malterlib/Test3");

					{
						DMibTestPath("After Remove");
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					}
				}
				{
					DMibTestPath("After Remove Reload");
					CPermissionTestState TestState{State, 31411};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(60.0)
					;
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
					DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(60.0));
				}
			};
			DMibTestSuite("User Database")
			{
				using CMetadata = TCMap<CStr, NEncoding::CEJSON>;
				using CKeys = TCSet<CStr>;
				CState State{_fDatabaseFactory, _fCleanup};
				CStr const ID1 = "2YAzJPcR2K5QMbJYP";
				CStr const ID2 = "DPYQEvAqw4RQhXRYe";
				CStr const ID3 = "JNsXrbgP3dL6xuFXm";
				CStr const ID4 = "Tao5Dmb6FpMzTCQyD";
				NStorage::TCOptional<NStr::CStr> UnsetUserName;

				CMetadata Empty;
				CMetadata Metadata2{{"Key", "Value"}};
				CMetadata Metadata3{{"Key2", "FunkyStuff"}};
				Metadata3["Key1"] = {"Key1"_= "NewValue1", "Key2"_= "Value2"};
				CMetadata Metadata4{{"Key0", "Value0"}};
				Metadata4["Key1"] = "Value1";
				
				TCSet<CStr> NoRemove;
				{
					DMibTestPath("Initial");
					CPermissionTestState TestState{State, 31407};
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID1, "User1").f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID2, "User2").f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID3, "User3").f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID4, "User4").f_CallSync(60.0);

					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID1, "User1 again").f_CallSync(60.0)
							, DMibErrorInstance("User '2YAzJPcR2K5QMbJYP' already exists")
						)
					;
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, "**COOL**", "Invalid ID").f_CallSync(60.0)
							, DMibErrorInstance("Invalid user ID")
						)
					;

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(60.0);

					DMibExpect(AllUsers.f_GetLen(), ==, 4);
					DMibExpect(AllUsers[ID1].m_UserName, ==, "User1");
					DMibExpect(AllUsers[ID2].m_UserName, ==, "User2");
					DMibExpect(AllUsers[ID3].m_UserName, ==, "User3");
					DMibExpect(AllUsers[ID4].m_UserName, ==, "User4");

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, NoRemove, Metadata2).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID3, UnsetUserName, NoRemove, Metadata3).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID4, UnsetUserName, NoRemove, Metadata4).f_CallSync(60.0);
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, "**COOL**", UnsetUserName, NoRemove, Metadata4).f_CallSync(60.0)
							, DMibErrorInstance("Invalid user ID")
						)
					;
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, "1234567890", UnsetUserName, NoRemove, Metadata4).f_CallSync(60.0)
							, DMibErrorInstance("User '1234567890' does not exist")
						)
					;

					AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(60.0);
					DMibExpectTrue(AllUsers[ID1].m_Metadata.f_IsEmpty());
					DMibExpect(AllUsers[ID2].m_Metadata, ==, Metadata2);
					DMibExpect(AllUsers[ID3].m_Metadata, ==, Metadata3);
					DMibExpect(AllUsers[ID4].m_Metadata, ==, Metadata4);
				}
				{
					DMibTestPath("From database");
					CPermissionTestState TestState{State, 31407};

					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(60.0);

					DMibExpect(AllUsers.f_GetLen(), ==, 4);
					DMibExpect(AllUsers[ID1].m_UserName, ==, "User1");
					DMibExpect(AllUsers[ID2].m_UserName, ==, "User2");
					DMibExpect(AllUsers[ID3].m_UserName, ==, "User3");
					DMibExpect(AllUsers[ID4].m_UserName, ==, "User4");
					DMibExpectTrue(AllUsers[ID1].m_Metadata.f_IsEmpty());
					DMibExpect(AllUsers[ID2].m_Metadata, ==, Metadata2);
					DMibExpect(AllUsers[ID3].m_Metadata, ==, Metadata3);
					DMibExpect(AllUsers[ID4].m_Metadata, ==, Metadata4);
				}
				{
					DMibTestPath("Set userinfo and remove metadata");
					CPermissionTestState TestState{State, 31407};

					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();


					NStorage::TCOptional<NStr::CStr> SetUserName{"NewUser1"};
					CMetadata UnsetMetadata;
					CMetadata SetMetadata;
					SetMetadata["Key1"] = "NewValue";
					SetMetadata["Key2"] = "Value2";
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID1, SetUserName, CKeys{}, UnsetMetadata).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID4, UnsetUserName, CKeys{}, SetMetadata).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, CKeys{"Key"}, CMetadata{}).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID3, UnsetUserName, CKeys{"Key1"}, CMetadata{}).f_CallSync(60.0);

					Metadata3.f_Remove("Key1");
					SetMetadata["Key0"] = "Value0";
					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(60.0);
					DMibExpect(AllUsers[ID1].m_UserName, ==, "NewUser1");
					DMibExpect(AllUsers[ID2].m_UserName, ==, "User2");
					DMibExpect(AllUsers[ID3].m_UserName, ==, "User3");
					DMibExpect(AllUsers[ID4].m_UserName, ==, "User4");
					DMibExpectTrue(AllUsers[ID1].m_Metadata.f_IsEmpty());
					DMibExpectTrue(AllUsers[ID2].m_Metadata.f_IsEmpty());
					DMibExpect(AllUsers[ID3].m_Metadata, ==, Metadata3);
					DMibExpect(AllUsers[ID4].m_Metadata, ==, SetMetadata);

					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, CKeys{"Key"}, CMetadata{}).f_CallSync(60.0);
							, DMibErrorInstance("Key 'Key' does not exist")
						)
					;
				}
				{
					DMibTestPath("Remove user");
					CPermissionTestState TestState{State, 31407};

					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID2).f_CallSync(60.0);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID4).f_CallSync(60.0);

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(60.0);

					DMibExpect(AllUsers.f_GetLen(), ==, 2);
					DMibExpect("NewUser1", ==, AllUsers[ID1].m_UserName);
					DMibExpect("User3", ==, AllUsers[ID3].m_UserName);

					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID2).f_CallSync(60.0)
							, DMibErrorInstance("No user with ID 'DPYQEvAqw4RQhXRYe'")
						)
					;


				}
			};
		}
		void f_DoTests()
		{
			DMibTestCategory("Dummy Database")
			{
				fp_DoBasicTests
					(
						[](CStr const &_Name)
						{
							return fg_ConstructActor<CTrustManagerDatabaseTestHelper>();
						}
						, []
						{
						}
					)
				;
			};
			DMibTestCategory("JSON Directory Database")
			{
				CStr BaseDirectory = NFile::CFile::fs_GetProgramDirectory() + "/TestTrustManager";
				fp_DoBasicTests
					(
						[BaseDirectory](CStr const &_Name)
						{
							return fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(BaseDirectory + "/" + _Name);
						}
						, [BaseDirectory]
						{
							for (mint i = 0; i < 5; ++i)
							{
								try
								{
									if (NFile::CFile::fs_FileExists(BaseDirectory))
										NFile::CFile::fs_DeleteDirectoryRecursive(BaseDirectory);
									break;
								}
								catch (NFile::CExceptionFile const &)
								{
								}
							}
						}
					)
				;
			};
		}
	};

	DMibTestRegister(CDistributedActorTrustManager_Tests, Malterlib::Concurrency);
}
