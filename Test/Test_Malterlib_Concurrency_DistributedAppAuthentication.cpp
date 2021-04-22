// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/String/Algorithm>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTrustManagerProxy>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Concurrency/DistributedAppTestHelpers>
#include <Mib/Concurrency/DistributedTrustTestHelpers>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Process/StdInActor>
#include <Mib/Test/Exception>

#ifdef DPlatformFamily_Windows
#include <Windows.h>
#endif

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NFile;
using namespace NMib::NStr;
using namespace NMib::NProcess;
using namespace NMib::NContainer;
using namespace NMib::NCryptography;
using namespace NMib::NStorage;
using namespace NMib::NAtomic;
using namespace NMib::NEncoding;
using namespace NMib::NStorage;
using namespace NMib::NNetwork;
using namespace NMib::NStr;
using namespace NMib::NContainer;
using namespace NMib::NTest;

#define DTestDistributredAppAuthenticationEnableLogging 0

static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;

namespace NTestAuthentication
{
	struct CPermissionQueryLocal : public CPermissionQuery
	{
		CPermissionQueryLocal(CPermissionQuery &&_Other)
			: CPermissionQuery(fg_Move(_Other))
		{
		}

		CPermissionQueryLocal(CPermissionQuery const &_Other)
			: CPermissionQuery(_Other)
		{
		}

		CPermissionQueryLocal() = default;
		CPermissionQueryLocal(CPermissionQueryLocal &&) = default;
		CPermissionQueryLocal(CPermissionQueryLocal const &) = default;

		CPermissionQueryLocal &operator = (CPermissionQueryLocal &&) = default;
		CPermissionQueryLocal &operator = (CPermissionQueryLocal const &) = default;

		CEJSON f_ToJSON() const
		{
			return
				{
					"Permissions"_= m_Permissions
					, "Description"_= m_Description
					, "Wildcard"_= m_bIsWildcard
				}
			;
		}

		static CPermissionQueryLocal fs_FromJSON(CEJSON const &_JSON)
		{
			CPermissionQueryLocal Return;
			for (auto &Permission : _JSON["Permissions"].f_Array())
				Return.m_Permissions.f_Insert(Permission.f_String());
			Return.m_Description = _JSON["Description"].f_String();
			Return.m_bIsWildcard = _JSON["Wildcard"].f_Boolean();

			return Return;
		}

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream)
		{
			_Stream % m_Permissions;
			_Stream % m_Description;
			_Stream % m_bIsWildcard;
		}
	};

	struct CServerInterface : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppAuthenticationTestsServer";

		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CServerInterface()
		{
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions1);
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions1LifeTime);
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions2);
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions3);
		}

		virtual NConcurrency::TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCFuture<bool> f_CheckPermissions1LifeTime(TCVector<CStr> const &_Permissions, int64 _AuthenticationLifetime) = 0;
		virtual NConcurrency::TCFuture<bool> f_CheckPermissions2(TCVector<CPermissionQueryLocal> const &_Permission) = 0;
		virtual NConcurrency::TCFuture<TCMap<CStr, bool>> f_CheckPermissions3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permission) = 0;
	};

	struct CServerActor : public CDistributedAppActor
	{
		CServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings("DistributedAppAuthenticationTestsServer")
			 	.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/Server")
			 	.f_SeparateDistributionManager(true)
			 	.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			)
		{
		}

		struct CServer : public CActor
		{
		public:
			using CActorHolder = CDelegatedActorHolder;

			CServer(CDistributedAppState &_AppState)
				: mp_AppState(_AppState)
			{
				mp_ProtocolInterface.f_Publish<CServerInterface>(mp_AppState.m_DistributionManager, this, CServerInterface::mc_pDefaultNamespace) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibConErrOut("Failed to publish server interface: {}\n", _Result.f_GetExceptionStr());
					}
				;
			}

			struct CServerInterfaceImplementation : public CServerInterface
			{
				TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<bool> f_CheckPermissions1LifeTime(TCVector<CStr> const &_Permissions, int64 _AuthenticationLifetime) override
				{
					TCPromise<bool> Promise;
					auto const &Info = fg_GetCallingHostInfo();
					auto CallingHostInfo =  CCallingHostInfo
						(
							Info.f_GetDistributionManager()
							, Info.f_GetAuthenticationHandler()
							, Info.f_GetUniqueHostID()
							, Info.f_GetHostInfo()
							, Info.f_LastExecutionID()
							, Info.f_GetProtocolVersion()
							, Info.f_GetClaimedUserID()
							, Info.f_GetClaimedUserName()
							, Info.f_GetHost()
						)
					;

					m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions, CallingHostInfo) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<bool> f_CheckPermissions2(TCVector<CPermissionQueryLocal> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions.f_HasPermissions("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<TCMap<CStr, bool>> f_CheckPermissions3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permissions) override
				{
					TCPromise<TCMap<CStr, bool>> Promise;

					m_pThis->mp_Permissions.f_HasPermissions("Test", _Permissions) > Promise / [Promise](NContainer::TCMap<NStr::CStr, bool> _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				CServer *m_pThis;
#				ifdef DMibDebug
					CEmpty self; // Hide dangerous self
#				endif
			};

			TCFuture<void> f_SubscribePermissions()
			{
				TCPromise<void> Promise;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.malterlib/*")
						 , fg_ThisActor(this)
					)
					> Promise / [this, Promise](CTrustedPermissionSubscription &&_TrustedSubscription)
					{
						mp_Permissions = fg_Move(_TrustedSubscription);
						Promise.f_SetResult();
					}
				;
				return Promise.f_MoveFuture();
			}

		private:
			TCFuture<void> fp_Destroy() override
			{
				return mp_ProtocolInterface.m_Publication.f_Destroy();
			}

			TCDistributedActorInstance<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions;
		};

	private:
		TCFuture<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CServerActor::CServer>(fg_Construct(self), mp_State);
			co_await mp_ServerActor(&CServer::f_SubscribePermissions);
			co_return {};
		}

		TCFuture<void> fp_StopApp() override
		{
			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CServerActor, Info, "Shutting down server");

				auto Result = co_await mp_ServerActor.f_Destroy().f_Wrap();
				if (!Result)
					DMibLogWithCategory(Mib/Concurrency/CServerActor, Error, "Failed to shut down server: {}", Result.f_GetExceptionStr());
			}

			co_return {};
		}

		TCActor<CServerActor::CServer> mp_ServerActor;
	};

	TCActor<CDistributedAppActor> fg_ConstructApp_Server()
	{
		return fg_Construct<CServerActor>();
	}

	struct CManyServerInterface : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppAuthenticationTestsManyServer";

		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CManyServerInterface()
		{
			DMibPublishActorFunction(CManyServerInterface::f_CheckPermissions1);
			DMibPublishActorFunction(CManyServerInterface::f_CheckPermissions2);
			DMibPublishActorFunction(CManyServerInterface::f_CheckPermissions3);
			DMibPublishActorFunction(CManyServerInterface::f_CheckPermissions4);
		}

		virtual NConcurrency::TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCFuture<bool> f_CheckPermissions2(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCFuture<bool> f_CheckPermissions3(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCFuture<bool> f_CheckPermissions4(TCVector<CStr> const &_Permissions) = 0;
	};

	// This server is used to test the behavior multiple multiple permission subscriptions
	struct CManyServerActor : public CDistributedAppActor
	{
		CManyServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings("DistributedAppAuthenticationTestsManyServer")
			 	.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/ManyServer")
			 	.f_SeparateDistributionManager(true)
			 	.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			)
		{
		}

		struct CServer : public CActor
		{
		public:
			using CActorHolder = CDelegatedActorHolder;

			CServer(CDistributedAppState &_AppState)
				: mp_AppState(_AppState)
			{
				mp_ProtocolInterface.f_Publish<CManyServerInterface>(mp_AppState.m_DistributionManager, this, CManyServerInterface::mc_pDefaultNamespace) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibConErrOut("Failed to publish server interface: {}\n", _Result.f_GetExceptionStr());
					}
				;
			}

			struct CServerInterfaceImplementation : public CManyServerInterface
			{
				TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions1.f_HasPermission("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<bool> f_CheckPermissions2(TCVector<CStr> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions2.f_HasPermission("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<bool> f_CheckPermissions3(TCVector<CStr> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions3.f_HasPermission("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				TCFuture<bool> f_CheckPermissions4(TCVector<CStr> const &_Permissions) override
				{
					TCPromise<bool> Promise;

					m_pThis->mp_Permissions4.f_HasPermission("Test", _Permissions) > Promise / [Promise](bool _Result)
						{
							Promise.f_SetResult(_Result);
						}
					;
					return Promise.f_MoveFuture();
				}

				CServer *m_pThis;
#				ifdef DMibDebug
					CEmpty self; // Hide dangerous self
#				endif
			};

			TCFuture<void> f_SubscribePermissions()
			{
				TCPromise<void> Promise;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					)
					+ mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					)
					> Promise / [this, Promise](CTrustedPermissionSubscription &&_TrustedSubscription1, CTrustedPermissionSubscription &&_TrustedSubscription2)
					{
						mp_Permissions1 = fg_Move(_TrustedSubscription1);
						mp_Permissions2 = fg_Move(_TrustedSubscription2);
						Promise.f_SetResult();
					}
				;
				return Promise.f_MoveFuture();
			}

			TCFuture<void> f_SubscribePermissions3()
			{
				TCPromise<void> Promise;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					)
					> Promise / [this, Promise](CTrustedPermissionSubscription &&_TrustedSubscription3)
					{
						mp_Permissions3 = fg_Move(_TrustedSubscription3);
						Promise.f_SetResult();
					}
				;
				return Promise.f_MoveFuture();
			}

			TCFuture<void> f_SubscribePermissions4()
			{
				TCPromise<void> Promise;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					)
					> Promise / [this, Promise](CTrustedPermissionSubscription &&_TrustedSubscription4)
					{
						mp_Permissions4 = fg_Move(_TrustedSubscription4);
						Promise.f_SetResult();
					}
				;
				return Promise.f_MoveFuture();
			}

		private:
			TCFuture<void> fp_Destroy() override
			{
				return mp_ProtocolInterface.m_Publication.f_Destroy();
			}

			TCDistributedActorInstance<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions1;
			CTrustedPermissionSubscription mp_Permissions2;
			CTrustedPermissionSubscription mp_Permissions3;
			CTrustedPermissionSubscription mp_Permissions4;
		};

	private:
		TCFuture<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override
		{
			if (_Command == "SubscribePermissions3")
			{
				co_await mp_ServerActor(&CServer::f_SubscribePermissions3);
				co_return {};
			}
			if (_Command == "SubscribePermissions4")
			{
				co_await mp_ServerActor(&CServer::f_SubscribePermissions4);
				co_return {};
			}
			co_return DMibErrorInstance("Unhandled command in fp_Test_Command: {}"_f << _Command);
		}

		TCFuture<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CManyServerActor::CServer>(fg_Construct(self), mp_State);
			co_await mp_ServerActor(&CServer::f_SubscribePermissions);

			co_return {};
		}

		TCFuture<void> fp_StopApp() override
		{
			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CManyServerActor, Info, "Shutting down server");

				auto Result = co_await mp_ServerActor.f_Destroy().f_Wrap();
				if (!Result)
					DMibLogWithCategory(Mib/Concurrency/CManyServerActor, Error, "Failed to shut down server: {}", Result.f_GetExceptionStr());
			}
			co_return {};
		}

		TCActor<CManyServerActor::CServer> mp_ServerActor;
	};

	TCActor<CDistributedAppActor> fg_ConstructApp_ManyServer()
	{
		return fg_Construct<CManyServerActor>();
	}

	struct CSlowServerInterface : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppAuthenticationTestsSlowServer";

		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CSlowServerInterface()
		{
			DMibPublishActorFunction(CSlowServerInterface::f_CheckPermissions1);
		}

		virtual NConcurrency::TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
	};

	struct CSlowServerActor : public CDistributedAppActor
	{
		CSlowServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings("DistributedAppAuthenticationTestsSlowServer")
			 	.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/SlowServer")
			 	.f_SeparateDistributionManager(true)
			 	.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			)
		{
		}

		struct CServer : public CActor
		{
		public:
			using CActorHolder = CDelegatedActorHolder;

			CServer(CDistributedAppState &_AppState)
				: mp_AppState(_AppState)
			{
				mp_ProtocolInterface.f_Publish<CSlowServerInterface>(mp_AppState.m_DistributionManager, this, CSlowServerInterface::mc_pDefaultNamespace) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibConErrOut("Failed to publish server interface: {}\n", _Result.f_GetExceptionStr());
					}
				;
			}

			struct CServerInterfaceImplementation : public CSlowServerInterface
			{
				TCFuture<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					co_return co_await m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions);
				}

				CServer *m_pThis;
#				ifdef DMibDebug
					CEmpty self; // Hide dangerous self
#				endif
			};

			TCFuture<void> f_SubscribePermissions()
			{
				TCPromise<void> Promise;

				mp_Permissions = co_await mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.slowmalterlib/*")
						 , fg_ThisActor(this)
					)
				;

				co_return {};
			}

		private:
			TCFuture<void> fp_Destroy() override
			{
				return mp_ProtocolInterface.m_Publication.f_Destroy();
			}

			TCDistributedActorInstance<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions;
		};

	private:
		TCFuture<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CSlowServerActor::CServer>(fg_Construct(self), mp_State);
			co_await mp_ServerActor(&CServer::f_SubscribePermissions);

			co_return {};
		}

		TCFuture<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CSlowServerActor, Info, "Shutting down server");

				auto Result = co_await mp_ServerActor.f_Destroy().f_Wrap();
				if (!Result)
					DMibLogWithCategory(Mib/Concurrency/CSlowServerActor, Error, "Failed to shut down server: {}", Result.f_GetExceptionStr());
			}

			co_return {};
		}

		TCActor<CSlowServerActor::CServer> mp_ServerActor;
	};

	TCActor<CDistributedAppActor> fg_ConstructApp_SlowServer()
	{
		return fg_Construct<CSlowServerActor>();
	}

	struct CClientActor : public CDistributedAppActor
	{
		CClientActor(CStr _DefaultUserID)
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings("DistributedAppAuthenticationTestsClient")
			 	.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/Client")
			 	.f_SeparateDistributionManager(true)
			 	.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			 	.f_SupportUserAuthentication(false)
			)
			, m_DefaultUserID(_DefaultUserID)
		{
		}

		TCFuture<CActorSubscription> fp_SetupAuthentication
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, CStr const &_UserID
			) override
		{
			return fp_EnableAuthentication(_pCommandLine, _AuthenticationLifetime, _UserID);
		}

		TCFuture<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override
		{
			TCPromise<NEncoding::CEJSON> Promise;
			if (_Command == "CommandLineHostID")
				co_return mp_State.m_CommandLineHostID;

			co_return DMibErrorInstance("Unhandled command in fp_Test_Command: {}"_f << _Command);
		}

	private:
		TCFuture<void> fp_StartApp(CEJSON const &_Params) override
		{
			// Have to create the user and set it as default user before registering the authentication handler.
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddUser, m_DefaultUserID, "Default User");
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, m_DefaultUserID);
			auto [Subscription, ManySubscription, SlowSubscription] = co_await
				(
					mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_SubscribeTrustedActors<CServerInterface>
						, CServerInterface::mc_pDefaultNamespace
						, fg_ThisActor(this)
					)
					+ mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_SubscribeTrustedActors<CManyServerInterface>
						, CManyServerInterface::mc_pDefaultNamespace
						, fg_ThisActor(this)
					)
					+ mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_SubscribeTrustedActors<CSlowServerInterface>
						, CSlowServerInterface::mc_pDefaultNamespace
						, fg_ThisActor(this)
					)
				)
			;

			mp_TestServers = fg_Move(Subscription);
			mp_ManyTestServers = fg_Move(ManySubscription);
			mp_SlowTestServers = fg_Move(SlowSubscription);

			co_return {};
		}

		TCVector<CStr> fs_GetCommandPermissions(CEJSON const &_Permissions)
		{
			TCVector<CStr> Return;
			for (auto &Permission : _Permissions.f_Array())
				Return.f_Insert(Permission.f_String());
			return Return;
		}

		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override
		{
			CDistributedAppActor::fp_BuildCommandLine(o_CommandLine);
			auto Section = o_CommandLine.f_AddSection("Test1", "Testing test 2");

			Section.f_RegisterCommand
				(
					{
						"Names"_= {"--test-actor"}
						, "Description"_= "Test 3."

					}
					, [](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						co_return 0;
					}
				)
			;

			auto fSharedParameters = [](auto _Type)
				{
					return "Parameters"_=
						{
							"Permissions"_=
							{
								"Description"_= "."
								, "Type"_= _Type
							}
						}
					;
				}
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pTestServer->m_Actor.f_CallActor(&CServerInterface::f_CheckPermissions1)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-2"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						TCVector<CPermissionQueryLocal> Permissions;
						for (auto &QueryJSON : _Params["Permissions"].f_Array())
							Permissions.f_Insert(CPermissionQueryLocal::fs_FromJSON(QueryJSON));

						bool bSuccess = co_await pTestServer->m_Actor.f_CallActor(&CServerInterface::f_CheckPermissions2)(Permissions);

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-3"}, "Description"_= ".", fSharedParameters(EJSONType_Object)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						TCMap<CStr, TCVector<CPermissionQueryLocal>> Permissions;
						for (auto &Queries : _Params["Permissions"].f_Object())
						{
							auto &OutputPermissions = Permissions[Queries.f_Name()];
							for (auto &QueryJSON : Queries.f_Value().f_Array())
								OutputPermissions.f_Insert(CPermissionQueryLocal::fs_FromJSON(QueryJSON));
						}

						auto Results = co_await pTestServer->m_Actor.f_CallActor(&CServerInterface::f_CheckPermissions3)(Permissions);

						CEJSON Output;
						for (auto &Result : Results)
							Output[Results.fs_GetKey(Result)] = Result;

						*_pCommandLine += Output.f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pManyTestServer->m_Actor.f_CallActor(&CManyServerInterface::f_CheckPermissions1)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-2"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pManyTestServer->m_Actor.f_CallActor(&CManyServerInterface::f_CheckPermissions2)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-3"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pManyTestServer->m_Actor.f_CallActor(&CManyServerInterface::f_CheckPermissions3)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-4"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pManyTestServer->m_Actor.f_CallActor(&CManyServerInterface::f_CheckPermissions4)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-slow-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						CStr Error;
						auto pSlowTestServer = mp_SlowTestServers.f_GetOneActor("", Error);
						if (!pSlowTestServer)
							co_return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						bool bSuccess = co_await pSlowTestServer->m_Actor.f_CallActor(&CSlowServerInterface::f_CheckPermissions1)(fs_GetCommandPermissions(_Params["Permissions"]));

						*_pCommandLine += CEJSON(bSuccess).f_ToString("\t", EJSONDialectFlag_AllowUndefined);

						co_return 0;
					}
				)
			;
		}

		TCFuture<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			return pCanDestroy->f_Future();
		}

		TCTrustedActorSubscription<CServerInterface> mp_TestServers;
		TCTrustedActorSubscription<CManyServerInterface> mp_ManyTestServers;
		TCTrustedActorSubscription<CSlowServerInterface> mp_SlowTestServers;

		CStr m_DefaultUserID;
	};

	NFunction::TCFunction<TCActor<CDistributedAppActor> ()> fg_ConstructApp_ClientFactory(CStr _DefaultUserID)
	{
		return [_DefaultUserID]
			{
				return fg_Construct<CClientActor>(_DefaultUserID);
			}
		;
	}

	struct CCommandLineControlActorTest : public ICCommandLineControl
	{
		CCommandLineControlActorTest(bool _bEchoStdErr)
			: m_bEchoStdErr(_bEchoStdErr)
		{
		}

		TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<NContainer::CSecureByteVector> f_ReadBinary() override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<NStr::CStrSecure> f_ReadLine() override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) override
		{
			co_return m_ReturnValues.f_PopBack();
		}

		TCFuture<void> f_AbortReads() override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<void> f_StdOut(NStr::CStrSecure const &_Output) override
		{
			m_StdOut += _Output;
			co_return {};
		}

		TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) override
		{
			DMibNeverGetHere;
			co_return {};
		}

		TCFuture<void> f_StdErr(NStr::CStrSecure const &_Output) override
		{
			if (m_bEchoStdErr)
				DMibConOut("{}", _Output);
			else
				m_StdErr += _Output;

			co_return {};
		}

		TCFuture<void> f_ReturnString(NStr::CStrSecure const &_String)
		{
			m_ReturnValues.f_InsertLast(_String);
			co_return {};
		}

		CStrSecure f_ReturnStdOut()
		{
			return fg_Move(m_StdOut);
		}

		CStrSecure f_ReturnStdErr()
		{
			return fg_Move(m_StdErr);
		}

		void f_Clear()
		{
			m_StdErr.f_Clear();
			m_StdOut.f_Clear();
		}

	private:
		TCFuture<void> fp_Destroy() override
		{
			co_return {};
		}

		TCVector<NStr::CStrSecure> m_ReturnValues;
		NStr::CStrSecure m_StdOut;
		NStr::CStrSecure m_StdErr;
		bool m_bEchoStdErr;
	};

	struct CCallCount : public CActor
	{
		void f_AddCall(CStr _Name)
		{
			++m_CallCounts[_Name];
		}

		void f_Clear()
		{
			m_CallCounts.f_Clear();
		}

		TCMap<CStr, aint> f_GetCounts()
		{
			return fg_Move(m_CallCounts);
		}

		TCMap<CStr, aint> m_CallCounts;
	};

	TCActor<CCallCount> g_CallCount;

	struct CAuthenticationActorTestSucceed : public ICDistributedActorTrustManagerAuthenticationActor
	{
		CAuthenticationActorTestSucceed
			(
			 	TCWeakActor<CDistributedActorTrustManager> const &_TrustManager
			 	, CStr const &_Name
			 	, EAuthenticationFactorCategory _Category
			)
			: m_TrustManager(_TrustManager)
			, m_Name(_Name)
			, m_Category(_Category)
		{
		}
		virtual ~CAuthenticationActorTestSucceed() = default;

		TCFuture<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = m_Category;
			Result.m_Name = m_Name;

			co_return fg_Move(Result);
		}

		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
				, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		{
			ICDistributedActorAuthenticationHandler::CResponse Response;
			for (auto const &RegisteredFactor : _Factors)
			{
				Response.m_SignedProperties = _SignedProperties;
				Response.m_Signature.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				break;
			}
			co_await g_CallCount(&CCallCount::f_AddCall, m_Name);

			co_return fg_Move(Response);
		};

		TCFuture<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
				ICDistributedActorAuthenticationHandler::CResponse const &_Response
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
				, CAuthenticationData const &_AuthenticationData
			) override
		{
			co_return CVerifyAuthenticationReturn{bool(_Response.m_Signature == CByteVector((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen()))};
		}

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
		CStr const m_HostID;
		CStr const m_Name;
		EAuthenticationFactorCategory const m_Category;
	};

	struct CAuthenticationActorFail : public ICDistributedActorTrustManagerAuthenticationActor
	{
		CAuthenticationActorFail(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager, CStr const &_Name)
			: m_TrustManager(_TrustManager)
			, m_Name(_Name)
		{
		}

		virtual ~CAuthenticationActorFail() = default;

		TCFuture<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = EAuthenticationFactorCategory_None;
			Result.m_Name = "Fail1";

			co_return fg_Move(Result);
		}

		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
				, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		{
			ICDistributedActorAuthenticationHandler::CResponse Response;
			for (auto const &RegisteredFactor : _Factors)
			{
				Response.m_SignedProperties = _SignedProperties;
				Response.m_Signature.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				break;
			}
			co_await g_CallCount(&CCallCount::f_AddCall, m_Name);

			co_return fg_Move(Response);
		};

		TCFuture<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
				ICDistributedActorAuthenticationHandler::CResponse const &_Response
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
				, CAuthenticationData const &_AuthenticationData
			) override
		{
			co_return CVerifyAuthenticationReturn{false};
		}

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
		CStr const m_HostID;
		CStr m_Name;
	};

	class CDistributedActorTrustManagerAuthenticationActorFactorySucceed1 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			EAuthenticationFactorCategory const None = EAuthenticationFactorCategory_None;
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorTestSucceed>(_TrustManager, "Succeed1", None), None};
		}
	};

	class CDistributedActorTrustManagerAuthenticationActorFactorySucceed2 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			EAuthenticationFactorCategory const Possession = EAuthenticationFactorCategory_Possession;
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorTestSucceed>(_TrustManager, "Succeed2", Possession), Possession};
		}
	};

	class CDistributedActorTrustManagerAuthenticationActorFactorySucceed3 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			EAuthenticationFactorCategory const Inherence = EAuthenticationFactorCategory_Inherence;
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorTestSucceed>(_TrustManager, "Succeed3", Inherence), Inherence};
		}
	};

	class CDistributedActorTrustManagerAuthenticationActorFactorySucceed4 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			EAuthenticationFactorCategory const Knowledge = EAuthenticationFactorCategory_Knowledge;
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorTestSucceed>(_TrustManager, "Succeed4", Knowledge), Knowledge};
		}
	};

	class CDistributedActorTrustManagerAuthenticationActorFactoryFail1 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorFail>(_TrustManager, "Fail1"), EAuthenticationFactorCategory_None};
		}
	};

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySucceed1);
	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySucceed2);
	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySucceed3);
	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySucceed4);
	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryFail1);
};

using namespace NTestAuthentication;

class CDistributedAppAuthentication_Tests : public NMib::NTest::CTest
{
public:
	void f_DoTests()
	{
		DMibTestSuite("General")
		{
			fp_DoGeneralTests();
		};
		DMibTestSuite(CTestCategory("U2F") << NMib::NTest::CTestGroup("Manual"))
		{
			fp_DoU2FTests();
		};
	}

	void fp_DoGeneralTests()
	{
#ifdef DPlatformFamily_Windows
		AllocConsole();
		SetConsoleCtrlHandler
			(
				nullptr
				, true
			)
		;
#endif

#if DTestDistributredAppAuthenticationEnableLogging
		fg_GetSys()->f_AddStdErrLogger();
#endif

		TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
		auto CleanupRunLoop = g_OnScopeExit > [&]
			{
				while (pRunLoop->f_RefCountGet() > 0)
					pRunLoop->f_WaitOnceTimeout(0.1);
			}
		;
		TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
		auto CleanupHelperActor = g_OnScopeExit > [&]
			{
				HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
		;
		CCurrentlyProcessingActorScope CurrentActor{HelperActor};

		CStr ProgramDirectory = CFile::fs_GetProgramDirectory();
		CStr RootDirectory = ProgramDirectory + "/DistributedAppAuthenticationTests";

		CProcessLaunch::fs_KillProcessesInDirectory("*", {}, RootDirectory, g_Timeout);

		for (mint i = 0; i < 5; ++i)
		{
			try
			{
				if (CFile::fs_FileExists(RootDirectory))
					CFile::fs_DeleteDirectoryRecursive(RootDirectory);
				break;
			}
			catch (NFile::CExceptionFile const &)
			{
			}
		}

		CFile::fs_CreateDirectory(RootDirectory);

		CTrustManagerTestHelper TrustManagerState;
		TCActor<CDistributedActorTrustManager> TrustManager = TrustManagerState.f_TrustManager("TestHelper", {}, false);
		auto CleanupTrustManager = g_OnScopeExit > [&]
			{
				TrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
		;

		CStr TestHostID = TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
		CTrustedSubscriptionTestHelper Subscriptions{TrustManager};

		CDistributedActorTrustManager_Address TestAddress;
		TestAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/controller.sock"_f << RootDirectory);
		TrustManager(&CDistributedActorTrustManager::f_AddListen, TestAddress).f_CallSync(pRunLoop, g_Timeout);

		CDistributedApp_LaunchHelperDependencies Dependencies;
		Dependencies.m_Address = TestAddress.m_URL;
		Dependencies.m_TrustManager = TrustManager;
		Dependencies.m_DistributionManager = TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(pRunLoop, g_Timeout);

		NMib::NConcurrency::CDistributedActorSecurity Security;
		static constexpr ch8 const *c_pDefaultNamespace = "com.malterlib/Concurrency/AuthenticationTest";
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(c_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(ICDistributedActorAuthentication::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CServerInterface::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CManyServerInterface::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CSlowServerInterface::mc_pDefaultNamespace);
		Dependencies.m_DistributionManager(&CActorDistributionManager::f_SetSecurity, Security).f_CallSync(pRunLoop, g_Timeout);

		TCActor<CDistributedApp_LaunchHelper> LaunchHelper = fg_ConstructActor<CDistributedApp_LaunchHelper>(Dependencies, DTestDistributredAppAuthenticationEnableLogging);
		g_CallCount = fg_ConstructActor<CCallCount>();

		auto Cleanup = g_OnScopeExit > [&]
			{
				LaunchHelper->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				g_CallCount.f_Clear();
			}
		;

		static auto constexpr c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
		// Launch Server side (who needs authentication for commands)
		CStr ServerName = "Server";
		CStr ServerDirectory = RootDirectory + "/" + ServerName;
		CFile::fs_CreateDirectory(ServerDirectory);
		auto ServerLaunch = LaunchHelper
			(
				&CDistributedApp_LaunchHelper::f_LaunchInProcess
				, ServerName
				, ServerDirectory
				, &fg_ConstructApp_Server
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pServerTrust = ServerLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> ServerTrust = *pServerTrust;
		CStr ServerHostID = ServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ServerAddress;
		ServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsServer.sock"_f << RootDirectory);
		ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(ServerAddress).f_CallSync(pRunLoop, g_Timeout);

		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_AllowHostsForNamespace)
			(
				CServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(pRunLoop, g_Timeout)
		;

		auto Server = Subscriptions.f_Subscribe<CServerInterface>();

		CStr ManyServerName = "ManyServer";
		CStr ManyServerDirectory = RootDirectory + "/" + ManyServerName;
		CFile::fs_CreateDirectory(ManyServerDirectory);
		auto ManyServerLaunch = LaunchHelper
			(
			 	&CDistributedApp_LaunchHelper::f_LaunchInProcess
			 	, ManyServerName
			 	, ManyServerDirectory
			 	, &fg_ConstructApp_ManyServer
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pManyServerTrust = ManyServerLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> ManyServerTrust = *pManyServerTrust;
		CStr ManyServerHostID = ManyServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ManyServerAddress;
		ManyServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsManyServer.sock"_f << RootDirectory);
		ManyServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(ManyServerAddress).f_CallSync(pRunLoop, g_Timeout);

		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_AllowHostsForNamespace)
			(
				CManyServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ManyServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(pRunLoop, g_Timeout)
		;

		auto ManyServer = Subscriptions.f_Subscribe<CManyServerInterface>();

		CStr SlowServerName = "SlowServer";
		CStr SlowServerDirectory = RootDirectory + "/" + SlowServerName;
		CFile::fs_CreateDirectory(SlowServerDirectory);
		auto SlowServerLaunch = LaunchHelper
			(
				&CDistributedApp_LaunchHelper::f_LaunchInProcess
				, SlowServerName
				, SlowServerDirectory
				, &fg_ConstructApp_SlowServer
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pSlowServerTrust = SlowServerLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> SlowServerTrust = *pSlowServerTrust;
		CStr SlowServerHostID = SlowServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address SlowServerAddress;
		SlowServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsSlowServer.sock"_f << RootDirectory);
		SlowServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(SlowServerAddress).f_CallSync(pRunLoop, g_Timeout);

		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_AllowHostsForNamespace)
			(
				CSlowServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(SlowServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(pRunLoop, g_Timeout)
		;

		auto SlowServer = Subscriptions.f_Subscribe<CSlowServerInterface>();

		auto DefaultUserID = fg_RandomID();

		// Launch Client side (who provides authentication)
		CStr ClientName = "Client";
		CStr ClientDirectory = RootDirectory + "/" + ClientName;
		CFile::fs_CreateDirectory(ClientDirectory);
		auto ClientLaunch = LaunchHelper
			(
				&CDistributedApp_LaunchHelper::f_LaunchInProcess
				, ClientName
				, ClientDirectory
				, fg_ConstructApp_ClientFactory(DefaultUserID)
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pClientTrust = ClientLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> ClientTrust = *pClientTrust;
		CStr ClientHostID = ClientLaunch.m_HostID;

		CDistributedActorTrustManager_Address ClientAddress;
		ClientAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsClient.sock"_f << RootDirectory);
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(ClientAddress).f_CallSync(pRunLoop, g_Timeout);

		auto fNamespaceHosts = [](auto &&_Namespace, auto &&_Hosts)
			{
				return CDistributedActorTrustManagerInterface::CChangeNamespaceHosts{_Namespace, _Hosts, c_WaitForSubscriptions};
			}
		;

		for (auto const &HostID : {ServerHostID, ManyServerHostID, SlowServerHostID})
		{
			ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
				(
					fNamespaceHosts(ICDistributedActorAuthentication::mc_pDefaultNamespace, fg_CreateSet<CStr>(HostID))
				).f_CallSync(pRunLoop, g_Timeout)
			;
		}
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
			(
				fNamespaceHosts(CServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(pRunLoop, g_Timeout)
		;
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
			(
				fNamespaceHosts(CManyServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ManyServerHostID))
			).f_CallSync(pRunLoop, g_Timeout)
		;
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
			(
				fNamespaceHosts(CSlowServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(SlowServerHostID))
			).f_CallSync(pRunLoop, g_Timeout)
		;

		{
			TCPromise<void> Promise;
			ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket)
				(
					CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ServerAddress}
				)
				> Promise / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddClientConnection)(_Ticket.m_Ticket, g_Timeout, -1) > Promise.f_ReceiveAny();
				}
			;
			Promise.f_MoveFuture().f_CallSync(pRunLoop, g_Timeout);
		}
		{
			TCPromise<void> Promise;
			ManyServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket)
				(
					CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ManyServerAddress}
				)
				> Promise / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddClientConnection)(_Ticket.m_Ticket, g_Timeout, -1) > Promise.f_ReceiveAny();
				}
			;
			Promise.f_MoveFuture().f_CallSync(pRunLoop, g_Timeout);
		}
		{
			TCPromise<void> Promise;
			SlowServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket)
				(
					CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{SlowServerAddress}
				)
				> Promise / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddClientConnection)(_Ticket.m_Ticket, g_Timeout, -1) > Promise.f_ReceiveAny();
				}
			;
			Promise.f_MoveFuture().f_CallSync(pRunLoop, g_Timeout);
		}

		// Before running a command on the client we also need a command line control object
		TCDistributedActor<CCommandLineControlActorTest> pCommandLineControl = Dependencies.m_DistributionManager->f_ConstructActor<CCommandLineControlActorTest>(false);
		CCommandLineControl CommandLineControl;
		CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

		auto ConsoleProperties = NSys::fg_GetConsoleProperties();
		CommandLineControl.m_CommandLineWidth = ConsoleProperties.m_Width;
		CommandLineControl.m_CommandLineHeight = ConsoleProperties.m_Height;

		// Now that we have a default user in the client and a command line control we will run a dummy command. That is the usual way the authentication handler is registered
		// on the server side. We will perform all the tests after the command has returned.
		auto CommandLineHostID = ClientLaunch.f_Test_Command("CommandLineHostID", {}).f_CallSync(pRunLoop, g_Timeout).f_String();
		NStorage::TCSharedPointer<CCommandLineControl> pCommandLine = fg_Construct(fg_Move(CommandLineControl));
		NEncoding::CEJSON JSON =
			{
				"Command"_= "--test-actor"
				, "ConcurrentLogging"_= true
				, "StdErrLogger"_= false
				, "TraceLogger"_= true
				, "Color"_= false
				, "Color24Bit"_= false
				, "ColorLight"_= false
				, "BoxDrawing"_= false
				, "TerminalWidth"_= -1
				, "TerminalHeight"_= -1
				, "HelpCurrentCommand"_= false
				, "HelpCurrentCommandVerbose"_= false
				, "AuthenticationLifetime"_= CPermissionRequirements::mc_OverrideLifetimeNotSet
			}
		;

		auto fHostInfo = [&Info = fg_GetCallingHostInfo()](CStr const &_UserID, CStr const &_HostID = "")
			{
				return CCallingHostInfo
					(
						Info.f_GetDistributionManager()
						, Info.f_GetAuthenticationHandler()
						, Info.f_GetUniqueHostID()
						, _HostID ? CHostInfo(_HostID, _HostID) : Info.f_GetHostInfo()
						, Info.f_LastExecutionID()
						, Info.f_GetProtocolVersion()
						, _UserID
					 	, "Test"
					 	, Info.f_GetHost()
					)
				;
			}
		;

		ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--test-actor", JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);

		// Setup authentication for the user. For the sake of simplicity we will setup the user in TrustManager. We don't need the user there but it is a full trust manager,
		// both ClientTrust and ServerTrust are CDistributedActorTrustManagerInterface with a more restricted interface.
		TrustManager(&CDistributedActorTrustManager::f_AddUser, DefaultUserID, "Default User").f_CallSync(pRunLoop, g_Timeout);
		TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, DefaultUserID).f_CallSync(pRunLoop, g_Timeout);
		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Succeed1").f_CallSync(pRunLoop, g_Timeout);
		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Succeed2").f_CallSync(pRunLoop, g_Timeout);
		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Succeed3").f_CallSync(pRunLoop, g_Timeout);
		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Fail1").f_CallSync(pRunLoop, g_Timeout);
		auto ExportedWithPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, true).f_CallSync(pRunLoop, g_Timeout);
		auto ExportedWithoutPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, false).f_CallSync(pRunLoop, g_Timeout);
		fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ClientTrust, ExportedWithPrivate).f_CallSync(pRunLoop, g_Timeout);
		fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);
		fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ManyServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);
		fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, SlowServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);


		// Add a permissions to Server that requires authentication
		auto fAddHostUserPermission = [HostID = ClientHostID, UserID = DefaultUserID, pRunLoop]
			(
				NStorage::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> _pServerTrust
				, CStr const &_Permission
				, CPermissionRequirements const &_Methods
			)
			{
				TCMap<CStr, CPermissionRequirements> Permissions;
				Permissions[_Permission] = _Methods;

				auto &ServerTrust = *_pServerTrust;
				ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddPermissions)
					(
					 	CDistributedActorTrustManagerInterface::CAddPermissions{{HostID, UserID}, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions}
					).f_CallSync(pRunLoop, g_Timeout)
				;
			}
		;
		fAddHostUserPermission(pServerTrust, "com.malterlib/NoAuth1", {});
		fAddHostUserPermission(pServerTrust, "com.malterlib/NoAuth2", {});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1", CPermissionRequirements{{{"Succeed1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1a", CPermissionRequirements{{{"Succeed1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1b", CPermissionRequirements{{{"Succeed1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1c", CPermissionRequirements{{{"Succeed1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1d", CPermissionRequirements{{{"Succeed1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed2", CPermissionRequirements{{{"Succeed2"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed3", CPermissionRequirements{{{"Succeed3"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed12", CPermissionRequirements{{{"Succeed1", "Succeed2"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed23", CPermissionRequirements{{{"Succeed2", "Succeed3"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed123", CPermissionRequirements{{{"Succeed1", "Succeed2", "Succeed3"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Fail1", CPermissionRequirements{{{"Fail1"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed123_Fail1", CPermissionRequirements{{{"Succeed1", "Succeed2", "Succeed3", "Fail1"}}, 0});
        fAddHostUserPermission(pServerTrust, "com.malterlib/Password", CPermissionRequirements{{{"Password"}}, 0});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1_Cacheable", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed12_Cacheable", CPermissionRequirements{{{"Succeed1", "Succeed2"}}});
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed3_Cacheable", CPermissionRequirements{{{"Succeed3"}}});
        fAddHostUserPermission(pServerTrust, "com.malterlib/Password_Cacheable", CPermissionRequirements{{{"Password"}}});
        fAddHostUserPermission(pServerTrust, "com.malterlib/Fail1_Cacheable", CPermissionRequirements{{{"Fail1"}}});
		fAddHostUserPermission(pManyServerTrust, "com.manymalterlib/Succeed1_Cacheable", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission(pManyServerTrust, "com.manymalterlib/Succeed2_Cacheable", CPermissionRequirements{{{"Succeed2"}}});
		fAddHostUserPermission(pManyServerTrust, "com.manymalterlib/Succeed3_Cacheable", CPermissionRequirements{{{"Succeed3"}}});
		fAddHostUserPermission(pSlowServerTrust, "com.slowmalterlib/Succeed1_Cacheable", CPermissionRequirements{{{"Succeed1"}}});

		using CCallCounts = TCMap<CStr, aint>;
		auto fCallCounts = [pRunLoop]
			{
				return g_CallCount(&CCallCount::f_GetCounts).f_CallSync(pRunLoop, g_Timeout);
			}
		;

		#define DCheckCallCounts(d_Expected, d_Description) DMibTest(DMibExpr(d_Description) && DMibExpr(fCallCounts()) == DMibExpr(d_Expected))

		int64 CurrentAuthLifetime = CPermissionRequirements::mc_OverrideLifetimeNotSet;

		auto fCommonParams = [&](CStr const &_Command) -> CEJSON
			{
				return
					{
						"Command"_= _Command
						, "ConcurrentLogging"_= true
						, "StdErrLogger"_= false
						, "TraceLogger"_= true
						, "Color"_= false
						, "Color24Bit"_= false
						, "ColorLight"_= false
						, "BoxDrawing"_= false
						, "TerminalWidth"_= -1
						, "TerminalHeight"_= -1
						, "HelpCurrentCommand"_= false
						, "HelpCurrentCommandVerbose"_= false
						, "AuthenticationLifetime"_= CurrentAuthLifetime
					}
				;
			}
		;

		auto fPreauthenticate = [&](CStr const &_Pattern, TCVector<CStr> const &_Factors, int64 _Lifetime) -> CEJSON
			{
				NEncoding::CEJSON JSON = fCommonParams("--trust-user-authenticate-pattern");

				JSON["Pattern"] = _Pattern;
				JSON["AuthenticationFactors"] = _Factors;
				JSON["AuthenticationLifetime"] = _Lifetime;
				JSON["JSONOutput"] = true;

				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_Clear)().f_CallSync(pRunLoop, g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--trust-user-authenticate-pattern", JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);

				return CEJSON::fs_FromString(pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnStdOut)().f_CallSync(pRunLoop, g_Timeout));
			}
		;

		auto fPerformSimpleCall = [&](CStr const &_Command, TCVector<CStr> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams(_Command);

				JSON["Permissions"] = _Permissions;

				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_Clear)().f_CallSync(pRunLoop, g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), _Command, JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);
				return CEJSON::fs_FromString(pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnStdOut)().f_CallSync(pRunLoop, g_Timeout)).f_Boolean();
			}
		;

		auto fPerformCall2 = [&](TCVector<CPermissionQueryLocal> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams("--perform-call-2");

				auto &Queries = JSON["Permissions"];
				for (auto &Query : _Permissions)
					Queries.f_Insert(Query.f_ToJSON());

				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_Clear)().f_CallSync(pRunLoop, g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--perform-call-2", JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);
				return CEJSON::fs_FromString(pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnStdOut)().f_CallSync(pRunLoop, g_Timeout)).f_Boolean();
			}
		;

		auto fPerformCall3 = [&](TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permissions) -> TCMap<CStr, bool>
			{
				NEncoding::CEJSON JSON = fCommonParams("--perform-call-3");

				for (auto &Permissions : _Permissions)
				{
					auto &Tag = _Permissions.fs_GetKey(Permissions);

					auto &Queries = JSON["Permissions"][Tag];
					for (auto &Query : Permissions)
						Queries.f_Insert(Query.f_ToJSON());
				}

				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_Clear)().f_CallSync(pRunLoop, g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--perform-call-3", JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);

				TCMap<CStr, bool> Return;
				auto Results = CEJSON::fs_FromString(pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnStdOut)().f_CallSync(pRunLoop, g_Timeout));

				for (auto &Result : Results.f_Object())
					Return[Result.f_Name()] = Result.f_Value().f_Boolean();

				return Return;
			}
		;

		auto fPerformCall1 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-call-1", _Permissions); };
		auto fPerformManyCall1 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-many-call-1", _Permissions); };
		auto fPerformManyCall2 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-many-call-2", _Permissions); };
		auto fPerformManyCall3 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-many-call-3", _Permissions); };
		auto fPerformManyCall4 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-many-call-4", _Permissions); };
		auto fPerformSlowCall1 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-slow-call-1", _Permissions); };

		{
			// f_HasPermission is the simple one - choose the simplest authentication factor the host/user combo has permission for
			DMibTestPath("HasPermission");

			//Basics
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1");

			DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Fail1"}));
			DCheckCallCounts(CCallCounts({{"Fail1", 1}}), "Fail1");

			DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Nonexistant1"}));
			DCheckCallCounts(CCallCounts{}, "Nonexistant1");

			// Multiple authetications needed for single permission
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed123"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}, {"Succeed3", 1}}), "Succeed123");

			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed123_Fail1"}));


			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. Ifboth have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 NonExistant1");

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "NonExistant1 Succeed1");

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}));
			DCheckCallCounts(CCallCounts{}, "Succeed1 NoAuth1");

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}));
			DCheckCallCounts(CCallCounts{}, "NoAuth1 Succeed1");

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 Succeed2");

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed2 Succeed1");
		};

		using CQueries = TCVector<CPermissionQuery>;
		{
			// Now it starts to get more complex - this is multiple queries like the one above but where each has too succeed
			DMibTestPath("HasPermissions(Multiple)");

			//Basics - should behave as above
			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed1"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1");

			DMibExpectFalse(fPerformCall2(CQueries{{"com.malterlib/Fail1"}}));
			DCheckCallCounts(CCallCounts({{"Fail1", 1}}), "Fail1");

			DMibExpectFalse(fPerformCall2(CQueries{{"com.malterlib/Nonexistant1"}}));
			DCheckCallCounts(CCallCounts{}, "Nonexistant1");

			// Multiple authetications needed for single permission
			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed123"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1},{"Succeed2", 1}, {"Succeed3", 1}}), "Succeed123");

			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse(fPerformCall2(CQueries{{"com.malterlib/Succeed123_Fail1"}}));
			DMibExpectFalse(fPerformCall2(CQueries{{"com.malterlib/Succeed123_Nonexistant1"}}));

			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. If both have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 NonExistant1");

			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "NonExistant1 Succeed1");

			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}}));
			DCheckCallCounts(CCallCounts{}, "Succeed1 NoAuth1");

			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}}));
			DCheckCallCounts(CCallCounts{}, "NoAuth1 Succeed1");

			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 Succeed2");

			DMibExpectTrue(fPerformCall2(CQueries{{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed2 Succeed1");

			// Now it is time to verify that we can handle the case where we need multiple permissions
			// Select the factor that exists in both sets
			CQueries Many1{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed3"}};
			DMibExpectTrue(fPerformCall2(Many1));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many1");

			CQueries Many2{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}};
			DMibExpectTrue(fPerformCall2(Many2));
			DCheckCallCounts(CCallCounts({{"Succeed2", 1}}), "Many2");

			//Select the simplest factor if there are multiple choices
			CQueries Many3{{"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Fail1"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Succeed3"}};
			DMibExpectTrue(fPerformCall2(Many3));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many3");

			// Superset implies subsets
			CQueries Many4{{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1"}};
			DMibExpectTrue(fPerformCall2(Many4));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}), "Many4");

			CQueries Many5{{"com.malterlib/Succeed1", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}};
			DMibExpectTrue(fPerformCall2(Many5));
			DCheckCallCounts(CCallCounts({{"Succeed2", 1}, {"Succeed3", 1}}), "Many5");

			// 12 is better than 23 (lower category)
			CQueries Many6
				{
					{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}
				}
			;
			DMibExpectTrue(fPerformCall2(Many6));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}), "Many6");

			// Many permissions use the same authentication factor
			CQueries Many7{{"com.malterlib/Succeed1a"}, {"com.malterlib/Succeed1b"}, {"com.malterlib/Succeed1c"}, {"com.malterlib/Succeed1d"}};
			DMibExpectTrue(fPerformCall2(Many7));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many7");
		};

		auto fMQ1 = [](CStr _Tag, CQueries &&_Query) -> TCMap<CStr, TCVector<CPermissionQuery>>
			{
				TCMap<CStr, TCVector<CPermissionQuery>> Result;
				Result[_Tag] = fg_Move(_Query);
				return Result;
			}
		;
//		auto fMQ2 = [](CStr _Tag1, CQueries &&_Query1, CStr _Tag2, CQueries &&_Query2) -> TCMap<CStr, TCVector<CPermissionQuery>>
//			{
//				TCMap<CStr, TCVector<CPermissionQuery>> Result;
//				Result[_Tag1] = fg_Move(_Query1);
//				Result[_Tag2] = fg_Move(_Query2);
//				return Result;
//			}
//		;
		auto fMQ3 = [](CStr _Tag1, CQueries &&_Query1, CStr _Tag2, CQueries &&_Query2, CStr _Tag3, CQueries &&_Query3) -> TCMap<CStr, TCVector<CPermissionQuery>>
			{
				TCMap<CStr, TCVector<CPermissionQuery>> Result;
				Result[_Tag1] = fg_Move(_Query1);
				Result[_Tag2] = fg_Move(_Query2);
				Result[_Tag3] = fg_Move(_Query3);
				return Result;
			}
		;
		{
			// Now it starts to get more complex - this is multiple queries like the one above but where each has to succeed
			DMibTestPath("HasPermissions(Tagged)");

			// Basics - should behave as above
			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Basics Succeed1");

			DMibExpectFalse(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Fail1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Fail1", 1}}), "Basics Fail1");

			DMibExpectFalse(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Nonexistant1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts{}, "Basics Nonexistant1");

			// Multiple authetications needed for single permission
			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed123"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1},{"Succeed2", 1}, {"Succeed3", 1}}), "Basics Succeed123");

			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed123_Fail1"}}))["Tag"]);
			DMibExpectFalse(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed123_Nonexistant1"}}))["Tag"]);

			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. If both have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 NonExistant1");

			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "NonExistant1 Succeed1");

			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts{}, "Succeed1 NoAuth1");

			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts{}, "NoAuth1 Succeed1");

			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1 Succeed2");

			DMibExpectTrue(fPerformCall3(fMQ1("Tag", CQueries{{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}}))["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed2 Succeed1");

			// Now it is time to verify that we can handle the case where we need multiple permissions
			// Select the factor that exists in both sets
			auto Many1 = fMQ1("Tag", CQueries{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed3"}});
			DMibExpectTrue(fPerformCall3(Many1)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many1");

			auto Many2 = fMQ1("Tag", {{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}});
			DMibExpectTrue(fPerformCall3(Many2)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed2", 1}}), "Many2");

			//Select the simplest factor if there are multiple choices
			auto Many3 = fMQ1
				(
				 	"Tag"
				 	, {{"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Fail1"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Succeed3"}}
				)
			;
			DMibExpectTrue(fPerformCall3(Many3)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many3");

			// Superset implies subsets
			auto Many4 = fMQ1("Tag", {{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1"}});
			DMibExpectTrue(fPerformCall3(Many4)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}), "Many4");

			auto Many5 = fMQ1
				(
				 	"Tag"
				 	, {{"com.malterlib/Succeed1", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}}
				)
			;
			DMibExpectTrue(fPerformCall3(Many5)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed2", 1}, {"Succeed3", 1}}), "Many5");

			// 12 is better than 23 (lower category)
			auto Many6 = fMQ1
				(
				 	"Tag"
				 	, {{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}}
				)
			;
			DMibExpectTrue(fPerformCall3(Many6)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}), "Many6");

			// Many permissions use the same authentication factor
			auto Many7 = fMQ1("Tag", {{"com.malterlib/Succeed1a"}, {"com.malterlib/Succeed1b"}, {"com.malterlib/Succeed1c"}, {"com.malterlib/Succeed1d"}});
			DMibExpectTrue(fPerformCall3(Many7)["Tag"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many7");

			// Check that the correct tag fails
			auto Many8 = fMQ3("Tag1", CQueries{{"com.malterlib/Succeed1"}}, "Tag2", CQueries{{"com.malterlib/Succeed2"}}, "Tag3", CQueries{{"com.malterlib/Fail1"}});
			auto Result1 = fPerformCall3(Many8);
			DMibExpectTrue(Result1["Tag1"]);
			DMibExpectTrue(Result1["Tag2"]);
			DMibExpectFalse(Result1["Tag3"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}, {"Fail1", 1}}), "Many8");

			auto Many9 = fMQ3("Tag1", CQueries{{"com.malterlib/Succeed1"}}, "Tag2", CQueries{{"com.malterlib/Succeed2"}, {"com.malterlib/Fail1"}}, "Tag3", CQueries{{"com.malterlib/Fail1"}});
			auto Result2 = fPerformCall3(Many9);
			DMibExpectTrue(Result2["Tag1"]);
			DMibExpectFalse(Result2["Tag2"]);
			DMibExpectFalse(Result2["Tag3"]);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}, {"Fail1", 1}}), "Many9");
		};

		// Test password authentication
		{
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Password").f_CallSync(pRunLoop, g_Timeout);
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Wrong").f_CallSync(pRunLoop, g_Timeout);
			DMibExpectException
				(
					(TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Password").f_CallSync(pRunLoop, g_Timeout))
					, DMibErrorInstance("Password mismatch")
				)
			;
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Password").f_CallSync(pRunLoop, g_Timeout);
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Password").f_CallSync(pRunLoop, g_Timeout);
			TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Password").f_CallSync(pRunLoop, g_Timeout);

			auto ExportedWithPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, true).f_CallSync(pRunLoop, g_Timeout);
			auto ExportedWithoutPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, false).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ClientTrust, ExportedWithPrivate).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);
			{
				DMibTestPath("Correct Password");
				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Password").f_CallSync(pRunLoop, g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			{
				DMibTestPath("Wrong Password");
				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Wrong").f_CallSync(pRunLoop, g_Timeout);
				DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			// Add another password factor - both password should work for authentication
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("OtherPassword").f_CallSync(pRunLoop, g_Timeout);
			pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("OtherPassword").f_CallSync(pRunLoop, g_Timeout);
			TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "Password").f_CallSync(pRunLoop, g_Timeout);
			ExportedWithPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, true).f_CallSync(pRunLoop, g_Timeout);
			ExportedWithoutPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, false).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ClientTrust, ExportedWithPrivate).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);
			{
				DMibTestPath("Correct Password 1");
				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("Password").f_CallSync(pRunLoop, g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			{
				DMibTestPath("Correct Password 2");
				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnString)("OtherPassword").f_CallSync(pRunLoop, g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
		}

		// Test preauthentication (Do this before testing the cache. Once peermissions are cached it will be hard to tell if it was preauthentication or the cache that prevented a call.)
		{
			DMibTestPath("Preauthentication");
			CurrentAuthLifetime = CPermissionRequirements::mc_OverrideLifetimeNotSet;
			g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);

			auto Results = fPreauthenticate("*", { "Succeed1" }, 5);
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1");
			DMibExpect(Results, ==, CEJSON{"AuthenticationFailures"_= _[_]});

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
			DCheckCallCounts(CCallCounts{}, "Succeed1_Cacheable");

			// We have only preauhenticated Succeed1, so we should still get a call for Succeed2
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed12_Cacheable"}));
			DCheckCallCounts(CCallCounts({{"Succeed2", 1}}), "Succeed12_Cacheable");

			// Not affected
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed3_Cacheable"}));
			DCheckCallCounts(CCallCounts({{"Succeed3", 1}}), "Succeed3_Cacheable");

			// fPerformCall1( and fPerformCall3 takes slighty different paths through the code, make sure this is cached too
			auto Many1 = fMQ1("Tag", CQueries{{"com.malterlib/Succeed1_Cacheable", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed1_Cacheable", "com.malterlib/Succeed3"}});
			DMibExpectTrue(fPerformCall3(Many1)["Tag"]);
			DCheckCallCounts(CCallCounts{}, "Many1");

			// Make sure the "*" pattern matched on all servers
			DMibExpectTrue(fPerformSlowCall1(TCVector<CStr>{"com.slowmalterlib/Succeed1_Cacheable"}));
			DCheckCallCounts(CCallCounts{}, "Slow Succeed1_Cacheable");

			DMibExpectTrue(fPerformManyCall1(TCVector<CStr>{"com.manymalterlib/Succeed1_Cacheable"}));
			DCheckCallCounts(CCallCounts{}, "Many Succeed1_Cacheable");
			{
				DMibTestPath("Non-cachable should not be preauthenticated");
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1");
			}
			{
				DMibTestPath("Zero lifetime invalidates old preauthentication");
				Results = fPreauthenticate("*", { "Succeed1" }, 0);
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1");

				DMibExpect(Results, ==, CEJSON{"AuthenticationFailures"_= _[_]});
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1_Cacheable");
			}
			{
				DMibTestPath("Preauthentication should only affect matching subscription patterns");

				Results = fPreauthenticate("com.malterlib/*", { "Succeed1" }, 5);
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Preauth");

				DMibExpect(Results, ==, CEJSON{"AuthenticationFailures"_= _[_]});
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed1_Cacheable");

				DMibExpectTrue(fPerformSlowCall1(TCVector<CStr>{"com.slowmalterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Slow Succeed1_Cacheable");

				DMibExpectTrue(fPerformManyCall1(TCVector<CStr>{"com.manymalterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Many Succeed1_Cacheable");

				{
					DMibTestPath("Again");
					DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
					DCheckCallCounts(CCallCounts{}, "Succeed1_Cacheable");
				}
			}
			{
				DMibTestPath("Preauthentication should not be done if the authentication fails");
				Results = fPreauthenticate("*", { "Fail1" }, 5);
				DCheckCallCounts(CCallCounts({{"Fail1", 1}}), "Fail1");

				DMibExpect(Results["AuthenticationFailures"].f_Array().f_GetLen(), ==, 3);
				DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Fail1_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Fail1", 1}}), "Fail1_Cacheable");
			}
			{
				DMibTestPath("Preauthentication should be propagated to other subscriptions");
				Results = fPreauthenticate("com.manymalterlib/*", { "Succeed3" }, 5);
				DCheckCallCounts(CCallCounts({{"Succeed3", 1}}), "Succeed3");

				ManyServerLaunch.f_Test_Command("SubscribePermissions4", {}).f_CallSync(pRunLoop, g_Timeout);
				DMibExpectTrue(fPerformManyCall4(TCVector<CStr>{"com.manymalterlib/Succeed3_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed3_Cacheable");
			}
			{
				DMibTestPath("Preauthentication should not work when the permission has been removed");
				pManyServerTrust->f_CallActor(&CDistributedActorTrustManagerInterface::f_RemovePermissions)
					(
					 	CDistributedActorTrustManagerInterface::CRemovePermissions
						{
							{ClientHostID, DefaultUserID}
							, {"com.manymalterlib/Succeed3_Cacheable"}
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						}
					).f_CallSync(pRunLoop, g_Timeout)
				;
				DMibExpectFalse(fPerformManyCall4(TCVector<CStr>{"com.manymalterlib/Succeed3_Cacheable"}));
				g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			}
			// Make sure the preauthenticated pattens doesn't interfere with the rest of the testing
			fPreauthenticate("com.malterlib/*", { "Succeed1" }, 0);
			fPreauthenticate("com.manymalterlib/*", { "Succeed3" }, 0);
		}

		// Test caching of authentications
		{
			DMibTestPath("Caching");

			g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			CurrentAuthLifetime = 10;
			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
			DCheckCallCounts(CCallCounts({{"Succeed1", 1}}), "Succeed1_Cacheable");
			{
				DMibTestPath("Succeed1 cached");
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed1_Cacheable");

				// The factor Success1 should be cached but that is for the "com.malterlib/Succeed1_Cacheable" permission, for a new permission all factors must be authenticated
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed12_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}), "Succeed12_Cacheable");

				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed3_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed3", 1}}), "Succeed3_Cacheable");
			}
			{
				DMibTestPath("All cached");
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed1_Cacheable");

				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed12_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed12_Cacheable");

				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed3_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed3_Cacheable");
			}
			{
				DMibTestPath("Cached permission should be propagated to other subscriptions");
				DMibExpectTrue(fPerformManyCall1(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				DCheckCallCounts(CCallCounts({{"Succeed2", 1}}), "Succeed2_Cacheable");

				DMibExpectTrue(fPerformManyCall2(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Many2 Succeed2_Cacheable");

				// Live permissions in the authentication cache should be propagated to the newly created permission subscription
				ManyServerLaunch.f_Test_Command("SubscribePermissions3", {}).f_CallSync(pRunLoop, g_Timeout);
				DMibExpectTrue(fPerformManyCall3(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Many3 Succeed2_Cacheable");
			}
			{
				DMibTestPath("Cached permission should be removed when the permission is removed");
				pManyServerTrust->f_CallActor(&CDistributedActorTrustManagerInterface::f_RemovePermissions)
					(
					 	CDistributedActorTrustManagerInterface::CRemovePermissions
						{
							{ClientHostID, DefaultUserID}
							, {"com.manymalterlib/Succeed2_Cacheable"}
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						}
					).f_CallSync(pRunLoop, g_Timeout)
				;
				DMibExpectFalse(fPerformManyCall3(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				g_CallCount(&CCallCount::f_Clear).f_CallSync(pRunLoop, g_Timeout);
			}
		}
	}

	void fp_DoU2FTests()
	{
#ifdef DPlatformFamily_Windows
		AllocConsole();
		SetConsoleCtrlHandler
			(
				nullptr
				, true
			)
		;
#endif

#if DTestDistributredAppAuthenticationEnableLogging
		fg_GetSys()->f_AddStdErrLogger();
#endif
		TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
		auto CleanupRunLoop = g_OnScopeExit > [&]
			{
				while (pRunLoop->f_RefCountGet() > 0)
					pRunLoop->f_WaitOnceTimeout(0.1);
			}
		;
		TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
		auto CleanupHelperActor = g_OnScopeExit > [&]
			{
				HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
		;
		CCurrentlyProcessingActorScope CurrentActor{HelperActor};

		CStr ProgramDirectory = CFile::fs_GetProgramDirectory();
		CStr RootDirectory = ProgramDirectory + "/DistributedAppAuthenticationTests";

		CProcessLaunch::fs_KillProcessesInDirectory("*", {}, RootDirectory, g_Timeout);

		for (mint i = 0; i < 5; ++i)
		{
			try
			{
				if (CFile::fs_FileExists(RootDirectory))
					CFile::fs_DeleteDirectoryRecursive(RootDirectory);
				break;
			}
			catch (NFile::CExceptionFile const &)
			{
			}
		}

		CFile::fs_CreateDirectory(RootDirectory);

		CTrustManagerTestHelper TrustManagerState;
		TCActor<CDistributedActorTrustManager> TrustManager = TrustManagerState.f_TrustManager("TestHelper", {}, false);
		auto CleanupTrustManager = g_OnScopeExit > [&]
			{
				TrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
		;

		CStr TestHostID = TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
		CTrustedSubscriptionTestHelper Subscriptions{TrustManager};

		CDistributedActorTrustManager_Address TestAddress;
		TestAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/controller.sock"_f << RootDirectory);
		TrustManager(&CDistributedActorTrustManager::f_AddListen, TestAddress).f_CallSync(pRunLoop, g_Timeout);

		CDistributedApp_LaunchHelperDependencies Dependencies;
		Dependencies.m_Address = TestAddress.m_URL;
		Dependencies.m_TrustManager = TrustManager;
		Dependencies.m_DistributionManager = TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(pRunLoop, g_Timeout);

		NMib::NConcurrency::CDistributedActorSecurity Security;
		static constexpr ch8 const *c_pDefaultNamespace = "com.malterlib/Concurrency/AuthenticationTest";
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(c_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(ICDistributedActorAuthentication::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CServerInterface::mc_pDefaultNamespace);
		Dependencies.m_DistributionManager(&CActorDistributionManager::f_SetSecurity, Security).f_CallSync(pRunLoop, g_Timeout);

		TCActor<CDistributedApp_LaunchHelper> LaunchHelper = fg_ConstructActor<CDistributedApp_LaunchHelper>(Dependencies, DTestDistributredAppAuthenticationEnableLogging);
		g_CallCount = fg_ConstructActor<CCallCount>();

		auto Cleanup = g_OnScopeExit > [&]
			{
				LaunchHelper->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				g_CallCount.f_Clear();
			}
		;

		static auto constexpr c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
		// Launch Server side (who needs authentication for commands)
		CStr ServerName = "Server";
		CStr ServerDirectory = RootDirectory + "/" + ServerName;
		CFile::fs_CreateDirectory(ServerDirectory);
		auto ServerLaunch = LaunchHelper
			(
				&CDistributedApp_LaunchHelper::f_LaunchInProcess
				, ServerName
				, ServerDirectory
				, &fg_ConstructApp_Server
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pServerTrust = ServerLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> ServerTrust = *pServerTrust;
		CStr ServerHostID = ServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ServerAddress;
		ServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsServer.sock"_f << RootDirectory);
		ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(ServerAddress).f_CallSync(pRunLoop, g_Timeout);

		TrustManager.f_CallActor(&CDistributedActorTrustManager::f_AllowHostsForNamespace)
			(
				CServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(pRunLoop, g_Timeout)
		;

		auto Server = Subscriptions.f_Subscribe<CServerInterface>();

		auto DefaultUserID = fg_RandomID();

		// Launch Client side (who provides authentication)
		CStr ClientName = "Client";
		CStr ClientDirectory = RootDirectory + "/" + ClientName;
		CFile::fs_CreateDirectory(ClientDirectory);
		auto ClientLaunch = LaunchHelper
			(
				&CDistributedApp_LaunchHelper::f_LaunchInProcess
				, ClientName
				, ClientDirectory
				, fg_ConstructApp_ClientFactory(DefaultUserID)
				, NContainer::TCVector<NStr::CStr>{}
			)
			.f_CallSync(pRunLoop, g_Timeout)
		;
		auto pClientTrust = ClientLaunch.m_pTrustInterface;
		NConcurrency::TCDistributedActor<CDistributedActorTrustManagerInterface> ClientTrust = *pClientTrust;
		CStr ClientHostID = ClientLaunch.m_HostID;

		CDistributedActorTrustManager_Address ClientAddress;
		ClientAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsClient.sock"_f << RootDirectory);
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddListen)(ClientAddress).f_CallSync(pRunLoop, g_Timeout);

		auto fNamespaceHosts = [](auto &&_Namespace, auto &&_Hosts)
			{
				return CDistributedActorTrustManagerInterface::CChangeNamespaceHosts{_Namespace, _Hosts, c_WaitForSubscriptions};
			}
		;

		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
			(
				fNamespaceHosts(ICDistributedActorAuthentication::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(pRunLoop, g_Timeout)
		;
		ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace)
			(
				fNamespaceHosts(CServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(pRunLoop, g_Timeout)
		;

		{
			TCPromise<void> Promise;
			ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket)
				(
					CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ServerAddress}
				)
				> Promise / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					ClientTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddClientConnection)(_Ticket.m_Ticket, g_Timeout, -1) > Promise.f_ReceiveAny();
				}
			;
			Promise.f_MoveFuture().f_CallSync(pRunLoop, g_Timeout);
		}

		// Before running a command on the client we also need a command line control object
		TCDistributedActor<CCommandLineControlActorTest> pCommandLineControl = Dependencies.m_DistributionManager->f_ConstructActor<CCommandLineControlActorTest>(true);
		CCommandLineControl CommandLineControl;
		CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

		auto ConsoleProperties = NSys::fg_GetConsoleProperties();
		CommandLineControl.m_CommandLineWidth = ConsoleProperties.m_Width;
		CommandLineControl.m_CommandLineHeight = ConsoleProperties.m_Height;

		// Now that we have a default user in the client and a command line control we will run a dummy command. That is the usual way the authentication handler is registered
		// on the server side. We will perform all the tests after the command has returned.
		auto CommandLineHostID = ClientLaunch.f_Test_Command("CommandLineHostID", {}).f_CallSync(pRunLoop, g_Timeout).f_String();
		NStorage::TCSharedPointer<CCommandLineControl> pCommandLine = fg_Construct(fg_Move(CommandLineControl));
		NEncoding::CEJSON JSON =
			{
				"Command"_= "--test-actor"
				, "ConcurrentLogging"_= true
				, "StdErrLogger"_= false
				, "TraceLogger"_= true
				, "Color"_= false
				, "Color24Bit"_= false
				, "ColorLight"_= false
				, "BoxDrawing"_= false
				, "TerminalWidth"_= -1
				, "TerminalHeight"_= -1
				, "HelpCurrentCommand"_= false
				, "HelpCurrentCommandVerbose"_= false
				, "AuthenticationLifetime"_= CPermissionRequirements::mc_OverrideLifetimeNotSet
			}
		;

		auto fHostInfo = [&Info = fg_GetCallingHostInfo()](CStr const &_UserID, CStr const &_HostID = "")
			{
				return CCallingHostInfo
					(
						Info.f_GetDistributionManager()
						, Info.f_GetAuthenticationHandler()
						, Info.f_GetUniqueHostID()
						, _HostID ? CHostInfo(_HostID, _HostID) : Info.f_GetHostInfo()
						, Info.f_LastExecutionID()
						, Info.f_GetProtocolVersion()
						, _UserID
					 	, "(Test)"
					 	, Info.f_GetHost()
					)
				;
			}
		;

		ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--test-actor", JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);

		// Setup authentication for the user. For the sake of simplicity we will setup the user in TrustManager. We don't need the user there but it is a full trust manager,
		// both ClientTrust and ServerTrust are CDistributedActorTrustManagerInterface with a more restricted interface.
		TrustManager(&CDistributedActorTrustManager::f_AddUser, DefaultUserID, "Default User").f_CallSync(pRunLoop, g_Timeout);
		TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, DefaultUserID).f_CallSync(pRunLoop, g_Timeout);


		// Add a permissions to Server that requires authentication
		auto fAddHostUserPermission = [HostID = ClientHostID, UserID = DefaultUserID, pRunLoop]
			(
				NStorage::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> _pServerTrust
				, CStr const &_Permission
				, CPermissionRequirements const &_Methods
			)
			{
				TCMap<CStr, CPermissionRequirements> Permissions;
				Permissions[_Permission] = _Methods;

				auto &ServerTrust = *_pServerTrust;
				ServerTrust.f_CallActor(&CDistributedActorTrustManagerInterface::f_AddPermissions)
					(
					 	CDistributedActorTrustManagerInterface::CAddPermissions{{HostID, UserID}, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions}
					).f_CallSync(pRunLoop, g_Timeout)
				;
			}
		;
		fAddHostUserPermission(pServerTrust, "com.malterlib/Succeed1", CPermissionRequirements{{{"U2F"}}, 0});


		auto fCommonParams = [&](CStr const &_Command) -> CEJSON
			{
				return
					{
						"Command"_= _Command
						, "ConcurrentLogging"_= true
						, "StdErrLogger"_= false
						, "TraceLogger"_= true
						, "Color"_= false
						, "Color24Bit"_= false
						, "ColorLight"_= false
						, "BoxDrawing"_= false
						, "TerminalWidth"_= -1
						, "TerminalHeight"_= -1
						, "HelpCurrentCommand"_= false
						, "HelpCurrentCommandVerbose"_= false
						, "AuthenticationLifetime"_= 0
					}
				;
			}
		;

		auto fPerformSimpleCall = [&](CStr const &_Command, TCVector<CStr> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams(_Command);

				JSON["Permissions"] = _Permissions;

				pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_Clear)().f_CallSync(pRunLoop, g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), _Command, JSON, pCommandLine).f_CallSync(pRunLoop, g_Timeout);
				return CEJSON::fs_FromString(pCommandLineControl.f_CallActor(&CCommandLineControlActorTest::f_ReturnStdOut)().f_CallSync(pRunLoop, g_Timeout)).f_Boolean();
			}
		;

		auto fPerformCall1 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-call-1", _Permissions); };

		// Test U2F registration and authentication
		{
			TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "U2F").f_CallSync(pRunLoop, g_Timeout);

			auto ExportedWithPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, true).f_CallSync(pRunLoop, g_Timeout);
			auto ExportedWithoutPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, false).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ClientTrust, ExportedWithPrivate).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));

			// Add another U2F factor - both should work for authentication
			TrustManager.f_CallActor(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor)(pCommandLine, DefaultUserID, "U2F").f_CallSync(pRunLoop, g_Timeout);
			ExportedWithPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, true).f_CallSync(pRunLoop, g_Timeout);
			ExportedWithoutPrivate = fg_CallSafeDispatched(&fg_ExportUser<CDistributedActorTrustManager>, TrustManager, DefaultUserID, false).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ClientTrust, ExportedWithPrivate).f_CallSync(pRunLoop, g_Timeout);
			fg_CallSafeDispatched(&fg_ImportUser<TCDistributedActorWrapper<CDistributedActorTrustManagerInterface>>, ServerTrust, ExportedWithoutPrivate).f_CallSync(pRunLoop, g_Timeout);

			{
				DMibTestPath("Still works with two registered factors");
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));
			}
		}
	}
};

DMibTestRegister(CDistributedAppAuthentication_Tests, Malterlib::Concurrency);
