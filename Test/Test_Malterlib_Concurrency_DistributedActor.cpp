// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Test/Performance>
#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/TestHelpers>
#include <Mib/Cryptography/RandomID>
#include <Mib/Web/WebSocket>
#include <Mib/Log/Destinations>
#include <Mib/File/File>

#include <type_traits>

namespace
{
	using namespace NMib;
	using namespace NMib::NPtr;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NStr;
	
	namespace NTest1
	{
		struct CTest
		{
		};
	}
	
	namespace NTest2
	{
		struct CTest
		{
		};
	}

	class CDistributedActorBase : public CActor
	{
	public:
		virtual void f_AddIntVirtual(uint32 _Value) = 0;
		virtual uint32 f_GetResultVirtual() = 0;
		virtual CStr f_GetCallingHostID() const = 0;
	};

	class CDistributedActor : public CDistributedActorBase
	{
		zuint32 m_Value;
		TCMap<CStr, CActorSubscription> m_CallingHostTests;
	public:
		void f_AddInt(uint32 _Value)
		{
			m_Value += _Value;
		}
		uint32 f_GetResult()
		{
			auto Ret = m_Value;
			m_Value = 0;
			return Ret;
		}

		void f_AddIntVirtual(uint32 _Value) override
		{
			m_Value += _Value;
		}
		
		uint32 f_GetResultVirtual() override
		{
			auto Ret = m_Value;
			m_Value = 0;
			return Ret;
		}

		CStr f_GetCallingHostID() const override
		{
			return CActorDistributionManager::fs_GetCallingHostInfo().f_GetRealHostID();
		}
		
		TCContinuation<CStr> f_TestOnDisconnect() 
		{
			auto &HostInfo = CActorDistributionManager::fs_GetCallingHostInfo();
			CStr HostID = HostInfo.f_GetRealHostID();
			
			if (HostID.f_IsEmpty())
				return fg_Explicit(CStr());
			
			TCContinuation<CStr> Continuation;
			HostInfo.f_OnDisconnect
				(
					fg_ThisActor(this)
					, [this, HostID]
					{
						m_CallingHostTests.f_Remove(HostID);
					}
				)
				> Continuation / [this, Continuation, HostID](CActorSubscription &&_Subscription)
				{
					if (!_Subscription)
					{
						Continuation.f_SetException(DMibErrorInstance("Subscription failed"));
						return;
					}
					m_CallingHostTests[HostID] = fg_Move(_Subscription);
					
					Continuation.f_SetResult(HostID);
				}
			;
			
			return Continuation;
		}
		
		uint8 f_HasOnDisconnect(CStr const &_HostID) const
		{
			return m_CallingHostTests.f_FindEqual(_HostID) != nullptr;
		}
		
		template <typename tf_CTest>
		uint32 f_GetResultTemplated()
		{
			auto Ret = m_Value;
			m_Value = 0;
			return Ret;
		}
		
		TCContinuation<uint32> f_GetResultDeferred()
		{
			TCContinuation<uint32> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Continuation](TCAsyncResult<uint32> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntDeferred(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Continuation](TCAsyncResult<void> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> f_GetResultException()
		{
			TCContinuation<uint32> Continuation;
			Continuation.f_SetException(DMibErrorInstance("Test"));
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntException(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			Continuation.f_SetException(DMibErrorInstance("Test"));
			return Continuation;
		}

		TCContinuation<uint32> f_GetResultDeferredException()
		{
			TCContinuation<uint32> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Continuation](TCAsyncResult<uint32> &&_Result)
				{
					Continuation.f_SetException(DMibErrorInstance("Test"));
				}
			;
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntDeferredException(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Continuation](TCAsyncResult<void> &&_Result)
				{
					Continuation.f_SetException(DMibErrorInstance("Test"));
				}
			;
			return Continuation;
		}
	};
	
	class CDistributedActor_Tests : public NMib::NTest::CTest
	{
		void fp_RunTests(TCFunction<TCDistributedActor<CDistributedActor> ()> const &_fGetActor)
		{
			DMibTestSuite("Direct")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				DMibCallActor(Actor, CDistributedActor::f_AddInt, 5).f_CallSync(60.0);
				
				uint32 Result = DMibCallActor(Actor, CDistributedActor::f_GetResult).f_CallSync(60.0);
				DMibExpect(Result, ==, 5);
			};
			DMibTestSuite("DirectTemplate")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				DMibCallActor(Actor, CDistributedActor::f_AddInt, 5).f_CallSync(60.0);

				uint32 Result1 = DMibCallActor(Actor, CDistributedActor::f_GetResultTemplated<NTest1::CTest>).f_CallSync(60.0);
				DMibExpect(Result1, ==, 5);
				
				DMibCallActor(Actor, CDistributedActor::f_AddInt, 5).f_CallSync(60.0);
				uint32 Result2 = DMibCallActor(Actor, CDistributedActor::f_GetResultTemplated<NTest2::CTest>).f_CallSync(60.0);
				DMibExpect(Result2, ==, 5);
			};
			DMibTestSuite("Virtual")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				DMibCallActor(Actor, CDistributedActorBase::f_AddIntVirtual, 5).f_CallSync(60.0);

				uint32 Result = DMibCallActor(Actor, CDistributedActorBase::f_GetResultVirtual).f_CallSync(60.0);
				DMibExpect(Result, ==, 5);
			};
			DMibTestSuite("VirtualActor")
			{
				TCDistributedActor<CDistributedActorBase> Actor = _fGetActor();
				
				DMibCallActor(Actor, CDistributedActorBase::f_AddIntVirtual, 5).f_CallSync(60.0);

				uint32 Result = DMibCallActor(Actor, CDistributedActorBase::f_GetResultVirtual).f_CallSync(60.0);
				DMibExpect(Result, ==, 5);
			};
			DMibTestSuite("Deferred")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				DMibCallActor(Actor, CDistributedActor::f_AddIntDeferred, 5).f_CallSync(60.0);
				
				uint32 Result = DMibCallActor(Actor, CDistributedActor::f_GetResultDeferred).f_CallSync(60.0);
				DMibExpect(Result, ==, 5);
			};
			DMibTestSuite("Exception")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				auto fTestCall = [&]
					{
						DMibCallActor(Actor, CDistributedActor::f_AddIntException, 5).f_CallSync(60.0);
					}
				;
				auto fTestResult = [&]
					{
						return DMibCallActor(Actor, CDistributedActor::f_GetResultException).f_CallSync(60.0);
					}
				;

				DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
				DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
			};
			DMibTestSuite("DeferredException")
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				auto fTestCall = [&]
					{
						DMibCallActor(Actor, CDistributedActor::f_AddIntDeferredException, 5).f_CallSync(60.0);
					}
				;
				auto fTestResult = [&]
					{
						return DMibCallActor(Actor, CDistributedActor::f_GetResultDeferredException).f_CallSync(60.0);
					}
				;

				DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
				DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
				
				
			}; 
		}
		
		void fp_BasicTests(NStr::CStr const &_Address)
		{
			CStr ServerHostID = NCryptography::fg_RandomID();
			CStr ClientHostID = NCryptography::fg_RandomID();
			TCActor<CActorDistributionManager> ServerManager = fg_ConstructActor<CActorDistributionManager>(ServerHostID);
			TCActor<CActorDistributionManager> ClientManager = fg_ConstructActor<CActorDistributionManager>(ClientHostID);
			
			CActorDistributionCryptographySettings ServerCryptography{ServerHostID};
			ServerCryptography.f_GenerateNewCert(fg_CreateVector<CStr>(_Address), 1024);
			
			NHTTP::CURL ConnectAddress;
			
			if (_Address == "localhost")
			{
				NNet::CNetAddressTCPv4 Address;
				Address.m_Port = 31393;
				ConnectAddress = fg_Format("wss://{}:31393/", _Address);
			}
			else
			{
				ConnectAddress = fg_Format("wss://[{}]/", _Address);
			}
			
			CActorDistributionListenSettings ListenSettings{fg_CreateVector(ConnectAddress)};
			ListenSettings.f_SetCryptography(ServerCryptography);
			ListenSettings.m_bRetryOnListenFailure = false;
			ListenSettings.m_ListenFlags = NNet::ENetFlag_ReuseAddress;
			CDistributedActorListenReference ListenReference = ServerManager(&CActorDistributionManager::f_Listen, ListenSettings).f_CallSync(60.0);

			TCDistributedActor<CDistributedActor> PublishedActor = fg_ConstructDistributedActor<CDistributedActor>();
			
			auto ActorPublication = ServerManager
				(
					&CActorDistributionManager::f_PublishActor
					, PublishedActor
					, "Test"
					, NMib::NConcurrency::CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy<CDistributedActorBase>()
				).f_CallSync(60.0)
			;
			
			CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = ConnectAddress;
			ConnectionSettings.m_PublicServerCertificate = ListenSettings.m_CACertificate;
			CActorDistributionCryptographySettings ClientCryptography{ClientHostID};
			ClientCryptography.f_GenerateNewCert(fg_CreateVector<CStr>(_Address), 1024);
			auto CertificateRequest = ClientCryptography.f_GenerateRequest();
			
			auto SignedRequest = ServerCryptography.f_SignRequest(CertificateRequest);
			
			ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, ServerCryptography.m_PublicCertificate, SignedRequest);
			
			ConnectionSettings.f_SetCryptography(ClientCryptography);
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			NThread::CMutual RemoteLock;
			NThread::CEventAutoReset RemoteEvent;
			TCDistributedActor<CDistributedActorBase> RemoteActor;
			CActorSubscription RemoteActorsSubscription;
			mint RemoteEvents = 0;
			bool bRemoved = false;
			
			auto &ConcurrentActor = fg_ConcurrentActor();
			
			ClientManager
				(
					&CActorDistributionManager::f_SubscribeActors
					, fg_CreateVector<CStr>("Test")
					, ConcurrentActor 
					, [&](CAbstractDistributedActor &&_NewActor)
					{
						DMibLock(RemoteLock);
						RemoteActor = _NewActor.f_GetActor<CDistributedActorBase>();
						RemoteEvent.f_Signal();
						++RemoteEvents;
					}
					, [&](TCWeakDistributedActor<CActor> const &_RemovedActor)
					{
						DMibLock(RemoteLock);
						if (_RemovedActor == RemoteActor)
						{
							RemoteActor.f_Clear();
							bRemoved = true;
						}
						RemoteEvent.f_Signal();
						++RemoteEvents;
					}
				)
				> ConcurrentActor / [&](TCAsyncResult<CActorSubscription> &&_Result)
				{
					DMibLock(RemoteLock);
					RemoteActorsSubscription = fg_Move(*_Result);
					RemoteEvent.f_Signal();
					++RemoteEvents;
				}
			;
			
			CDistributedActorConnectionReference ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0).m_ConnectionReference;
			
			bool bConnectionSuccessful = true; // If not we would have had exceptions above
			
			DMibExpectTrue(bConnectionSuccessful);
			
			bool bTimedOutWatingForActor = false;
			while (!bTimedOutWatingForActor)
			{
				{
					DMibLock(RemoteLock);
					if (RemoteEvents >= 2)
						break;
				}
				bTimedOutWatingForActor = RemoteEvent.f_WaitTimeout(60.0);
			}

			{
				DMibLock(RemoteLock);

				DMibAssertFalse(bTimedOutWatingForActor);
				DMibAssert(RemoteEvents, ==, 2);
				DMibAssertTrue(RemoteActor);
				DMibAssertTrue(RemoteActorsSubscription);
			}
			
			DMibCallActor(RemoteActor, CDistributedActorBase::f_AddIntVirtual, 5).f_CallSync(60.0);
			uint32 Result = DMibCallActor(RemoteActor, CDistributedActorBase::f_GetResultVirtual).f_CallSync(60.0);
			DMibExpect(Result, ==, 5);

			CStr RemoteCallingHostID = (DMibCallActor(RemoteActor, CDistributedActorBase::f_GetCallingHostID)).f_CallSync(60.0);
			DMibExpect(RemoteCallingHostID, ==, ClientHostID);
			
			ActorPublication.f_Clear();
			
			bool bTimedOutWatingForUnPublish = false;
			while (!bTimedOutWatingForUnPublish)
			{
				{
					DMibLock(RemoteLock);
					if (RemoteEvents >= 3)
						break;
				}
				bTimedOutWatingForUnPublish = RemoteEvent.f_WaitTimeout(60.0);
			}

			DMibAssertFalse(bTimedOutWatingForUnPublish);
			DMibExpectTrue(bRemoved);
			
			ClientConnectionReference.f_Disconnect().f_CallSync(60.0);
			DMibExpectExceptionType(ClientConnectionReference.f_Disconnect().f_CallSync(60.0), NException::CException);
			
			ListenReference.f_Stop().f_CallSync(60.0);
			DMibExpectExceptionType(ListenReference.f_Stop().f_CallSync(60.0), NException::CException);
		}
		void fp_AnonymousClientTests()
		{
			uint16 Port = 31402;
			
			CDistributedActorTestHelperCombined TestHelper{Port};
			
			TestHelper.f_SeparateServerManager();
			TestHelper.f_InitServer();
			CDistributedActorSecurity Security;
			Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Anonymous/Test");
			Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Anonymous/Test2");
			Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Test");
			TestHelper.f_GetServer().f_SetSecurity(Security);
			TestHelper.f_InitAnonymousClient(TestHelper);
			TestHelper.f_Publish<CDistributedActor, CDistributedActorBase>(fg_ConstructDistributedActor<CDistributedActor>(), "Anonymous/Test");
			TestHelper.f_Publish<CDistributedActor, CDistributedActorBase>(fg_ConstructDistributedActor<CDistributedActor>(), "Test");
			CStr Subscription = TestHelper.f_Subscribe("Anonymous/Test");
			auto Actor = TestHelper.f_GetRemoteActor<CDistributedActor>(Subscription);
			if (!Actor)
				DMibError("Failed to distributed actor environment");
			
			auto HostID = DMibCallActor(Actor, CDistributedActorBase::f_GetCallingHostID).f_CallSync();
			DMibExpectTrue(HostID.f_StartsWith("Anonymous_"));
			
			{
				DMibTestPath("Anonymous cannot access normal publications");
				TestHelper.f_GetClient().f_SubscribeExpectFailure("Test");
			}

			TestHelper.f_GetClient().f_Publish<CDistributedActor, CDistributedActorBase>(fg_ConstructDistributedActor<CDistributedActor>(), "Anonymous/Test2");
			{
				DMibTestPath("Anonymous cannot publish");
				TestHelper.f_GetServer().f_SubscribeExpectFailure("Anonymous/Test2");
			}
		}
		
		void fp_OnDisconnectedTests()
		{
			uint16 Port = 31404;
			CDistributedActorTestHelperCombined TestState(Port);
			TestState.f_SeparateServerManager();
			TestState.f_Init();
			auto LocalActor = fg_ConstructDistributedActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);
			
			CStr HostID = DMibCallActor(Actor, CDistributedActor::f_TestOnDisconnect).f_CallSync(60.0);
			
			DMibExpect(HostID, != , "");
			bool bHasClient = DMibCallActor(Actor, CDistributedActor::f_HasOnDisconnect, HostID).f_CallSync(60.0);
			DMibExpectTrue(bHasClient);
			TestState.f_DisconnectClient(true);
			TestState.f_InitClient(TestState);
			SubscriptionID = TestState.f_Subscribe("Test");
			Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);
			bHasClient = LocalActor(&CDistributedActor::f_HasOnDisconnect, HostID).f_CallSync(60.0);
			DMibExpectFalse(bHasClient);
		}
		
	public:
		void f_FunctionalTests()
		{
			DMibTestCategory("Local")
			{
				fp_RunTests
					(
						[]
						{
							return fg_ConstructDistributedActor<CDistributedActor>();
						}
					)
				;
			};
			DMibTestCategory("Remote")
			{
				DMibTestSuite("Basics")
				{
					fp_BasicTests("localhost");
				};
#ifndef DPlatformFamily_Windows
				DMibTestSuite("Basics Unix Sockets")
				{
					fp_BasicTests(fg_Format("UNIX:{}/TestDistributedActor.socket", NMib::NFile::CFile::fs_GetProgramDirectory()));
				};
#endif
				DMibTestSuite("Anonymous client")
				{
					fp_AnonymousClientTests();
				};

				DMibTestSuite("On disconnected")
				{
					fp_OnDisconnectedTests();
				};
				
				TCSharedPointer<CDistributedActorTestHelperCombined> pTestState;
				CStr SubscriptionID;
				
				fp_RunTests
					(
						[&]
						{
							if (!pTestState)
							{
								uint16 Port = 31403;
								
								pTestState = fg_Construct(Port);
								
								pTestState->f_SeparateServerManager();
								pTestState->f_Init();
								pTestState->f_Publish<CDistributedActor, CDistributedActorBase>(fg_ConstructDistributedActor<CDistributedActor>(), "Test");
								SubscriptionID = pTestState->f_Subscribe("Test");
							}
							
							auto Actor = pTestState->f_GetRemoteActor<CDistributedActor>(SubscriptionID);
							
							if (!Actor)
								DMibError("Failed to distributed actor environment");
							
							return Actor;
						}
					)
				;
			};
		}

		void f_DoTests()
		{
			f_FunctionalTests();
		}
	};

	DMibTestRegister(CDistributedActor_Tests, Malterlib::Concurrency);
}
