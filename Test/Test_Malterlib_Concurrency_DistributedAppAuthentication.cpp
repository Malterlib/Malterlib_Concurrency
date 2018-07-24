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
using namespace NMib::NPtr;
using namespace NMib::NAtomic;
using namespace NMib::NEncoding;
using namespace NMib::NStorage;
using namespace NMib::NNet;
using namespace NMib::NStr;
using namespace NMib::NContainer;

#define DTestDistributredAppAuthenticationEnableLogging 0

static fp64 g_Timeout = 60.0;

namespace
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

		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions1LifeTime(TCVector<CStr> const &_Permissions, int64 _AuthenticationLifetime) = 0;
		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions2(TCVector<CPermissionQueryLocal> const &_Permission) = 0;
		virtual NConcurrency::TCContinuation<TCMap<CStr, bool>> f_CheckPermissions3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permission) = 0;
	};

	struct CServerActor : public CDistributedAppActor
	{
		CServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings
				(
					"DistributedAppAuthenticationTestsServer"
					, false
					, NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/Server"
					, true
					, NConcurrency::CDistributedActorTestKeySettings{}
				)
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
				mp_ProtocolInterface.f_Publish<CServerInterface>(mp_AppState.m_DistributionManager, this, CServerInterface::mc_pDefaultNamespace);
			}

			struct CServerInterfaceImplementation : public CServerInterface
			{
				TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<bool> f_CheckPermissions1LifeTime(TCVector<CStr> const &_Permissions, int64 _AuthenticationLifetime) override
				{
					TCContinuation<bool> Continuation;
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

					m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions, CallingHostInfo) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<bool> f_CheckPermissions2(TCVector<CPermissionQueryLocal> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions.f_HasPermissions("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<TCMap<CStr, bool>> f_CheckPermissions3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permissions) override
				{
					TCContinuation<TCMap<CStr, bool>> Continuation;

					m_pThis->mp_Permissions.f_HasPermissions("Test", _Permissions) > Continuation / [Continuation](NContainer::TCMap<NStr::CStr, bool> _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				CServer *m_pThis;
			};

			TCContinuation<void> f_SubscribePermissions()
			{
				TCContinuation<void> Continuation;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.malterlib/*")
						 , fg_ThisActor(this)
					) > Continuation / [this, Continuation](CTrustedPermissionSubscription &&_TrustedSubscription)
					{
						mp_Permissions = fg_Move(_TrustedSubscription);
						Continuation.f_SetResult();
					}
				;
				return Continuation;
			}

		private:
			TCContinuation<void> fp_Destroy() override
			{
				mp_ProtocolInterface.m_Publication.f_Clear();

				return fg_Explicit();
			}

			TCDelegatedActorInterface<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions;
		};

	private:
		TCContinuation<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CServerActor::CServer>(fg_Construct(self), mp_State);
			return mp_ServerActor(&CServer::f_SubscribePermissions);
		}

		TCContinuation<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CServerActor, Info, "Shutting down server");

				mp_ServerActor->f_Destroy() > [pCanDestroy](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(Mib/Concurrency/CServerActor, Error, "Failed to shut down server: {}", _Result.f_GetExceptionStr());
					}
				;
			}
			return pCanDestroy->m_Continuation;
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

		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions2(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions3(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions4(TCVector<CStr> const &_Permissions) = 0;
	};

	// This server is used to test the behavior multiple multiple permission subscriptions
	struct CManyServerActor : public CDistributedAppActor
	{
		CManyServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings
				(
					"DistributedAppAuthenticationTestsManyServer"
					, false
					, NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/ManyServer"
					, true
					, NConcurrency::CDistributedActorTestKeySettings{}
				)
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
				mp_ProtocolInterface.f_Publish<CManyServerInterface>(mp_AppState.m_DistributionManager, this, CManyServerInterface::mc_pDefaultNamespace);
			}

			struct CServerInterfaceImplementation : public CManyServerInterface
			{
				TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions1.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<bool> f_CheckPermissions2(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions2.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<bool> f_CheckPermissions3(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions3.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				TCContinuation<bool> f_CheckPermissions4(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions4.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				CServer *m_pThis;
			};

			TCContinuation<void> f_SubscribePermissions()
			{
				TCContinuation<void> Continuation;

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
					) > Continuation / [this, Continuation](CTrustedPermissionSubscription &&_TrustedSubscription1, CTrustedPermissionSubscription &&_TrustedSubscription2)
					{
						mp_Permissions1 = fg_Move(_TrustedSubscription1);
						mp_Permissions2 = fg_Move(_TrustedSubscription2);
						Continuation.f_SetResult();
					}
				;
				return Continuation;
			}

			TCContinuation<void> f_SubscribePermissions3()
			{
				TCContinuation<void> Continuation;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					) > Continuation / [this, Continuation](CTrustedPermissionSubscription &&_TrustedSubscription3)
					{
						mp_Permissions3 = fg_Move(_TrustedSubscription3);
						Continuation.f_SetResult();
					}
				;
				return Continuation;
			}

			TCContinuation<void> f_SubscribePermissions4()
			{
				TCContinuation<void> Continuation;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.manymalterlib/*")
						 , fg_ThisActor(this)
					) > Continuation / [this, Continuation](CTrustedPermissionSubscription &&_TrustedSubscription4)
					{
						mp_Permissions4 = fg_Move(_TrustedSubscription4);
						Continuation.f_SetResult();
					}
				;
				return Continuation;
			}

		private:
			TCContinuation<void> fp_Destroy() override
			{
				mp_ProtocolInterface.m_Publication.f_Clear();

				return fg_Explicit();
			}

			TCDelegatedActorInterface<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions1;
			CTrustedPermissionSubscription mp_Permissions2;
			CTrustedPermissionSubscription mp_Permissions3;
			CTrustedPermissionSubscription mp_Permissions4;
		};

	private:
		TCContinuation<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override
		{
			TCContinuation<NEncoding::CEJSON> Continuation;
			if (_Command == "SubscribePermissions3")
			{
				mp_ServerActor(&CServer::f_SubscribePermissions3) > Continuation / [Continuation]
					{
						Continuation.f_SetResult(CEJSON{});
					}
				;
				return Continuation;
			}
			if (_Command == "SubscribePermissions4")
			{
				mp_ServerActor(&CServer::f_SubscribePermissions4) > Continuation / [Continuation]
					{
						Continuation.f_SetResult(CEJSON{});
					}
				;
				return Continuation;
			}
			return DMibErrorInstance("Unhandled command in fp_Test_Command: {}"_f << _Command);
		}

		TCContinuation<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CManyServerActor::CServer>(fg_Construct(self), mp_State);
			return mp_ServerActor(&CServer::f_SubscribePermissions);
		}

		TCContinuation<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CManyServerActor, Info, "Shutting down server");

				mp_ServerActor->f_Destroy() > [pCanDestroy](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(Mib/Concurrency/CManyServerActor, Error, "Failed to shut down server: {}", _Result.f_GetExceptionStr());
					}
				;
			}
			return pCanDestroy->m_Continuation;
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

		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
	};

	struct CSlowServerActor : public CDistributedAppActor
	{
		CSlowServerActor()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings
				(
					"DistributedAppAuthenticationTestsSlowServer"
					, false
					, NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/SlowServer"
					, true
					, NConcurrency::CDistributedActorTestKeySettings{}
				)
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
				mp_ProtocolInterface.f_Publish<CSlowServerInterface>(mp_AppState.m_DistributionManager, this, CSlowServerInterface::mc_pDefaultNamespace);
			}

			struct CServerInterfaceImplementation : public CSlowServerInterface
			{
				TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) override
				{
					TCContinuation<bool> Continuation;

					m_pThis->mp_Permissions.f_HasPermission("Test", _Permissions) > Continuation / [Continuation](bool _Result)
						{
							Continuation.f_SetResult(_Result);
						}
					;
					return Continuation;
				}

				CServer *m_pThis;
			};

			TCContinuation<void> f_SubscribePermissions()
			{
				TCContinuation<void> Continuation;

				mp_AppState.m_TrustManager
					(
						 &CDistributedActorTrustManager::f_SubscribeToPermissions
						 , fg_CreateVector<CStr>("com.slowmalterlib/*")
						 , fg_ThisActor(this)
					) > Continuation / [this, Continuation](CTrustedPermissionSubscription &&_TrustedSubscription)
					{
						mp_Permissions = fg_Move(_TrustedSubscription);
						Continuation.f_SetResult();
					}
				;
				return Continuation;
			}

		private:
			TCContinuation<void> fp_Destroy() override
			{
				mp_ProtocolInterface.m_Publication.f_Clear();

				return fg_Explicit();
			}

			TCDelegatedActorInterface<CServerInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			CTrustedPermissionSubscription mp_Permissions;
		};

	private:
		TCContinuation<void> fp_StartApp(CEJSON const &_Params) override
		{
			mp_ServerActor = fg_ConstructActor<CSlowServerActor::CServer>(fg_Construct(self), mp_State);
			return mp_ServerActor(&CServer::f_SubscribePermissions);
		}

		TCContinuation<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CSlowServerActor, Info, "Shutting down server");

				mp_ServerActor->f_Destroy() > [pCanDestroy](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(Mib/Concurrency/CSlowServerActor, Error, "Failed to shut down server: {}", _Result.f_GetExceptionStr());
					}
				;
			}
			return pCanDestroy->m_Continuation;
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
				CDistributedAppActor_Settings
				(
					"DistributedAppAuthenticationTestsClient"
					, false
					, NFile::CFile::fs_GetProgramDirectory() + "/DistributedAppAuthenticationTests/Client"
					, true
					, NConcurrency::CDistributedActorTestKeySettings{}
				)
			 	.f_SupportUserAuthentication(false)
			)
			, m_DefaultUserID(_DefaultUserID)
		{
		}

		TCContinuation<CActorSubscription> fp_SetupAuthentication
			(
			 	NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, CStr const &_UserID
			) override
		{
			return fp_EnableAuthentication(_pCommandLine, _AuthenticationLifetime, _UserID);
		}

		TCContinuation<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override
		{
			TCContinuation<NEncoding::CEJSON> Continuation;
			if (_Command == "CommandLineHostID")
			{
				Continuation.f_SetResult(mp_State.m_CommandLineHostID);
				return Continuation;
			}
			return DMibErrorInstance("Unhandled command in fp_Test_Command: {}"_f << _Command);
		}

	private:
		TCContinuation<void> fp_StartApp(CEJSON const &_Params) override
		{
			TCContinuation<void> Continuation;

			// Have to create the user and set it as default user before registering the authentication handler.
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddUser, m_DefaultUserID, "Default User") > Continuation / [this, Continuation]
				{
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, m_DefaultUserID)  > Continuation / [this, Continuation]
						{
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
								> [this, Continuation]
								(
									TCAsyncResult<TCTrustedActorSubscription<CServerInterface>> &&_Subscription
									, TCAsyncResult<TCTrustedActorSubscription<CManyServerInterface>> &&_ManySubscription
									, TCAsyncResult<TCTrustedActorSubscription<CSlowServerInterface>> &&_SlowSubscription
								)
								{
									if (!_Subscription)
									{
										DMibLogWithCategory(Malterlib/Concurrency/TestDistAppAuthentication, Error, "Failed to subscribe to server: {}", _Subscription.f_GetExceptionStr());
										Continuation.f_SetException(_Subscription);
										return;
									}
									if (!_ManySubscription)
									{
										DMibLogWithCategory
											(
											 	Malterlib/Concurrency/TestDistAppAuthentication
											 	, Error, "Failed to subscribe to many server: {}"
											 	, _ManySubscription.f_GetExceptionStr()
											)
										;
										Continuation.f_SetException(_ManySubscription);
										return;
									}
									if (!_SlowSubscription)
									{
										DMibLogWithCategory
											(
											 	Malterlib/Concurrency/TestDistAppAuthentication
											 	, Error
											 	, "Failed to subscribe to slow server: {}"
											 	, _SlowSubscription.f_GetExceptionStr()
											)
										;
										Continuation.f_SetException(_SlowSubscription);
										return;
									}

									mp_TestServers = fg_Move(*_Subscription);
									mp_ManyTestServers = fg_Move(*_ManySubscription);
									mp_SlowTestServers = fg_Move(*_SlowSubscription);

									Continuation.f_SetResult();
								}
							;
						}
					;
				}
			;

			return fg_Explicit();
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
					, [](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						return fg_Explicit(0);
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

			struct CTestCommandOutput
			{
				CTestCommandOutput(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					: m_pCommandLine(_pCommandLine)

				{
				}

				~CTestCommandOutput()
				{
					*m_pCommandLine += m_Results.f_ToString();
					if (!m_Continuation.f_IsSet())
						m_Continuation.f_SetResult(0);
				}

				CEJSON m_Results;
				TCContinuation<uint32> m_Continuation;
				NPtr::TCSharedPointer<CCommandLineControl> m_pCommandLine;
			};

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions1, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-2"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						TCVector<CPermissionQueryLocal> Permissions;
						for (auto &QueryJSON : _Params["Permissions"].f_Array())
							Permissions.f_Insert(CPermissionQueryLocal::fs_FromJSON(QueryJSON));

						DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions2, Permissions)
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-call-3"}, "Description"_= ".", fSharedParameters(EJSONType_Object)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pTestServer = mp_TestServers.f_GetOneActor("", Error);
						if (!pTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						TCMap<CStr, TCVector<CPermissionQueryLocal>> Permissions;
						for (auto &Queries : _Params["Permissions"].f_Object())
						{
							auto &OutputPermissions = Permissions[Queries.f_Name()];
							for (auto &QueryJSON : Queries.f_Value().f_Array())
								OutputPermissions.f_Insert(CPermissionQueryLocal::fs_FromJSON(QueryJSON));
						}

						DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions3, Permissions)
							> pOutput->m_Continuation / [=](TCMap<CStr, bool> &&_Results)
							{
								for (auto &Result : _Results)
									pOutput->m_Results[_Results.fs_GetKey(Result)] = Result;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pManyTestServer->m_Actor, CManyServerInterface::f_CheckPermissions1, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-2"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pManyTestServer->m_Actor, CManyServerInterface::f_CheckPermissions2, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-3"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pManyTestServer->m_Actor, CManyServerInterface::f_CheckPermissions3, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-many-call-4"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pManyTestServer = mp_ManyTestServers.f_GetOneActor("", Error);
						if (!pManyTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pManyTestServer->m_Actor, CManyServerInterface::f_CheckPermissions4, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;

			Section.f_RegisterCommand
				(
					{ "Names"_= {"--perform-slow-call-1"}, "Description"_= ".", fSharedParameters(EJSONType_Array)}
					, [this](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						TCSharedPointer<CTestCommandOutput> pOutput = fg_Construct(_pCommandLine);

						CStr Error;
						auto pSlowTestServer = mp_SlowTestServers.f_GetOneActor("", Error);
						if (!pSlowTestServer)
							return DMibErrorInstance(fg_Format("Error selecting server: {}", Error));

						DMibCallActor(pSlowTestServer->m_Actor, CSlowServerInterface::f_CheckPermissions1, fs_GetCommandPermissions(_Params["Permissions"]))
							> pOutput->m_Continuation / [=](bool _bSuccess)
							{
								pOutput->m_Results = _bSuccess;
							}
						;

						return pOutput->m_Continuation;
					}
				)
			;
		}

		TCContinuation<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			return pCanDestroy->m_Continuation;
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

		TCContinuation<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<NContainer::CSecureByteVector> f_ReadBinary() override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<NStr::CStrSecure> f_ReadLine() override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) override
		{
			return fg_Explicit(m_ReturnValues.f_PopBack());
		}

		TCContinuation<void> f_AbortReads() override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<void> f_StdOut(NStr::CStrSecure const &_Output) override
		{
			m_StdOut += _Output;
			return fg_Explicit();
		}

		TCContinuation<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) override
		{
			DMibNeverGetHere;
			return fg_Explicit();
		}

		TCContinuation<void> f_StdErr(NStr::CStrSecure const &_Output) override
		{
			if (m_bEchoStdErr)
				DMibConOut("{}", _Output);
			else
				m_StdErr += _Output;

			return fg_Explicit();
		}

		TCContinuation<void> f_ReturnString(NStr::CStrSecure const &_String)
		{
			m_ReturnValues.f_InsertLast(_String);
			return fg_Explicit();
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
		TCContinuation<void> fp_Destroy() override
		{
			return fg_Explicit();
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

		TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = m_Category;
			Result.m_Name = m_Name;

			return fg_Explicit(Result);
		}

		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
				, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		{
			TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
			ICDistributedActorAuthenticationHandler::CResponse Response;
			for (auto const &RegisteredFactor : _Factors)
			{
				Response.m_SignedProperties = _SignedProperties;
				Response.m_Signature.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				break;
			}
			g_CallCount(&CCallCount::f_AddCall, m_Name) > Continuation / [Continuation, Response = fg_Move(Response)]() mutable
				{
					Continuation.f_SetResult(fg_Move(Response));
				}
			;
			return Continuation;
		};

		TCContinuation<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
				ICDistributedActorAuthenticationHandler::CResponse const &_Response
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
				, CAuthenticationData const &_AuthenticationData
			) override
		{
			return fg_Explicit(CVerifyAuthenticationReturn{bool(_Response.m_Signature == CByteVector((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen()))});
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

		TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = EAuthenticationFactorCategory_None;
			Result.m_Name = "Fail1";

			return fg_Explicit(Result);
		}

		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
				, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		{
			TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
			ICDistributedActorAuthenticationHandler::CResponse Response;
			for (auto const &RegisteredFactor : _Factors)
			{
				Response.m_SignedProperties = _SignedProperties;
				Response.m_Signature.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				break;
			}
			g_CallCount(&CCallCount::f_AddCall, m_Name) > Continuation / [Continuation, Response = fg_Move(Response)]() mutable
				{
					Continuation.f_SetResult(fg_Move(Response));
				}
			;
			return Continuation;
		};

		TCContinuation<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
				ICDistributedActorAuthenticationHandler::CResponse const &_Response
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
				, CAuthenticationData const &_AuthenticationData
			) override
		{
			return fg_Explicit(CVerifyAuthenticationReturn{false});
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
		CStr TestHostID = TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(g_Timeout);
		CTrustedSubscriptionTestHelper Subscriptions{TrustManager};

		CDistributedActorTrustManager_Address TestAddress;
		TestAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/controller.sock"_f << RootDirectory);
		TrustManager(&CDistributedActorTrustManager::f_AddListen, TestAddress).f_CallSync(g_Timeout);

		CDistributedApp_LaunchHelperDependencies Dependencies;
		Dependencies.m_Address = TestAddress.m_URL;
		Dependencies.m_TrustManager = TrustManager;
		Dependencies.m_DistributionManager = TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(g_Timeout);

		NMib::NConcurrency::CDistributedActorSecurity Security;
		static constexpr ch8 const *c_pDefaultNamespace = "com.malterlib/Concurrency/AuthenticationTest";
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(c_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(ICDistributedActorAuthentication::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CServerInterface::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CManyServerInterface::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CSlowServerInterface::mc_pDefaultNamespace);
		Dependencies.m_DistributionManager(&CActorDistributionManager::f_SetSecurity, Security).f_CallSync(g_Timeout);

		TCActor<CDistributedApp_LaunchHelper> LaunchHelper = fg_ConstructActor<CDistributedApp_LaunchHelper>(Dependencies, DTestDistributredAppAuthenticationEnableLogging);
		g_CallCount = fg_ConstructActor<CCallCount>();

		auto Cleanup = g_OnScopeExit > [&]
			{
				LaunchHelper->f_BlockDestroy();
				g_CallCount.f_Clear();
			}
		;

		static auto constexpr c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
		// Launch Server side (who needs authentication for commands)
		CStr ServerName = "Server";
		CStr ServerDirectory = RootDirectory + "/" + ServerName;
		CFile::fs_CreateDirectory(ServerDirectory);
		auto ServerLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, ServerName, ServerDirectory, &fg_ConstructApp_Server).f_CallSync(g_Timeout);
		auto pServerTrust = ServerLaunch.m_pTrustInterface;
		auto &ServerTrust = *pServerTrust;
		CStr ServerHostID = ServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ServerAddress;
		ServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsServer.sock"_f << RootDirectory);
		DMibCallActor(ServerTrust, CDistributedActorTrustManagerInterface::f_AddListen, ServerAddress).f_CallSync(g_Timeout);

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

		auto Server = Subscriptions.f_Subscribe<CServerInterface>();

		CStr ManyServerName = "ManyServer";
		CStr ManyServerDirectory = RootDirectory + "/" + ManyServerName;
		CFile::fs_CreateDirectory(ManyServerDirectory);
		auto ManyServerLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, ManyServerName, ManyServerDirectory, &fg_ConstructApp_ManyServer).f_CallSync(g_Timeout);
		auto pManyServerTrust = ManyServerLaunch.m_pTrustInterface;
		auto &ManyServerTrust = *pManyServerTrust;
		CStr ManyServerHostID = ManyServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ManyServerAddress;
		ManyServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsManyServer.sock"_f << RootDirectory);
		DMibCallActor(ManyServerTrust, CDistributedActorTrustManagerInterface::f_AddListen, ManyServerAddress).f_CallSync(g_Timeout);

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CManyServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ManyServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

		auto ManyServer = Subscriptions.f_Subscribe<CManyServerInterface>();

		CStr SlowServerName = "SlowServer";
		CStr SlowServerDirectory = RootDirectory + "/" + SlowServerName;
		CFile::fs_CreateDirectory(SlowServerDirectory);
		auto SlowServerLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, SlowServerName, SlowServerDirectory, &fg_ConstructApp_SlowServer).f_CallSync(g_Timeout);
		auto pSlowServerTrust = SlowServerLaunch.m_pTrustInterface;
		auto &SlowServerTrust = *pSlowServerTrust;
		CStr SlowServerHostID = SlowServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address SlowServerAddress;
		SlowServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsSlowServer.sock"_f << RootDirectory);
		DMibCallActor(SlowServerTrust, CDistributedActorTrustManagerInterface::f_AddListen, SlowServerAddress).f_CallSync(g_Timeout);

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CSlowServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(SlowServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

		auto SlowServer = Subscriptions.f_Subscribe<CSlowServerInterface>();

		auto DefaultUserID = fg_RandomID();

		// Launch Client side (who provides authentication)
		CStr ClientName = "Client";
		CStr ClientDirectory = RootDirectory + "/" + ClientName;
		CFile::fs_CreateDirectory(ClientDirectory);
		auto ClientLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, ClientName, ClientDirectory, fg_ConstructApp_ClientFactory(DefaultUserID)).f_CallSync(g_Timeout);
		auto pClientTrust = ClientLaunch.m_pTrustInterface;
		auto &ClientTrust = *pClientTrust;
		CStr ClientHostID = ClientLaunch.m_HostID;

		CDistributedActorTrustManager_Address ClientAddress;
		ClientAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsClient.sock"_f << RootDirectory);
		DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddListen, ClientAddress).f_CallSync(g_Timeout);

		auto fNamespaceHosts = [](auto &&_Namespace, auto &&_Hosts)
			{
				return CDistributedActorTrustManagerInterface::CChangeNamespaceHosts{_Namespace, _Hosts, c_WaitForSubscriptions};
			}
		;

		for (auto const &HostID : {ServerHostID, ManyServerHostID, SlowServerHostID})
		{
			DMibCallActor
				(
					ClientTrust
					, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
					, fNamespaceHosts(ICDistributedActorAuthentication::mc_pDefaultNamespace, fg_CreateSet<CStr>(HostID))
				).f_CallSync(g_Timeout)
			;
		}
		DMibCallActor
			(
				ClientTrust
				, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
				, fNamespaceHosts(CServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(g_Timeout)
		;
		DMibCallActor
			(
				ClientTrust
				, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
				, fNamespaceHosts(CManyServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ManyServerHostID))
			).f_CallSync(g_Timeout)
		;
		DMibCallActor
			(
				ClientTrust
				, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
				, fNamespaceHosts(CSlowServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(SlowServerHostID))
			).f_CallSync(g_Timeout)
		;

		auto HelperActor = fg_ConcurrentActor();
		CCurrentActorScope CurrentActor{HelperActor};

		{
			TCContinuation<void> Continuation;
			DMibCallActor
				(
					ServerTrust
					, CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket
					, CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ServerAddress}
				)
				> Continuation / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddClientConnection, _Ticket.m_Ticket, g_Timeout, -1) > Continuation.f_ReceiveAny();
				}
			;
			Continuation.f_CallSync(g_Timeout);
		}
		{
			TCContinuation<void> Continuation;
			DMibCallActor
				(
					ManyServerTrust
					, CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket
					, CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ManyServerAddress}
				)
				> Continuation / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddClientConnection, _Ticket.m_Ticket, g_Timeout, -1) > Continuation.f_ReceiveAny();
				}
			;
			Continuation.f_CallSync(g_Timeout);
		}
		{
			TCContinuation<void> Continuation;
			DMibCallActor
				(
					SlowServerTrust
					, CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket
					, CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{SlowServerAddress}
				)
				> Continuation / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddClientConnection, _Ticket.m_Ticket, g_Timeout, -1) > Continuation.f_ReceiveAny();
				}
			;
			Continuation.f_CallSync(g_Timeout);
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
		auto CommandLineHostID = ClientLaunch.f_Test_Command("CommandLineHostID", {}).f_CallSync(g_Timeout).f_String();
		NPtr::TCSharedPointer<CCommandLineControl> pCommandLine = fg_Construct(fg_Move(CommandLineControl));
		NEncoding::CEJSON JSON =
			{
				"Command"_= "--test-actor"
				, "ConcurrentLogging"_= true
				, "StdErrLogger"_= false
				, "TraceLogger"_= true
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

		ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--test-actor", JSON, pCommandLine).f_CallSync(g_Timeout);

		// Setup authentication for the user. For the sake of simplicity we will setup the user in TrustManager. We don't need the user there but it is a full trust manager,
		// both ClientTrust and ServerTrust are CDistributedActorTrustManagerInterface with a more restricted interface.
		TrustManager(&CDistributedActorTrustManager::f_AddUser, DefaultUserID, "Default User").f_CallSync(g_Timeout);
		TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, DefaultUserID).f_CallSync(g_Timeout);
		DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Succeed1").f_CallSync(g_Timeout);
		DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Succeed2").f_CallSync(g_Timeout);
		DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Succeed3").f_CallSync(g_Timeout);
		DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Fail1").f_CallSync(g_Timeout);
		auto ExportedWithPrivate = fg_ExportUser(TrustManager, DefaultUserID, true).f_CallSync(60.0);
		auto ExportedWithoutPrivate = fg_ExportUser(TrustManager, DefaultUserID, false).f_CallSync(60.0);
		fg_ImportUser(ClientTrust, ExportedWithPrivate).f_CallSync(60.0);
		fg_ImportUser(ServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);
		fg_ImportUser(ManyServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);
		fg_ImportUser(SlowServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);


		// Add a permissions to Server that requires authentication
		auto fAddHostUserPermission = [HostID = ClientHostID, UserID = DefaultUserID]
			(
				NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> _pServerTrust
				, CStr const &_Permission
				, CPermissionRequirements const &_Methods
			)
			{
				TCMap<CStr, CPermissionRequirements> Permissions;
				Permissions[_Permission] = _Methods;

				auto &ServerTrust = *_pServerTrust;
				DMibCallActor
					(
					 	ServerTrust
						, CDistributedActorTrustManagerInterface::f_AddPermissions
					 	, CDistributedActorTrustManagerInterface::CAddPermissions{{HostID, UserID}, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions}
					).f_CallSync(60.0)
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
		auto fCallCounts = []
			{
				return g_CallCount(&CCallCount::f_GetCounts).f_CallSync(g_Timeout);
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

				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_Clear).f_CallSync(g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--trust-user-authenticate-pattern", JSON, pCommandLine).f_CallSync(g_Timeout);

				return CEJSON::fs_FromString(DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnStdOut).f_CallSync(g_Timeout));
			}
		;

		auto fPerformSimpleCall = [&](CStr const &_Command, TCVector<CStr> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams(_Command);

				JSON["Permissions"] = _Permissions;

				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_Clear).f_CallSync(g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), _Command, JSON, pCommandLine).f_CallSync(g_Timeout);
				return CEJSON::fs_FromString(DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnStdOut).f_CallSync(g_Timeout)).f_Boolean();
			}
		;

		auto fPerformCall2 = [&](TCVector<CPermissionQueryLocal> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams("--perform-call-2");

				auto &Queries = JSON["Permissions"];
				for (auto &Query : _Permissions)
					Queries.f_Insert(Query.f_ToJSON());

				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_Clear).f_CallSync(g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--perform-call-2", JSON, pCommandLine).f_CallSync(g_Timeout);
				return CEJSON::fs_FromString(DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnStdOut).f_CallSync(g_Timeout)).f_Boolean();
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

				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_Clear).f_CallSync(g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--perform-call-3", JSON, pCommandLine).f_CallSync(g_Timeout);

				TCMap<CStr, bool> Return;
				auto Results = CEJSON::fs_FromString(DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnStdOut).f_CallSync(g_Timeout));

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
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
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
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
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
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
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
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Password").f_CallSync(g_Timeout);
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Wrong").f_CallSync(g_Timeout);
			DMibExpectException
				(
					(DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Password").f_CallSync(g_Timeout))
					, DMibErrorInstance("Password mismatch")
				)
			;
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Password").f_CallSync(g_Timeout);
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Password").f_CallSync(g_Timeout);
			DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Password").f_CallSync(g_Timeout);
			auto ExportedWithPrivate = fg_ExportUser(TrustManager, DefaultUserID, true).f_CallSync(60.0);
			auto ExportedWithoutPrivate = fg_ExportUser(TrustManager, DefaultUserID, false).f_CallSync(60.0);
			fg_ImportUser(ClientTrust, ExportedWithPrivate).f_CallSync(60.0);
			fg_ImportUser(ServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);
			{
				DMibTestPath("Correct Password");
				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Password").f_CallSync(g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			{
				DMibTestPath("Wrong Password");
				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Wrong").f_CallSync(g_Timeout);
				DMibExpectFalse(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			// Add another password factor - both password should work for authentication
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "OtherPassword").f_CallSync(g_Timeout);
			DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "OtherPassword").f_CallSync(g_Timeout);
			DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "Password").f_CallSync(g_Timeout);
			ExportedWithPrivate = fg_ExportUser(TrustManager, DefaultUserID, true).f_CallSync(60.0);
			ExportedWithoutPrivate = fg_ExportUser(TrustManager, DefaultUserID, false).f_CallSync(60.0);
			fg_ImportUser(ClientTrust, ExportedWithPrivate).f_CallSync(60.0);
			fg_ImportUser(ServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);
			{
				DMibTestPath("Correct Password 1");
				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "Password").f_CallSync(g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
			{
				DMibTestPath("Correct Password 2");
				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnString, "OtherPassword").f_CallSync(g_Timeout);
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Password"}));
			}
		}

		// Test preauthentication (Do this before testing the cache. Once peermissions are cached it will be hard to tell if it was preauthentication or the cache that prevented a call.)
		{
			DMibTestPath("Preauthentication");
			CurrentAuthLifetime = CPermissionRequirements::mc_OverrideLifetimeNotSet;
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);

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

				ManyServerLaunch.f_Test_Command("SubscribePermissions4", {}).f_CallSync(g_Timeout);
				DMibExpectTrue(fPerformManyCall4(TCVector<CStr>{"com.manymalterlib/Succeed3_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Succeed3_Cacheable");
			}
			{
				DMibTestPath("Preauthentication should not work when the permission has been removed");
				DMibCallActor
					(
					 	*pManyServerTrust
						, CDistributedActorTrustManagerInterface::f_RemovePermissions
					 	, CDistributedActorTrustManagerInterface::CRemovePermissions
						{
							{ClientHostID, DefaultUserID}
							, {"com.manymalterlib/Succeed3_Cacheable"}
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						}
					).f_CallSync(60.0)
				;
				DMibExpectFalse(fPerformManyCall4(TCVector<CStr>{"com.manymalterlib/Succeed3_Cacheable"}));
				g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
			}
			// Make sure the preauthenticated pattens doesn't interfere with the rest of the testing
			fPreauthenticate("com.malterlib/*", { "Succeed1" }, 0);
			fPreauthenticate("com.manymalterlib/*", { "Succeed3" }, 0);
		}

		// Test caching of authentications
		{
			DMibTestPath("Caching");

			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
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
				ManyServerLaunch.f_Test_Command("SubscribePermissions3", {}).f_CallSync(g_Timeout);
				DMibExpectTrue(fPerformManyCall3(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				DCheckCallCounts(CCallCounts{}, "Many3 Succeed2_Cacheable");
			}
			{
				DMibTestPath("Cached permission should be removed when the permission is removed");
				DMibCallActor
					(
					 	*pManyServerTrust
						, CDistributedActorTrustManagerInterface::f_RemovePermissions
					 	, CDistributedActorTrustManagerInterface::CRemovePermissions
						{
							{ClientHostID, DefaultUserID}
							, {"com.manymalterlib/Succeed2_Cacheable"}
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						}
					).f_CallSync(60.0)
				;
				DMibExpectFalse(fPerformManyCall3(TCVector<CStr>{"com.manymalterlib/Succeed2_Cacheable"}));
				g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
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
		CStr TestHostID = TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(g_Timeout);
		CTrustedSubscriptionTestHelper Subscriptions{TrustManager};

		CDistributedActorTrustManager_Address TestAddress;
		TestAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/controller.sock"_f << RootDirectory);
		TrustManager(&CDistributedActorTrustManager::f_AddListen, TestAddress).f_CallSync(g_Timeout);

		CDistributedApp_LaunchHelperDependencies Dependencies;
		Dependencies.m_Address = TestAddress.m_URL;
		Dependencies.m_TrustManager = TrustManager;
		Dependencies.m_DistributionManager = TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(g_Timeout);

		NMib::NConcurrency::CDistributedActorSecurity Security;
		static constexpr ch8 const *c_pDefaultNamespace = "com.malterlib/Concurrency/AuthenticationTest";
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(c_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(ICDistributedActorAuthentication::mc_pDefaultNamespace);
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CServerInterface::mc_pDefaultNamespace);
		Dependencies.m_DistributionManager(&CActorDistributionManager::f_SetSecurity, Security).f_CallSync(g_Timeout);

		TCActor<CDistributedApp_LaunchHelper> LaunchHelper = fg_ConstructActor<CDistributedApp_LaunchHelper>(Dependencies, DTestDistributredAppAuthenticationEnableLogging);
		g_CallCount = fg_ConstructActor<CCallCount>();

		auto Cleanup = g_OnScopeExit > [&]
			{
				LaunchHelper->f_BlockDestroy();
				g_CallCount.f_Clear();
			}
		;

		static auto constexpr c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
		// Launch Server side (who needs authentication for commands)
		CStr ServerName = "Server";
		CStr ServerDirectory = RootDirectory + "/" + ServerName;
		CFile::fs_CreateDirectory(ServerDirectory);
		auto ServerLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, ServerName, ServerDirectory, &fg_ConstructApp_Server).f_CallSync(g_Timeout);
		auto pServerTrust = ServerLaunch.m_pTrustInterface;
		auto &ServerTrust = *pServerTrust;
		CStr ServerHostID = ServerLaunch.m_HostID;
		CDistributedActorTrustManager_Address ServerAddress;
		ServerAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsServer.sock"_f << RootDirectory);
		DMibCallActor(ServerTrust, CDistributedActorTrustManagerInterface::f_AddListen, ServerAddress).f_CallSync(g_Timeout);

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

		auto Server = Subscriptions.f_Subscribe<CServerInterface>();

		auto DefaultUserID = fg_RandomID();

		// Launch Client side (who provides authentication)
		CStr ClientName = "Client";
		CStr ClientDirectory = RootDirectory + "/" + ClientName;
		CFile::fs_CreateDirectory(ClientDirectory);
		auto ClientLaunch = LaunchHelper(&CDistributedApp_LaunchHelper::f_LaunchInProcess, ClientName, ClientDirectory, fg_ConstructApp_ClientFactory(DefaultUserID)).f_CallSync(g_Timeout);
		auto pClientTrust = ClientLaunch.m_pTrustInterface;
		auto &ClientTrust = *pClientTrust;
		CStr ClientHostID = ClientLaunch.m_HostID;

		CDistributedActorTrustManager_Address ClientAddress;
		ClientAddress.m_URL = "wss://[UNIX(666):{}]/"_f << fg_GetSafeUnixSocketPath("{}/DistributedAppAuthenticationTestsClient.sock"_f << RootDirectory);
		DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddListen, ClientAddress).f_CallSync(g_Timeout);

		auto fNamespaceHosts = [](auto &&_Namespace, auto &&_Hosts)
			{
				return CDistributedActorTrustManagerInterface::CChangeNamespaceHosts{_Namespace, _Hosts, c_WaitForSubscriptions};
			}
		;

		DMibCallActor
			(
				ClientTrust
				, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
				, fNamespaceHosts(ICDistributedActorAuthentication::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(g_Timeout)
		;
		DMibCallActor
			(
				ClientTrust
				, CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace
				, fNamespaceHosts(CServerInterface::mc_pDefaultNamespace, fg_CreateSet<CStr>(ServerHostID))
			).f_CallSync(g_Timeout)
		;

		auto HelperActor = fg_ConcurrentActor();
		CCurrentActorScope CurrentActor{HelperActor};

		{
			TCContinuation<void> Continuation;
			DMibCallActor
				(
					ServerTrust
					, CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket
					, CDistributedActorTrustManagerInterface::CGenerateConnectionTicket{ServerAddress}
				)
				> Continuation / [=](CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					auto &ClientTrust = *pClientTrust;
					DMibCallActor(ClientTrust, CDistributedActorTrustManagerInterface::f_AddClientConnection, _Ticket.m_Ticket, g_Timeout, -1) > Continuation.f_ReceiveAny();
				}
			;
			Continuation.f_CallSync(g_Timeout);
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
		auto CommandLineHostID = ClientLaunch.f_Test_Command("CommandLineHostID", {}).f_CallSync(g_Timeout).f_String();
		NPtr::TCSharedPointer<CCommandLineControl> pCommandLine = fg_Construct(fg_Move(CommandLineControl));
		NEncoding::CEJSON JSON =
			{
				"Command"_= "--test-actor"
				, "ConcurrentLogging"_= true
				, "StdErrLogger"_= false
				, "TraceLogger"_= true
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

		ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), "--test-actor", JSON, pCommandLine).f_CallSync(g_Timeout);

		// Setup authentication for the user. For the sake of simplicity we will setup the user in TrustManager. We don't need the user there but it is a full trust manager,
		// both ClientTrust and ServerTrust are CDistributedActorTrustManagerInterface with a more restricted interface.
		TrustManager(&CDistributedActorTrustManager::f_AddUser, DefaultUserID, "Default User").f_CallSync(g_Timeout);
		TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, DefaultUserID).f_CallSync(g_Timeout);


		// Add a permissions to Server that requires authentication
		auto fAddHostUserPermission = [HostID = ClientHostID, UserID = DefaultUserID]
			(
				NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> _pServerTrust
				, CStr const &_Permission
				, CPermissionRequirements const &_Methods
			)
			{
				TCMap<CStr, CPermissionRequirements> Permissions;
				Permissions[_Permission] = _Methods;

				auto &ServerTrust = *_pServerTrust;
				DMibCallActor
					(
					 	ServerTrust
						, CDistributedActorTrustManagerInterface::f_AddPermissions
					 	, CDistributedActorTrustManagerInterface::CAddPermissions{{HostID, UserID}, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions}
					).f_CallSync(60.0)
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
						, "AuthenticationLifetime"_= 0
					}
				;
			}
		;

		auto fPerformSimpleCall = [&](CStr const &_Command, TCVector<CStr> const &_Permissions) -> bool
			{
				NEncoding::CEJSON JSON = fCommonParams(_Command);

				JSON["Permissions"] = _Permissions;

				DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_Clear).f_CallSync(g_Timeout);
				ClientLaunch.f_RunCommandLine(fHostInfo("UserID", CommandLineHostID), _Command, JSON, pCommandLine).f_CallSync(g_Timeout);
				return CEJSON::fs_FromString(DMibCallActor(pCommandLineControl, CCommandLineControlActorTest::f_ReturnStdOut).f_CallSync(g_Timeout)).f_Boolean();
			}
		;

		auto fPerformCall1 = [&](TCVector<CStr> const &_Permissions) -> bool { return fPerformSimpleCall("--perform-call-1", _Permissions); };

		// Test U2F registration and authentication
		{
			DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "U2F").f_CallSync(g_Timeout);

			auto ExportedWithPrivate = fg_ExportUser(TrustManager, DefaultUserID, true).f_CallSync(60.0);
			auto ExportedWithoutPrivate = fg_ExportUser(TrustManager, DefaultUserID, false).f_CallSync(60.0);
			fg_ImportUser(ClientTrust, ExportedWithPrivate).f_CallSync(60.0);
			fg_ImportUser(ServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);

			DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));

			// Add another U2F factor - both should work for authentication
			DMibCallActor(TrustManager, CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, DefaultUserID, "U2F").f_CallSync(g_Timeout);
			ExportedWithPrivate = fg_ExportUser(TrustManager, DefaultUserID, true).f_CallSync(60.0);
			ExportedWithoutPrivate = fg_ExportUser(TrustManager, DefaultUserID, false).f_CallSync(60.0);
			fg_ImportUser(ClientTrust, ExportedWithPrivate).f_CallSync(60.0);
			fg_ImportUser(ServerTrust, ExportedWithoutPrivate).f_CallSync(60.0);

			{
				DMibTestPath("Still works with two registered factors");
				DMibExpectTrue(fPerformCall1(TCVector<CStr>{"com.malterlib/Succeed1"}));
			}
		}
	}
};

DMibTestRegister(CDistributedAppAuthentication_Tests, Malterlib::Concurrency);
