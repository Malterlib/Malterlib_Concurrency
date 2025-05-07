// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Concurrency/DistributedTrustDDPBridge>
#include <Mib/Web/DDPClient>
#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/DistributedTrustTestHelpers>
#include <Mib/Concurrency/DistributedActorTestHelpers>

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NEncoding;
using namespace NMib::NStr;
using namespace NMib::NFunction;
using namespace NMib::NWeb;
using namespace NMib::NNetwork;
using namespace NMib::NTest;

namespace
{
	static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;
	
	class CDistributedDDPBridge_Tests : public NMib::NTest::CTest
	{
	public:
		
		void f_DoTests()
		{
			DMibTestSuite("DDP")
			{
				CActorRunLoopTestHelper RunLoopHelper;

				CStr ProgramDirectory = NFile::CFile::fs_GetProgramDirectory();
				CStr RootDirectory = ProgramDirectory + "/DistributedDDPBridgeTests";

				fg_TestAddCleanupPath(RootDirectory);

				CTrustManagerTestHelper ServerState;
				CTrustManagerTestHelper ClientState;

				TCActor<CDistributedActorTrustManager> ServerTrustManager = ServerState.f_TrustManager("Server");
				TCActor<CDistributedActorTrustManager> ClientTrustManager = ClientState.f_TrustManager("Client");
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/server.sock"_f << RootDirectory);
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr)
						.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
					;
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}

				TCActor<CDistributedTrustDDPBridge> DdpBridge;
				DdpBridge = fg_ConstructActor<CDistributedTrustDDPBridge>(ServerTrustManager);
				
				DdpBridge(&CDistributedTrustDDPBridge::f_Startup).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout / 3);

				TCActor<CActor> HandlerActor{fg_Construct()};
				
				CActorSubscription HandlerSubscription = DdpBridge
					(
						&CDistributedTrustDDPBridge::f_RegisterMethods
						, fg_CreateVector<CDistributedTrustDDPBridge::CMethod>
						(
							{
								"testMethod"
								, g_ActorFunctor(HandlerActor) / [](NContainer::TCVector<NEncoding::CEJsonSorted> _Params) mutable -> TCFuture<NEncoding::CEJsonSorted>
								{
									if (_Params[0].f_GetMember("Invalid"))
										co_return DMibErrorInstance("Invalid params");

									auto &HostInfo = fg_GetCallingHostInfo();
									co_return NEncoding::CEJsonSorted
										{
											"Result"_= 5
											, "HostID"_= HostInfo.f_GetRealHostID()
										}
									;
								}
							}
						)
					)
					.f_CallSync()
				;
				auto ConnectToURL = ServerAddress.m_URL;
				ConnectToURL.f_SetPath({"ActorDDPBridge"});
				
				auto &ClientDatabase = ClientState.m_Database->f_AccessInternal();
				
				auto pClientConnection = ClientDatabase.m_ClientConnections.f_FindAny();
			
				CSSLSettings ClientSettings;
				ClientSettings.m_CACertificateData = pClientConnection->m_PublicServerCertificate;
				ClientSettings.m_PrivateKeyData = ClientDatabase.m_BasicConfig.m_CAPrivateKey;
				ClientSettings.m_PublicCertificateData = pClientConnection->m_PublicClientCertificate; 
				
				NStorage::TCSharedPointer<CSSLContext> pClientContext = fg_Construct(CSSLContext::EType_Client, ClientSettings);
						
				TCActor<CDDPClient> Client = fg_ConstructActor<CDDPClient>(ConnectToURL, "", fg_Default(), "", CSocket_SSL::fs_GetFactory(pClientContext));
			
				CDDPClient::CConnectInfo ConnectionInfo = Client(&CDDPClient::f_Connect, "", "", "", "", 20.0, nullptr).f_CallSync();
			
				DMibExpect(ConnectionInfo.m_UserID, ==, "");

				CEJsonSorted MethodParams =
					{
						"Test"_= "Test"
					}
				;
				CEJsonSorted MethodResult = Client(&CDDPClient::f_Method, CStr("testMethod"), fg_CreateVector(MethodParams)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout / 3);
				
				DMibExpect(MethodResult["HostID"].f_String(), == , ClientDatabase.m_BasicConfig.m_HostID);

				auto fCallInvalidParams = [&]
					{
						CEJsonSorted InvalidMethodParams =
							{
								"Invalid"_= "Test"
							}
						;
						Client(&CDDPClient::f_Method, CStr("testMethod"), fg_CreateVector(InvalidMethodParams)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout / 3);
					}
				;
				DMibExpectException(fCallInvalidParams(), DMibErrorInstance("exception: Invalid params"));
				
				fg_Exchange(HandlerSubscription, nullptr)->f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpectException(fCallInvalidParams(), DMibErrorInstance("method-not-found: Method not found"));
				
				ServerTrustManager->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
				ClientTrustManager->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
			};
		}
	};

	DMibTestRegister(CDistributedDDPBridge_Tests, Malterlib::Concurrency);
}
