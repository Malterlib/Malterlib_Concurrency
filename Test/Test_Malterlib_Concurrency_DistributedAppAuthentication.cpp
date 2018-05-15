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
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions2);
			DMibPublishActorFunction(CServerInterface::f_CheckPermissions3);
		}

		virtual NConcurrency::TCContinuation<bool> f_CheckPermissions1(TCVector<CStr> const &_Permissions) = 0;
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

	struct CClientInterface : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppAuthenticationTestsClient";

		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CClientInterface()
		{
			DMibPublishActorFunction(CClientInterface::f_PerformCall1);
			DMibPublishActorFunction(CClientInterface::f_PerformCall2);
			DMibPublishActorFunction(CClientInterface::f_PerformCall3);
		}

		virtual NConcurrency::TCContinuation<bool> f_PerformCall1(TCVector<CStr> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<bool> f_PerformCall2(TCVector<CPermissionQueryLocal> const &_Permissions) = 0;
		virtual NConcurrency::TCContinuation<TCMap<CStr, bool>> f_PerformCall3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permissions) = 0;
	};

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

		struct CServer : public CActor
		{
		public:
			using CActorHolder = CDelegatedActorHolder;

			CServer(CDistributedAppState &_AppState, TCTrustedActorSubscription<CServerInterface> &&_Subscription)
				: mp_AppState(_AppState)
				, mp_TestServers(fg_Move(_Subscription))
			{
				mp_ProtocolInterface.f_Publish<CClientInterface>(mp_AppState.m_DistributionManager, this, CClientInterface::mc_pDefaultNamespace);
			}

			struct CClientInterfaceImplementation : public CClientInterface
			{
				TCContinuation<bool> f_PerformCall1(TCVector<CStr> const &_Permissions) override
				{
					CStr Error;
					auto pTestServer = m_pThis->mp_TestServers.f_GetOneActor("", Error);
					if (!pTestServer)
					{
						TCContinuation<bool> Continuation;
						Continuation.f_SetException(DMibErrorInstance(fg_Format("Error selecting server: {}", Error)));
						return Continuation;
					}

					return DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions1, _Permissions);
				}

				TCContinuation<bool> f_PerformCall2(TCVector<CPermissionQueryLocal> const &_Permissions) override
				{
					CStr Error;
					auto pTestServer = m_pThis->mp_TestServers.f_GetOneActor("", Error);
					if (!pTestServer)
					{
						TCContinuation<bool> Continuation;
						Continuation.f_SetException(DMibErrorInstance(fg_Format("Error selecting server: {}", Error)));
						return Continuation;
					}

					return DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions2, _Permissions);
				}

				TCContinuation<TCMap<CStr, bool>> f_PerformCall3(TCMap<CStr, TCVector<CPermissionQueryLocal>> const &_Permissions) override
				{
					CStr Error;
					auto pTestServer = m_pThis->mp_TestServers.f_GetOneActor("", Error);
					if (!pTestServer)
					{
						TCContinuation<TCMap<CStr, bool>> Continuation;
						Continuation.f_SetException(DMibErrorInstance(fg_Format("Error selecting server: {}", Error)));
						return Continuation;
					}

					return DMibCallActor(pTestServer->m_Actor, CServerInterface::f_CheckPermissions3, _Permissions);
				}

				CServer *m_pThis;
			};

		private:
			TCContinuation<void> fp_Destroy() override
			{
				mp_ProtocolInterface.m_Publication.f_Clear();

				return fg_Explicit();
			}

			TCDelegatedActorInterface<CClientInterfaceImplementation> mp_ProtocolInterface;
			CDistributedAppState &mp_AppState;
			TCTrustedActorSubscription<CServerInterface> mp_TestServers;
		};

		TCContinuation<void> fp_SetupAuthentication(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			return fp_EnableAuthentication(_pCommandLine);
		}

		TCContinuation<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override
		{
			TCContinuation<NEncoding::CEJSON> Continuation;
			if (_Command == "CommandLineHostID")
			{
				Continuation.f_SetResult(mp_State.m_CommandLineHostID);
				return Continuation;
			}

			Continuation.f_SetException(DMibErrorInstance("Unhandled command in fp_Test_Command: {}"_f << _Command));
			return Continuation;
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
								> [this, Continuation](TCAsyncResult<TCTrustedActorSubscription<CServerInterface>> &&_Subscription)
								{
									if (!_Subscription)
									{
										DMibLogWithCategory(Malterlib/Concurrency/TestDistAppAuthentication, Error, "Failed to subscribe to server: {}", _Subscription.f_GetExceptionStr());
										Continuation.f_SetException(_Subscription);
										return;
									}

									mp_ServerActor = fg_ConstructActor<CClientActor::CServer>(fg_Construct(self), mp_State, fg_Move(*_Subscription));
									Continuation.f_SetResult();
								}
							;
						}
					;
				}
			;

			return fg_Explicit();
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
		}

		TCContinuation<void> fp_StopApp() override
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();

			if (mp_ServerActor)
			{
				DMibLogWithCategory(Mib/Concurrency/CClientActor, Info, "Shutting down server");

				mp_ServerActor->f_Destroy() > [pCanDestroy](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(Mib/Cloud/CClientActor, Error, "Failed to shut down server: {}", _Result.f_GetExceptionStr());
					}
				;
			}
			return pCanDestroy->m_Continuation;
		}

		TCActor<CClientActor::CServer> mp_ServerActor;
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

	struct CCommandLineControlActor : public ICCommandLineControl
	{
		TCContinuation<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			fp_CreateInputActor();

			TCContinuation<TCActorSubscriptionWithID<>> Continuation;
			m_InputActor(&NProcess::CStdInActor::f_RegisterForInputBinary, fg_Move(_fOnInput), _Flags) > Continuation / [=](NConcurrency::CActorSubscription &&_Subscription)
				{
					Continuation.f_SetResult(fg_Move(_Subscription));
				}
			;

			return Continuation;
		}

		TCContinuation<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			fp_CreateInputActor();

			TCContinuation<TCActorSubscriptionWithID<>> Continuation;
			m_InputActor(&NProcess::CStdInActor::f_RegisterForInput, fg_Move(_fOnInput), _Flags) > Continuation / [=](NConcurrency::CActorSubscription &&_Subscription)
				{
					Continuation.f_SetResult(fg_Move(_Subscription));
				}
			;

			return Continuation;
		}

		TCContinuation<NContainer::CSecureByteVector> f_ReadBinary() override
		{
			fp_CreateInputActor();
			return m_InputActor(&NProcess::CStdInActor::f_ReadBinary);
		}

		TCContinuation<NStr::CStrSecure> f_ReadLine() override
		{
			fp_CreateInputActor();
			return m_InputActor(&NProcess::CStdInActor::f_ReadLine);
		}

		TCContinuation<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) override
		{
			fp_CreateInputActor();
			return m_InputActor(&NProcess::CStdInActor::f_ReadPrompt, _Params);
		}

		TCContinuation<void> f_AbortReads() override
		{
			fp_CreateInputActor();
			return m_InputActor(&NProcess::CStdInActor::f_AbortReads);
		}

		TCContinuation<void> f_StdOut(NStr::CStrSecure const &_Output) override
		{
			DMibConOut("CON: {}", _Output);
			return fg_Explicit();
		}

		TCContinuation<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) override
		{
			NSys::fg_ConsoleOutputBinary(_Output);
			return fg_Explicit();
		}

		TCContinuation<void> f_StdErr(NStr::CStrSecure const &_Output) override
		{
			DMibConErrOutRaw(_Output);
			return fg_Explicit();
		}

	private:
		TCActor<NProcess::CStdInActor> m_InputActor;

		TCContinuation<void> fp_Destroy() override
		{
			if (!m_InputActor)
				return fg_Explicit();
			return m_InputActor->f_Destroy();
		}

		void fp_CreateInputActor()
		{
			if (!m_InputActor)
				m_InputActor = fg_Construct();
		}
	};

	struct CCallCount : public CActor
	{
		bool f_AddCall(CStr _Name, bool _Result)
		{
			++m_CallCounts[_Name];
			return _Result;
		}

		void f_Clear()
		{
			m_CallCounts.f_Clear();
		}

		TCMap<CStr, aint> f_GetCounts()
		{
			return m_CallCounts;
		}

		TCMap<CStr, aint> m_CallCounts;
	};

	TCActor<CCallCount> g_CallCount;
		
	struct CAuthenticationActorTestSucceed : public ICDistributedActorTrustManagerAuthenticationActor
	{
		CAuthenticationActorTestSucceed(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager, CStr const &_Name, EAuthenticationFactorCategory _Category)
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

		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> f_AuthenticateCommand
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Description
				, NContainer::TCSet<NStr::CStr> const &_Permissions
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, TCMap<CStr, CAuthenticationData> &&_Factors
			) override
		{
			TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
			if (_Challenge.m_UserID)
			{
				ICDistributedActorAuthenticationHandler::CResponse Response;

				for (auto const &RegisteredFactor : _Factors)
				{
					if (RegisteredFactor.m_Name == m_Name)
					{
						Response.m_Challenge = _Challenge;
						Response.m_Permissions = _Permissions;
						Response.m_ResponseData.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
						Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
						Response.m_FactorName = RegisteredFactor.m_Name;
						break;
					}
				}
				Continuation.f_SetResult(fg_Move(Response));
			}
			else
				Continuation.f_SetResult(ICDistributedActorAuthenticationHandler::CResponse{});
			return Continuation;
		}

		TCContinuation<bool> f_VerifyResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) override
		{
			bool Result = _Response.m_Challenge ==_Challenge && m_Name == CStr(_Response.m_ResponseData.f_GetArray(), _Response.m_ResponseData.f_GetLen());
			return g_CallCount(&CCallCount::f_AddCall, m_Name, Result);
		}

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
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
		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> f_AuthenticateCommand
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Description
				, NContainer::TCSet<NStr::CStr> const &_Permissions
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, NContainer::TCMap<NStr::CStr, CAuthenticationData> &&_Factors
			) override
		{
			TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
			if (_Challenge.m_UserID)
			{
				ICDistributedActorAuthenticationHandler::CResponse Response;

				for (auto const &RegisteredFactor : _Factors)
				{
					if (RegisteredFactor.m_Name == m_Name)
					{
						Response.m_Challenge = _Challenge;
						Response.m_Permissions = _Permissions;
						Response.m_ResponseData.f_Insert((uint8 const *)m_Name.f_GetStr(), m_Name.f_GetLen());
						Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
						Response.m_FactorName = RegisteredFactor.m_Name;
						break;
					}
				}
				Continuation.f_SetResult(fg_Move(Response));
			}
			else
				Continuation.f_SetResult(ICDistributedActorAuthenticationHandler::CResponse{});
			return Continuation;
		}

		TCContinuation<bool> f_VerifyResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) override
		{
			return g_CallCount(&CCallCount::f_AddCall, m_Name, false);
		}

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
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
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert(CClientInterface::mc_pDefaultNamespace);
		Dependencies.m_DistributionManager(&CActorDistributionManager::f_SetSecurity, Security).f_CallSync(g_Timeout);

		TCActor<CDistributedApp_LaunchHelper> LaunchHelper = fg_ConstructActor<CDistributedApp_LaunchHelper>(Dependencies, DTestDistributredAppAuthenticationEnableLogging);
		g_CallCount = fg_ConstructActor<CCallCount>();

		auto Cleanup = g_OnScopeExit > [&]
			{
				LaunchHelper->f_BlockDestroy();
				g_CallCount->f_Destroy() > fg_DiscardResult();
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

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CServerInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ServerHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

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

		DMibCallActor
			(
				TrustManager
				, CDistributedActorTrustManager::f_AllowHostsForNamespace
				, CClientInterface::mc_pDefaultNamespace
				, fg_CreateSet<CStr>(ClientHostID)
				, c_WaitForSubscriptions
			).f_CallSync(g_Timeout)
		;

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
		TCDistributedActor<CCommandLineControlActor> pCommandLineControl = Dependencies.m_DistributionManager->f_ConstructActor<CCommandLineControlActor>();
		CCommandLineControl CommandLineControl;
		CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

		auto ConsoleProperties = NSys::fg_GetConsoleProperties();
		CommandLineControl.m_CommandLineWidth = ConsoleProperties.m_Width;
		CommandLineControl.m_CommandLineHeight = ConsoleProperties.m_Height;

		// Now that we have a default user in the client and a command line control we will run a dummy command. That is the usual way the authentication handler is registered
		// on the server side. We will perform all the tests after the command has returned.
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
					 	, Info.f_GetHost()
					)
				;
			}
		;
		auto CommandLineHostID = ClientLaunch.f_Test_Command("CommandLineHostID", {}).f_CallSync(g_Timeout).f_String();
		NPtr::TCSharedPointer<CCommandLineControl> pCommandLine = fg_Construct(fg_Move(CommandLineControl));
		NEncoding::CEJSON JSON =
		{
			"Command"_= "--test-actor"
			, "ConcurrentLogging"_= true
			, "StdErrLogger"_= false
			, "TraceLogger"_= true,
		};
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


		// Add a permissions to Server that requires authentication
		auto fAddHostUserPermission = [pServerTrust, HostID = ClientHostID, UserID = DefaultUserID](CStr const &_Permission, CPermissionRequirements const &_Methods)
			{
				TCMap<CStr, CPermissionRequirements> Permissions;
				Permissions[_Permission] = _Methods;

				auto &ServerTrust = *pServerTrust;
				DMibCallActor
					(
					 	ServerTrust
						, CDistributedActorTrustManagerInterface::f_AddPermissions
					 	, CDistributedActorTrustManagerInterface::CAddPermissions{{HostID, UserID}, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions}
					).f_CallSync(60.0)
				;
			}
		;
		fAddHostUserPermission("com.malterlib/NoAuth1", {});
		fAddHostUserPermission("com.malterlib/NoAuth2", {});
		fAddHostUserPermission("com.malterlib/Succeed1", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission("com.malterlib/Succeed1a", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission("com.malterlib/Succeed1b", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission("com.malterlib/Succeed1c", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission("com.malterlib/Succeed1d", CPermissionRequirements{{{"Succeed1"}}});
		fAddHostUserPermission("com.malterlib/Succeed2", CPermissionRequirements{{{"Succeed2"}}});
		fAddHostUserPermission("com.malterlib/Succeed3", CPermissionRequirements{{{"Succeed3"}}});
		fAddHostUserPermission("com.malterlib/Succeed12", CPermissionRequirements{{{"Succeed1", "Succeed2"}}});
		fAddHostUserPermission("com.malterlib/Succeed23", CPermissionRequirements{{{"Succeed2", "Succeed3"}}});
		fAddHostUserPermission("com.malterlib/Succeed123", CPermissionRequirements{{{"Succeed1", "Succeed2", "Succeed3"}}});
		fAddHostUserPermission("com.malterlib/Fail1", CPermissionRequirements{{{"Fail1"}}});
		fAddHostUserPermission("com.malterlib/Nonexistant1", CPermissionRequirements{{{"Nonexistant1"}}});
		fAddHostUserPermission("com.malterlib/Succeed123_Fail1", CPermissionRequirements{{{"Succeed1", "Succeed2", "Succeed3", "Fail1"}}});
		fAddHostUserPermission("com.malterlib/Succeed123_Nonexistant1", CPermissionRequirements{{{"Succeed1", "Succeed2", "Succeed3", "Nonexistant1"}}});

		auto fCheckCallCounts = [](TCMap<CStr, aint> &&_Expected, aint _Line)
			{
				auto CallCounts = g_CallCount(&CCallCount::f_GetCounts).f_CallSync(g_Timeout);

				DMibTestPath("Line {}"_f << _Line);
				DMibExpect(CallCounts, ==, _Expected);
				g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
			}
		;

		auto Client = Subscriptions.f_Subscribe<CClientInterface>();
		{
			// f_HasPermission is the simple one - choose the simplest authentication factor the host/user combo has permission for
			DMibTestPath("HasPermission");

			//Basics
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Fail1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Fail1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Nonexistant1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);

			// Multiple authetications needed for single permission
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed123"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1},{"Succeed2", 1}, {"Succeed3", 1}}, __LINE__);
			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed123_Fail1"}).f_CallSync(g_Timeout)));
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed123_Nonexistant1"}).f_CallSync(g_Timeout)));


			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. Ifboth have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall1, TCVector<CStr>{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
		};
		using VQ = TCVector<CPermissionQuery>;
		{
			// Now it starts to get more complex - this is multiple queries like the one above but where each has too succeed
			DMibTestPath("HasPermissions(1)");

			//Basics - should behave as above
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Fail1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Fail1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Nonexistant1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);

			// Multiple authetications needed for single permission
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed123"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1},{"Succeed2", 1}, {"Succeed3", 1}}, __LINE__);
			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed123_Fail1"}}).f_CallSync(g_Timeout)));
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed123_Nonexistant1"}}).f_CallSync(g_Timeout)));

			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. If both have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, VQ{{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}}).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);

			// Now it is time to verify that we can handle the case where we need multiple permissions
			// Select the factor that exists in both sets
			VQ Many1{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed3"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many1).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			VQ Many2{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many2).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed2", 1}}, __LINE__);
			//Select the simplest factor if there are multiple choices
			VQ Many3{{"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Fail1"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Succeed3"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many3).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			// Superset implies subsets
			VQ Many4{{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many4).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}, __LINE__);
			VQ Many5{{"com.malterlib/Succeed1", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many5).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed2", 1}, {"Succeed3", 1}}, __LINE__);
			// 12 is better than 23 (lower category)
			VQ Many6{{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many6).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}, __LINE__);
			// Many permissions use the same authentication factor
			VQ Many7{{"com.malterlib/Succeed1a"}, {"com.malterlib/Succeed1b"}, {"com.malterlib/Succeed1c"}, {"com.malterlib/Succeed1d"}};
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall2, Many7).f_CallSync(g_Timeout)));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
		};
		
		auto fMQ1 = [](CStr _Tag, VQ &&_Query) -> TCMap<CStr, TCVector<CPermissionQuery>>
			{
				TCMap<CStr, TCVector<CPermissionQuery>> Result;
				Result[_Tag] = fg_Move(_Query);
				return Result;
			}
		;
//		auto fMQ2 = [](CStr _Tag1, VQ &&_Query1, CStr _Tag2, VQ &&_Query2) -> TCMap<CStr, TCVector<CPermissionQuery>>
//			{
//				TCMap<CStr, TCVector<CPermissionQuery>> Result;
//				Result[_Tag1] = fg_Move(_Query1);
//				Result[_Tag2] = fg_Move(_Query2);
//				return Result;
//			}
//		;
		auto fMQ3 = [](CStr _Tag1, VQ &&_Query1, CStr _Tag2, VQ &&_Query2, CStr _Tag3, VQ &&_Query3) -> TCMap<CStr, TCVector<CPermissionQuery>>
			{
				TCMap<CStr, TCVector<CPermissionQuery>> Result;
				Result[_Tag1] = fg_Move(_Query1);
				Result[_Tag2] = fg_Move(_Query2);
				Result[_Tag3] = fg_Move(_Query3);
				return Result;
			}
		;
		{
			// Now it starts to get more complex - this is multiple queries like the one above but where each has too succeed
			DMibTestPath("HasPermissions(1)");

			//Basics - should behave as above
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Fail1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Fail1", 1}}, __LINE__);
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Nonexistant1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({}, __LINE__);

			// Multiple authetications needed for single permission
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed123"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1},{"Succeed2", 1}, {"Succeed3", 1}}, __LINE__);
			// Don't know the exact order the authentication actors are called, so we do not check the call counts
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed123_Fail1"}})).f_CallSync(g_Timeout)["Tag"]));
			DMibExpectFalse((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed123_Nonexistant1"}})).f_CallSync(g_Timeout)["Tag"]));

			// Multiple permissions - select the one with simplest authentication.
			// No authentication is better that any authentication. If both have authentication, select the one with the lowest value for Category.
			g_CallCount(&CCallCount::f_Clear).f_CallSync(g_Timeout);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed1", "com.malterlib/NonExistant1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/NonExistant1", "com.malterlib/Succeed1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed1", "com.malterlib/NoAuth1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/NoAuth1", "com.malterlib/Succeed1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, fMQ1("Tag", VQ{{"com.malterlib/Succeed2", "com.malterlib/Succeed1"}})).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);

			// Now it is time to verify that we can handle the case where we need multiple permissions
			// Select the factor that exists in both sets
			auto Many1 = fMQ1("Tag", VQ{{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed3"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many1).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			auto Many2 = fMQ1("Tag", {{"com.malterlib/Succeed1", "com.malterlib/Succeed2"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many2).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed2", 1}}, __LINE__);
			//Select the simplest factor if there are multiple choices
			auto Many3 = fMQ1("Tag", {{"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Fail1"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed2", "com.malterlib/Succeed3"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many3).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);
			// Superset implies subsets
			auto Many4 = fMQ1("Tag", {{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many4).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}, __LINE__);
			auto Many5 = fMQ1("Tag", {{"com.malterlib/Succeed1", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many5).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed2", 1}, {"Succeed3", 1}}, __LINE__);
			// 12 is better than 23 (lower category)
			auto Many6 = fMQ1("Tag", {{"com.malterlib/Succeed12", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed2", "com.malterlib/Succeed3"}, {"com.malterlib/Succeed1", "com.malterlib/Succeed23"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many6).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}}, __LINE__);
			// Many permissions use the same authentication factor
			auto Many7 = fMQ1("Tag", {{"com.malterlib/Succeed1a"}, {"com.malterlib/Succeed1b"}, {"com.malterlib/Succeed1c"}, {"com.malterlib/Succeed1d"}});
			DMibExpectTrue((DMibCallActor(Client, CClientInterface::f_PerformCall3, Many7).f_CallSync(g_Timeout)["Tag"]));
			fCheckCallCounts({{"Succeed1", 1}}, __LINE__);

			// Check that the correct tag fails
			auto Many8 = fMQ3("Tag1", VQ{{"com.malterlib/Succeed1"}}, "Tag2", VQ{{"com.malterlib/Succeed2"}}, "Tag3", VQ{{"com.malterlib/Fail1"}});
			auto Result1 = DMibCallActor(Client, CClientInterface::f_PerformCall3, Many8).f_CallSync(g_Timeout);
			DMibExpectTrue(Result1["Tag1"]);
			DMibExpectTrue(Result1["Tag2"]);
			DMibExpectFalse(Result1["Tag3"]);
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}, {"Fail1", 1}}, __LINE__);
			auto Many9 = fMQ3("Tag1", VQ{{"com.malterlib/Succeed1"}}, "Tag2", VQ{{"com.malterlib/Succeed2"}, {"com.malterlib/Fail1"}}, "Tag3", VQ{{"com.malterlib/Fail1"}});
			auto Result2 = DMibCallActor(Client, CClientInterface::f_PerformCall3, Many9).f_CallSync(g_Timeout);
			DMibExpectTrue(Result2["Tag1"]);
			DMibExpectFalse(Result2["Tag2"]);
			DMibExpectFalse(Result2["Tag3"]);
			fCheckCallCounts({{"Succeed1", 1}, {"Succeed2", 1}, {"Fail1", 1}}, __LINE__);
		};

	}
};

DMibTestRegister(CDistributedAppAuthentication_Tests, Malterlib::Concurrency);
