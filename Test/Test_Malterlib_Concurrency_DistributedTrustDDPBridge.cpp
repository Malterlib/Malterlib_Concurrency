// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Concurrency/DistributedTrustDDPBridge>
#include <Mib/Web/DDPClient>
#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/DistributedTrustTestHelpers>

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
				CTrustManagerTestHelper ServerState;
				CTrustManagerTestHelper ClientState;

				TCActor<CDistributedActorTrustManager> ServerTrustManager = ServerState.f_TrustManager("Server");
				TCActor<CDistributedActorTrustManager> ClientTrustManager = ClientState.f_TrustManager("Client");
				
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = "wss://localhost:31409/";
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 30.0, -1).f_CallSync(g_Timeout);
				}

				TCActor<CDistributedTrustDDPBridge> DdpBridge;
				DdpBridge = fg_ConstructActor<CDistributedTrustDDPBridge>(ServerTrustManager);
				
				DdpBridge(&CDistributedTrustDDPBridge::f_Startup).f_CallSync(g_Timeout / 3);

				auto HandlerActor = fg_ConcurrentActor();
				
				CActorSubscription HandlerSubscription = DdpBridge
					(
						&CDistributedTrustDDPBridge::f_RegisterMethods
						, fg_CreateVector<CDistributedTrustDDPBridge::CMethod>
						(
							{
								"testMethod"
								, g_ActorFunctor(HandlerActor) / [](NContainer::TCVector<NEncoding::CEJSON> const &_Params) mutable -> TCFuture<NEncoding::CEJSON>
								{
									TCPromise<NEncoding::CEJSON> Promise;
									if (_Params[0].f_GetMember("Invalid"))
										return Promise <<= DMibErrorInstance("Invalid params");
									auto &HostInfo = fg_GetCallingHostInfo();
									return Promise <<= NEncoding::CEJSON
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
				
				CStr ConnectToURLString = "wss://localhost:31409/ActorDDPBridge";
				
				auto &ClientDatabase = ClientState.m_Database->f_AccessInternal();
				
				auto pClientConnection = ClientDatabase.m_ClientConnections.f_FindAny();
			
				CSSLSettings ClientSettings;
				ClientSettings.m_CACertificateData = pClientConnection->m_PublicServerCertificate;
				ClientSettings.m_PrivateKeyData = ClientDatabase.m_BasicConfig.m_CAPrivateKey;
				ClientSettings.m_PublicCertificateData = pClientConnection->m_PublicClientCertificate; 
				
				NStorage::TCSharedPointer<CSSLContext> pClientContext = fg_Construct(CSSLContext::EType_Client, ClientSettings);
						
				TCActor<CDDPClient> Client = fg_ConstructActor<CDDPClient>(ConnectToURLString, "", fg_Default(), "", CSocket_SSL::fs_GetFactory(pClientContext));
			
				CDDPClient::CConnectInfo ConnectionInfo = Client(&CDDPClient::f_Connect, "", "", "", "", 20.0, nullptr, nullptr).f_CallSync();
			
				DMibExpect(ConnectionInfo.m_UserID, ==, "");

				CEJSON MethodParams = 
					{
						"Test"_= "Test"
					}
				;
				CEJSON MethodResult = Client(&CDDPClient::f_Method, CStr("testMethod"), fg_CreateVector(MethodParams)).f_CallSync(g_Timeout / 3);
				
				DMibExpect(MethodResult["HostID"].f_String(), == , ClientDatabase.m_BasicConfig.m_HostID);

				auto fCallInvalidParams = [&]
					{
						CEJSON InvalidMethodParams = 
							{
								"Invalid"_= "Test"
							}
						;
						Client(&CDDPClient::f_Method, CStr("testMethod"), fg_CreateVector(InvalidMethodParams)).f_CallSync(g_Timeout / 3);
					}
				;
				DMibExpectException(fCallInvalidParams(), DMibErrorInstance("exception: Invalid params"));
				
				HandlerSubscription.f_Clear();
				DMibExpectException(fCallInvalidParams(), DMibErrorInstance("method-not-found: Method not found"));
				
				ServerTrustManager->f_BlockDestroy();
				ClientTrustManager->f_BlockDestroy();
			};
		}
	};

	DMibTestRegister(CDistributedDDPBridge_Tests, Malterlib::Concurrency);
}
