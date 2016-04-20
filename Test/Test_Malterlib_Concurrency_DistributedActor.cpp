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
#include <Mib/Web/WebSocket>
#include <Mib/Log/Destinations>

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
		virtual void f_AddIntVirtual(uint32 _Value) pure;
		virtual uint32 f_GetResultVirtual() pure;
	};

	class CDistributedActor : public CDistributedActorBase
	{
		zuint32 m_Value;
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
		
		void fp_BasicTests()
		{
			TCActor<CActorDistributionManager> ServerManager = fg_ConstructActor<CActorDistributionManager>();
			TCActor<CActorDistributionManager> ClientManager = fg_ConstructActor<CActorDistributionManager>();
			
			CActorDistributionCryptographySettings ServerCryptography;
			ServerCryptography.f_GenerateNewCert(fg_CreateVector<NStr::CStr>("localhost"), 1024);
			
			CActorDistributionListenSettings ListenSettings{1392}; // 1392 is 'mib' encoded with alphabet positions
			ListenSettings.f_SetCryptography(ServerCryptography);
			ListenSettings.m_bRetryOnListenFailure = false;
			ServerManager(&CActorDistributionManager::f_Listen, ListenSettings).f_CallSync(60.0);

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
			ConnectionSettings.m_ServerURL = "wss://localhost:1392/";
			ConnectionSettings.m_PublicServerCertificate = ListenSettings.m_PublicCertificate;
			CActorDistributionCryptographySettings ClientCryptography;
			ClientCryptography.f_GenerateNewCert(fg_CreateVector<NStr::CStr>("localhost"), 1024);
			auto CertificateRequest = ClientCryptography.f_GenerateRequest();
			
			auto SignedRequest = ServerCryptography.f_SignRequest(CertificateRequest);
			
			ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, ServerCryptography.m_PublicCertificate, SignedRequest);
			
			ConnectionSettings.f_SetCryptography(ClientCryptography);
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			NThread::CMutual RemoteLock;
			NThread::CEventAutoReset RemoteEvent;
			TCDistributedActor<CDistributedActorBase> RemoteActor;
			CActorCallback RemoteActorsSubscription;
			mint RemoteEvents = 0;
			bool bRemoved = false;
			
			auto &ConcurrentActor = fg_ConcurrentActor();
			
			ClientManager
				(
					&CActorDistributionManager::f_SubscribeActors
					, fg_CreateVector<NStr::CStr>("Test")
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
				> ConcurrentActor / [&](TCAsyncResult<CActorCallback> &&_Result)
				{
					DMibLock(RemoteLock);
					RemoteActorsSubscription = fg_Move(*_Result);
					RemoteEvent.f_Signal();
					++RemoteEvents;
				}
			;
			
			ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0);
			
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
					fp_BasicTests();
				};
				
				TCSharedPointer<CDistributedActorTestHelper> pTestState;
				
				fp_RunTests
					(
						[&]
						{
							if (!pTestState)
							{
								pTestState = fg_Construct();
								
								pTestState->f_SeparateServerManager();
								pTestState->f_Init();
								pTestState->f_Publish<CDistributedActor, CDistributedActorBase>(fg_ConstructDistributedActor<CDistributedActor>(), "Test");
								pTestState->f_Subscribe("Test");
							}
							
							auto Actor = pTestState->f_GetRemoteActor<CDistributedActor>();
							
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
