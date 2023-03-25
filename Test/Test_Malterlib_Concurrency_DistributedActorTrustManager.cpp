// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Concurrency/DistributedTrustTestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include <Mib/Network/SSL>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Concurrency/DistributedActorTrustManagerProxy>
#include <Mib/Concurrency/DistributedApp> // For CCommandLineControl
#include <Mib/Cryptography/Certificate>

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NStr;
using namespace NMib::NCryptography;
using namespace NMib::NAtomic;
using namespace NMib::NFunction;
using namespace NMib::NTest;
using namespace NMib::NStorage;
using namespace NMib::NNetwork;

static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;

namespace NTestTrustManager
{
	struct CAuthenticationActorTestSucceed : public ICDistributedActorTrustManagerAuthenticationActor
	{
		CAuthenticationActorTestSucceed(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager, CStr const &_Name)
			: m_TrustManager(_TrustManager)
			, m_Name(_Name)
		{
		}
		virtual ~CAuthenticationActorTestSucceed() = default;

		TCFuture<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = EAuthenticationFactorCategory_None;
			Result.m_Name = m_Name;
			Result.m_PublicData["Public"] = "PublicData1";
			Result.m_PrivateData["Private"] = "PrivateData1";

			co_return fg_Move(Result);
		}

		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				TCSharedPointer<CCommandLineControl> const &_pCommandLine
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
			}

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
		CStr m_Name;
	};

	TCFuture<void> DMibWorkaroundUBSanSectionErrors fg_TestIt(TCDistributedActorInterface<CAuthenticationActorTestSucceed> &&_Value)
	{
		co_return {};
	}

	struct CTestInterfaceConversion : public CActor
	{
		TCFuture<void> f_TestIt(TCDistributedActorInterface<CAuthenticationActorTestSucceed> &&_Value)
		{
			co_return {};
		}

		TCFuture<void> f_TestIt2(TCDistributedActor<CAuthenticationActorTestSucceed> &&_Value)
		{
			co_await self(&CTestInterfaceConversion::f_TestIt, fg_TempCopy(_Value));

			co_await fg_CallSafe(this, &CTestInterfaceConversion::f_TestIt, fg_TempCopy(_Value));

			co_return {};
		}
	};

	struct CAuthenticationActorFail : public ICDistributedActorTrustManagerAuthenticationActor
	{
		CAuthenticationActorFail(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager, CStr const &_Name)
			: m_TrustManager(_TrustManager)
			, m_Name(_Name)
		{
		}
		virtual ~CAuthenticationActorFail() = default;

		TCFuture<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, TCSharedPointer<CCommandLineControl> const &_pCommandLine) override
		{
			CAuthenticationData Result;
			Result.m_Category = EAuthenticationFactorCategory_None;
			Result.m_Name = "Test2";
			Result.m_PublicData["Public"] = "PublicData2";
			Result.m_PrivateData["Private"] = "PrivateData2";

			co_return fg_Move(Result);
		}

		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				TCSharedPointer<CCommandLineControl> const &_pCommandLine
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
		CStr m_Name;
	};

	class CDistributedActorTrustManagerAuthenticationActorFactoryTest1 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorTestSucceed>(_TrustManager, "Test1"), EAuthenticationFactorCategory_None};
		}
	};

	class CDistributedActorTrustManagerAuthenticationActorFactoryTest2 : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override
		{
			return CAuthenticationActorInfo{fg_ConstructActor<CAuthenticationActorFail>(_TrustManager, "Test2"), EAuthenticationFactorCategory_None};
		}
	};

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryTest1);
	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryTest2);

	class CDistributedActorTrustManager_Tests : public NMib::NTest::CTest
	{
	public:
		struct CTestActor : public CActor
		{
			enum : uint32
			{
				EProtocolVersion_Min = 0x101
				, EProtocolVersion_Current = 0x101
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
				EProtocolVersion_Min = 0x101
				, EProtocolVersion_Current = 0x101
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
				EProtocolVersion_Min = 0x101
				, EProtocolVersion_Current = 0x101
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
			auto f_CreateServerTrustManager(CDistributedActorTrustManager::COptions &&_Options = {}) const
			{
				CDistributedActorTrustManager::COptions Options(fg_Move(_Options));
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNetwork::ENetFlag_None;
				Options.m_FriendlyName = "TestServer";
				Options.m_InitialConnectionTimeout = g_Timeout / 2;
				Options.m_bRetryOnListenFailureDuringInit = false;

				return fg_ConstructActor<CDistributedActorTrustManager>(m_ServerDatabase, fg_Move(Options));
			}

			auto f_CreateClientTrustManager(CDistributedActorTrustManager::COptions &&_Options = {}) const
			{
				CDistributedActorTrustManager::COptions Options(fg_Move(_Options));
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNetwork::ENetFlag_None;
				Options.m_FriendlyName = "TestClient";
				Options.m_InitialConnectionTimeout = g_Timeout / 2;
				Options.m_bRetryOnListenFailureDuringInit = false;

				return fg_ConstructActor<CDistributedActorTrustManager>(m_ClientDatabase, fg_Move(Options));
			}

			CState
				(
					TCSharedPointer<CDefaultRunLoop> const &_pRunLoop
					, TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (CStr const &_Name)> const &_fDatabaseFactory
					, TCFunction<void ()> const &_fCleanup
					, CStr const &_NameDistinguish = {}
				)
				: m_fCleanup(_fCleanup)
				, m_pRunLoop(_pRunLoop)
			{
				if (_fCleanup)
					_fCleanup();
				m_ServerDatabase = _fDatabaseFactory("Server" + _NameDistinguish);
				m_ClientDatabase = _fDatabaseFactory("Client" + _NameDistinguish);
			}
			~CState()
			{
				m_ServerDatabase->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
				m_ClientDatabase->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
				if (m_fCleanup)
					m_fCleanup();
			}


			TCActor<ICDistributedActorTrustManagerDatabase> m_ServerDatabase;
			TCActor<ICDistributedActorTrustManagerDatabase> m_ClientDatabase;
			TCFunction<void ()> m_fCleanup;
			TCSharedPointer<CDefaultRunLoop> m_pRunLoop;
		};

		static CStr fg_GetSocketUrl(CStr const &_SocketName)
		{
			CStr Path = NNetwork::fg_GetSafeUnixSocketPath(NFile::CFile::fs_GetProgramDirectory() / ("Sockets/TrustManagerPermissions/{}.sock"_f << _SocketName));
			fg_TestAddCleanupPath(Path);
			return NStr::CStr::CFormat("wss://[UNIX(666):{}]/") << Path;
		}

		struct CPermissionTestState
		{
			CPermissionTestState(CState &_State, NStr::CStr const &_SocketName)
				: m_SocketUrl(fg_GetSocketUrl(_SocketName))
				, m_ServerTrustManager{f_CreateServerTrustManager(_State)}
				, m_ClientTrustManager{f_CreateClientTrustManager(_State)}
				, m_ServerHelper{f_InitServerTrustManager()}
				, m_ClientHelper{f_InitClientTrustManager()}
				, m_ServerHostInfo({}, {}, "", CHostInfo{m_ServerHostID, m_ServerHostID}, "", 0, "TBD", "Test", nullptr)
				, m_pRunLoop(_State.m_pRunLoop)
			{
				m_ExpectedPermissions[CPermissionIdentifiers::fs_GetKeyFromFileNameOld(m_ServerHostID)]["com.malterlib/Test"];
				m_ExpectedEnumPermissions["com.malterlib/Test"][CPermissionIdentifiers::fs_GetKeyFromFileNameOld(m_ServerHostID)].m_HostInfo = CHostInfo(m_ServerHostID, "TestServer");
				m_ExpectedEnumPermissionsNoHostInfo["com.malterlib/Test"][CPermissionIdentifiers::fs_GetKeyFromFileNameOld(m_ServerHostID)];
			}

			TCActor<CDistributedActorTrustManager> f_CreateServerTrustManager(CState &_State)
			{
				TCActor<CDistributedActorTrustManager> TrustManager = _State.f_CreateServerTrustManager();
				TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(m_pRunLoop, g_Timeout);
				return TrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_CreateClientTrustManager(CState &_State)
			{
				TCActor<CDistributedActorTrustManager> TrustManager = _State.f_CreateClientTrustManager();
				TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(m_pRunLoop, g_Timeout);
				return TrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_InitServerTrustManager()
			{
				m_ServerHostID = m_ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(m_pRunLoop, g_Timeout);

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = m_SocketUrl;
				if (!m_ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(m_pRunLoop, g_Timeout))
					m_ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(m_pRunLoop, g_Timeout);

				return m_ServerTrustManager;
			}

			TCActor<CDistributedActorTrustManager> f_InitClientTrustManager()
			{
				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = m_SocketUrl;

				if (!m_ClientTrustManager(&CDistributedActorTrustManager::f_HasClientConnection, ServerAddress).f_CallSync(m_pRunLoop, g_Timeout))
				{
					auto TrustTicket = m_ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(m_pRunLoop, g_Timeout);
					m_ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(m_pRunLoop, g_Timeout);
				}

				return m_ClientTrustManager;
			}

			~CPermissionTestState()
			{
				m_ClientTrustManager->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
				m_ServerTrustManager->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
			}

			template <typename tf_CKey, typename tf_CValue, typename... tf_CParams>
			void fg_CreateMapHelper(TCMap<tf_CKey, tf_CValue> &_Return)
			{
			}

			template <typename tf_CKey, typename tf_CValue, typename... tf_CParams>
			void fg_CreateMapHelper(TCMap<tf_CKey, tf_CValue> &_Return, tf_CKey &&_First, tf_CParams && ...p_Params)
			{
				_Return[fg_Forward<tf_CKey>(_First)];
				fg_CreateMapHelper<tf_CKey, tf_CValue>(_Return, fg_Forward<tf_CParams>(p_Params)...);
			}

			template <typename tf_CKey, typename tf_CValue, typename... tf_CParams>
			TCMap<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CKey>::CType, typename NTraits::TCRemoveReferenceAndQualifiers<tf_CValue>::CType> fg_CreateMap
				(
					tf_CKey && _First
					, tf_CParams && ...p_Params
				)
			{
				TCMap<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CKey>::CType, typename NTraits::TCRemoveReferenceAndQualifiers<tf_CValue>::CType> Return;
				fg_CreateMapHelper<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CKey>::CType, typename NTraits::TCRemoveReferenceAndQualifiers<tf_CValue>::CType>
					(
						Return
						, fg_Forward<tf_CKey>(_First)
						, fg_Forward<tf_CParams>(p_Params)...
					)
				;
				return Return;
			}

			template <typename tf_CKey, typename tf_CValue, typename... tf_CParams>
			TCMap<tf_CKey, tf_CValue> fg_CreateMap(tf_CParams && ...p_Params)
			{
				TCMap<tf_CKey, tf_CValue> Return;
				fg_CreateMapHelper<tf_CKey, tf_CValue>(Return, fg_Forward<tf_CParams>(p_Params)...);
				return Return;
			}

			static TCMap<CStr, CPermissionRequirements> fs_CreateMap(CStr const &_Permission, CPermissionRequirements const &_Methods)
			{
				TCMap<CStr, CPermissionRequirements> Result;
				Result[_Permission] = _Methods;
				return Result;
			}

			template <typename ...tfp_CPermission>
			void f_AddPermissions(tfp_CPermission &&...p_Permissions)
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_AddPermissions
						, CPermissionIdentifiers{m_ServerHostID, ""}
						, fg_CreateMap<CStr, CPermissionRequirements>(p_Permissions...)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			template <typename ...tfp_CPermission>
			void f_RemovePermissions(tfp_CPermission &&...p_Permissions)
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_RemovePermissions
						, CPermissionIdentifiers{m_ServerHostID, ""}
						, fg_CreateSet<CStr>(p_Permissions...)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			void f_AddUserPermission(CStr const &_UserID, CStr const &_Permission, CPermissionRequirements const &_Methods = {})
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_AddPermissions
						, CPermissionIdentifiers{"", _UserID}
						, fs_CreateMap(_Permission, _Methods)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			void f_RemoveUserPermission(CStr const &_UserID, CStr const &_Permission)
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_RemovePermissions
						, CPermissionIdentifiers{"", _UserID}
						, fg_CreateSet<CStr>(_Permission)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			void f_AddHostUserPermission(CStr const &_UserID, CStr const &_Permission, CPermissionRequirements const &_Methods = {})
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_AddPermissions
						, CPermissionIdentifiers{m_ServerHostID, _UserID}
						, fs_CreateMap(_Permission, _Methods)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			void f_RemoveHostUserPermission(CStr const &_UserID, CStr const &_Permission)
			{
				m_ClientTrustManager
					(
						&CDistributedActorTrustManager::f_RemovePermissions
						, CPermissionIdentifiers{m_ServerHostID, _UserID}
						, fg_CreateSet<CStr>(_Permission)
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					).f_CallSync(m_pRunLoop, g_Timeout)
				;
			}

			NStr::CStr m_SocketUrl;

			TCSharedPointer<CDefaultRunLoop> m_pRunLoop;

			TCActor<CDistributedActorTrustManager> m_ServerTrustManager;
			TCActor<CDistributedActorTrustManager> m_ClientTrustManager;
			CStr m_ServerHostID;
			CDistributedActorTestHelper m_ServerHelper;
			CDistributedActorTestHelper m_ClientHelper;
			CCallingHostInfo m_ServerHostInfo;

			TCActor<> m_TestActor = fg_ConcurrentActor();

			NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> m_ExpectedPermissionsEmpty;
			NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> m_ExpectedPermissions;

			NContainer::TCMap<NStr::CStr, NContainer::TCMap<CPermissionIdentifiers, CDistributedActorTrustManagerInterface::CPermissionInfo>> m_ExpectedEnumPermissionsEmpty;
			NContainer::TCMap<NStr::CStr, NContainer::TCMap<CPermissionIdentifiers, CDistributedActorTrustManagerInterface::CPermissionInfo>> m_ExpectedEnumPermissions;
			NContainer::TCMap<NStr::CStr, NContainer::TCMap<CPermissionIdentifiers, CDistributedActorTrustManagerInterface::CPermissionInfo>> m_ExpectedEnumPermissionsNoHostInfo;
		};

		void fp_DoTests(TCActor<CDistributedActorTrustManager> const &_ServerTrustManager, TCActor<CDistributedActorTrustManager> const &_ClientTrustManager, TCSharedPointer<CDefaultRunLoop> const &_pRunLoop)
		{
			CDistributedActorTestHelper ServerHelper{_ServerTrustManager};
			CDistributedActorTestHelper ClientHelper{_ClientTrustManager};

			ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
			CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

			TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

			Actor.f_CallActor(&CTestActor::f_TestVoid)().f_CallSync(_pRunLoop, g_Timeout);

			uint32 Result = Actor.f_CallActor(&CTestActor::f_Test)().f_CallSync(_pRunLoop, g_Timeout);
			DMibExpect(Result, ==, 5);
		}

		bool fp_WaitForCondition(TCFunction<bool ()> const &_fPredicate)
		{
			bool bTimedOut = false;

			NTime::CClock Clock;
			Clock.f_Start();

			while (!_fPredicate())
			{
				NSys::fg_Thread_Sleep(0.01f);
				if (Clock.f_GetTime() > g_Timeout / 2)
				{
					bTimedOut = true;
					break;
				}
			}

			return bTimedOut;
		}

		void fp_DoBasicTests
			(
				TCFunction<TCActor<ICDistributedActorTrustManagerDatabase> (CStr const &_Name)> const &_fDatabaseFactory
				, TCFunction<void ()> const &_fCleanup
			)
		{
			{
				DMibTestPath("Basic");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				{
					DMibTestPath("Initial");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					CStr ServerHostID = ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
					CStr ClientHostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);

					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = fg_GetSocketUrl("TrustBasic");
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout));

					auto AllListens = ServerTrustManager(&CDistributedActorTrustManager::f_EnumListens).f_CallSync(pRunLoop, g_Timeout);
					NContainer::TCSet<CDistributedActorTrustManager_Address> ExpectedListens;
					ExpectedListens[ServerAddress];
					DMibExpect(AllListens, ==, ExpectedListens);

					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);

					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(ClientTrustManager(&CDistributedActorTrustManager::f_HasClientConnection, TrustTicket.m_Ticket.m_ServerAddress).f_CallSync(pRunLoop, g_Timeout));

					auto AllClientConnections = ClientTrustManager(&CDistributedActorTrustManager::f_EnumClientConnections).f_CallSync(pRunLoop, g_Timeout);
					NContainer::TCMap<CDistributedActorTrustManager_Address, CDistributedActorTrustManager::CClientConnectionInfo> ExpectedClientConnections;
					{
						auto &ExpectedHostInfo = ExpectedClientConnections[TrustTicket.m_Ticket.m_ServerAddress];
						ExpectedHostInfo.m_HostInfo.m_FriendlyName = "TestServer";
						ExpectedHostInfo.m_HostInfo.m_HostID = ServerHostID;
						ExpectedHostInfo.m_ConnectionConcurrency = -1;
					}
					DMibExpect(AllClientConnections, ==, ExpectedClientConnections);

					fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);

					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasClient, ClientHostID).f_CallSync(pRunLoop, g_Timeout));

					auto AllClients = ServerTrustManager(&CDistributedActorTrustManager::f_EnumClients).f_CallSync(pRunLoop, g_Timeout);
					NContainer::TCMap<CStr, CHostInfo> ExpectedClients;
					{
						auto &ExpectedHostInfo = ExpectedClients[ClientHostID];
						ExpectedHostInfo.m_FriendlyName = "TestClient";
						ExpectedHostInfo.m_HostID = ClientHostID;
					}
					DMibExpect(AllClients, ==, ExpectedClients);

					ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				}

				{
					DMibTestPath("From database");

					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = fg_GetSocketUrl("TrustBasic");
					DMibExpectTrue(ServerTrustManager(&CDistributedActorTrustManager::f_HasListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout));

					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);

					ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				}

				{
					DMibTestPath("Change listen");
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = fg_GetSocketUrl("TrustBasicChanged");
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

					CDistributedActorTrustManager_Address OldServerAddress;
					OldServerAddress.m_URL = fg_GetSocketUrl("TrustBasic");

					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveListen, OldServerAddress).f_CallSync(pRunLoop, g_Timeout);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					ClientTrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, ServerAddress, -1).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, OldServerAddress, false).f_CallSync(pRunLoop, g_Timeout);

					fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);

					ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				}

				{
					DMibTestPath("Remove client");
					CStr HostID;
					{
						TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();
						HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
						ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();

					ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(pRunLoop, g_Timeout);;

					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

					auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(pRunLoop, g_Timeout);

					DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
					DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());

					auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();

					DMibAssertTrue(Address.m_States.f_GetLen() == 1);

					auto &ConcurrentConnectionState = Address.m_States[0];

					DMibExpectFalse(ConcurrentConnectionState.m_bConnected);
					DMibExpect(ConcurrentConnectionState.m_Error, !=, "");

					DMibExpectExceptionType(fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop), NException::CException);

					ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				}
			}
			{
				DMibTestPath("Remove client while connected");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustRemoveClient");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);

				ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);

				fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);

				auto HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
				ServerTrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID).f_CallSync(pRunLoop, g_Timeout);

				auto ConnectionState = ClientTrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(pRunLoop, g_Timeout);

				DMibAssertFalse(ConnectionState.m_Hosts.f_IsEmpty());
				DMibAssertFalse(ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_IsEmpty());

				auto &Address = *ConnectionState.m_Hosts.f_FindAny()->m_Addresses.f_FindAny();

				DMibAssertTrue(Address.m_States.f_GetLen() == 1);

				auto &ConcurrentConnectionState = Address.m_States[0];

				DMibExpectFalse(ConcurrentConnectionState.m_bConnected);
				DMibExpect(ConcurrentConnectionState.m_Error, !=, "");

				DMibExpectExceptionType(fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop), NException::CException);

				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
			{
				DMibTestPath("Disconnects");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustRemoveDisconnects");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
				CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

				TCActor<CSeparateThreadActor> DispatchActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Dispatch"));

				CStr DispatchError;
				TCAtomic<mint> nCalls = 0;
				TCAtomic<bool> bAbort = false;
				auto Cleanup = g_OnScopeExit / [&]
					{
						bAbort = true;
					}
				;
				fg_Dispatch
					(
						DispatchActor
						, [&]
						{
							while (!bAbort.f_Load())
							{
								try
								{
									Actor.f_CallActor(&CTestActor::f_Test)().f_CallSync(g_Timeout / 2);
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

				{
					mint ExpectedCalls = nCalls.f_Load() + 10;
					bool bTimedOutPreDisconnect = fp_WaitForCondition
						(
							[&]
							{
								return nCalls.f_Load() >= ExpectedCalls;
							}
						)
					;

					DMibExpectFalse(bTimedOutPreDisconnect);
				}


				bool bTimedOut = false;

				for (mint i = 0; i < 4 && !bTimedOut; ++i)
				{
					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, ServerAddress, true).f_CallSync(pRunLoop, g_Timeout);

					mint ExpectedCalls = nCalls.f_Load() + 10;

					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);

					bTimedOut = fp_WaitForCondition
						(
							[&]
							{
								return nCalls.f_Load() >= ExpectedCalls;
							}
						)
					;
				}

				bAbort = true;
				DispatchActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());

				DMibExpect(DispatchError, ==, "");
				DMibExpect(nCalls, >, 1u);
				DMibExpectFalse(bTimedOut);

				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());

			}
			{
				DMibTestPath("Broken Connections");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustBrokenConnections");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, 10).f_CallSync(pRunLoop, g_Timeout);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
				CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

				TCActor<CSeparateThreadActor> DispatchActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Dispatch"));

				ClientTrustManager(&CDistributedActorTrustManager::f_Debug_BreakClientConnection, ServerAddress, 0.1, ESocketDebugFlag_StopProcessing).f_CallSync(pRunLoop, g_Timeout);
				ServerTrustManager(&CDistributedActorTrustManager::f_Debug_BreakListenConnections, ServerAddress, 0.1, ESocketDebugFlag_StopProcessing).f_CallSync(pRunLoop, g_Timeout);

				CStr DispatchError;
				TCAtomic<mint> nCalls = 0;
				TCAtomic<bool> bAbort = false;
				auto Cleanup = g_OnScopeExit / [&]
					{
						bAbort = true;
					}
				;
				fg_Dispatch
					(
						DispatchActor
						, [&]
						{
							while (!bAbort.f_Load())
							{
								try
								{
									Actor.f_CallActor(&CTestActor::f_Test)().f_CallSync(g_Timeout / 2);
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

				bool bTimedOut = false;

				for (mint i = 0; i < 2 && !bTimedOut; ++i)
				{
					ClientTrustManager
						(
							&CDistributedActorTrustManager::f_Debug_BreakClientConnection
							, ServerAddress, fp64(0.05) + NMisc::fg_GetRandomFloat() * fp64(0.1)
							, ESocketDebugFlag_StopProcessing
						).f_CallSync(pRunLoop, g_Timeout)
					;
					ServerTrustManager
						(
							&CDistributedActorTrustManager::f_Debug_BreakListenConnections
							, ServerAddress
							, fp64(0.05) + NMisc::fg_GetRandomFloat() * fp64(0.1)
							, ESocketDebugFlag_StopProcessing
						).f_CallSync(pRunLoop, g_Timeout)
					;

					mint ExpectedCalls = nCalls.f_Load() + 2;
					bTimedOut = fp_WaitForCondition
						(
							[&]
							{
								return nCalls.f_Load() >= ExpectedCalls;
							}
						)
					;
				}

				bAbort = true;
				DispatchActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());

				DMibExpect(DispatchError, ==, "");
				DMibExpect(nCalls, >, 1u);
				DMibExpectFalse(bTimedOut);

				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
			{
				DMibTestPath("Host Cleanup");
				for (mint i = 0; i < 2; ++i)
				{
					DMibTestPath(i == 0 ? "Server" : "Client");

					TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
					auto CleanupRunLoop = g_OnScopeExit / [&]
						{
							while (pRunLoop->m_RefCount.f_Get() > 0)
								pRunLoop->f_WaitOnceTimeout(0.1);
						}
					;
					TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
					auto CleanupHelperActor = g_OnScopeExit / [&]
						{
							HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
						}
					;
					CCurrentlyProcessingActorScope CurrentActor{HelperActor};

					CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

					CDistributedActorTrustManager::COptions ServerOptions;
					CDistributedActorTrustManager::COptions ClientOptions;

					if (i == 0)
						ServerOptions.m_HostTimeout = ServerOptions.m_HostDaemonTimeout = 0.5;
					else
						ClientOptions.m_HostTimeout = ClientOptions.m_HostDaemonTimeout = 0.5;

					TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager(fg_Move(ServerOptions));
					TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager(fg_Move(ClientOptions));

					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = fg_GetSocketUrl("TrustHostCleanup");
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

					CHostInfo HostInfo;
					{
						auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
						HostInfo = ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
					}

					CDistributedActorTestHelper ServerHelper{ServerTrustManager};
					CDistributedActorTestHelper ClientHelper{ClientTrustManager};

					ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

					TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

					TCActor<CSeparateThreadActor> DispatchActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Dispatch"));

					CStr DispatchError;
					TCAtomic<mint> nCalls = 0;
					TCAtomic<bool> bAbort = false;
					TCAtomic<bool> bHasError = false;
					auto Cleanup = g_OnScopeExit / [&]
						{
							bAbort = true;
						}
					;
					fg_Dispatch
						(
							DispatchActor
							, [&]
							{
								while (!bAbort.f_Load())
								{
									try
									{
										Actor.f_CallActor(&CTestActor::f_Test)().f_CallSync(g_Timeout / 6);
										++nCalls;
									}
									catch (NException::CException const &_Exception)
									{
										DispatchError = _Exception.f_GetErrorStr();
										bHasError = true;
										break;
									}
								}
							}
						)
						> fg_DiscardResult();
					;

					mint ExpectedCalls = nCalls.f_Load() + 2;
					bool bTimedOut = fp_WaitForCondition
						(
							[&]
							{
								return nCalls.f_Load() >= ExpectedCalls;
							}
						)
					;
					ServerTrustManager(&CDistributedActorTrustManager::f_Debug_SetListenServerBroken, ServerAddress, true).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_Debug_BreakClientConnection, ServerAddress, 0.1, ESocketDebugFlag_StopProcessing).f_CallSync(pRunLoop, g_Timeout);
					ServerTrustManager(&CDistributedActorTrustManager::f_Debug_BreakListenConnections, ServerAddress, 0.1, ESocketDebugFlag_StopProcessing).f_CallSync(pRunLoop, g_Timeout);

					bool bTimedOut2;
					if (i == 1)
					{
						bTimedOut2 = fp_WaitForCondition
							(
								[&]
								{
									return bHasError.f_Load();
								}
							)
						;
					}
					else
					{
						bTimedOut2 = fp_WaitForCondition
							(
								[&]
								{
									auto DebugStats = ServerTrustManager(&CDistributedActorTrustManager::f_GetConnectionsDebugStats).f_CallSync(pRunLoop, g_Timeout);
									return DebugStats.m_DebugStats.m_Hosts.f_IsEmpty();
								}
							)
						;
					}

					ServerTrustManager(&CDistributedActorTrustManager::f_Debug_SetListenServerBroken, ServerAddress, false).f_CallSync(pRunLoop, g_Timeout);

					if (i == 0)
					{
						bTimedOut2 = fp_WaitForCondition
							(
								[&]
								{
									return bHasError.f_Load();
								}
							)
						;
					}

					bAbort = true;
					DispatchActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());


					DMibTest
						(
							DMibExpr(DispatchError) == DMibExpr(("Remote host '{} [TestServer]' no longer running: Host inactivity (0.5 s)"_f << HostInfo.m_HostID).f_GetStr())
							|| DMibExpr(DispatchError) == DMibExpr(("Remote host '{} [TestServer]' no longer running: Identify with new execution ID"_f << HostInfo.m_HostID).f_GetStr())
						)
					;
					DMibExpect(nCalls, >, 1u);
					DMibExpectFalse(bTimedOut);
					DMibExpectFalse(bTimedOut2);

					ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				}
			}
			{
				DMibTestPath("Security");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustSecurity");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);

					ClientTrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, ServerAddress, true).f_CallSync(pRunLoop, g_Timeout);

					// Check that you cannot reuse old ticket
					DMibExpectExceptionType(ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout), NException::CException);

					// Check that you can add trust to server, even with old client in database
					TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
				}

				{
					DMibTestPath("Before fraudulent try");
					fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);
				}

				TCActor<ICDistributedActorTrustManagerDatabase> Client2Database = _fDatabaseFactory("Client2");
				Client2Database(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig).f_CallSync(pRunLoop, g_Timeout);

				{
					NDistributedActorTrustManagerDatabase::CBasicConfig BasicConfig;
					BasicConfig.m_HostID = ClientTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);

					CCertificateOptions Options;
					Options.m_KeySetting = CDistributedActorTestKeySettings{};
					Options.m_CommonName = fg_Format("Malterlib Distributed Actors Root - {}", BasicConfig.m_HostID).f_Left(64);
					auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
					Extension.m_bCritical = false;
					Extension.m_Value = BasicConfig.m_HostID;

					CCertificateSignOptions SignOptions;
					SignOptions.m_Days = 100*365;

					CCertificate::fs_GenerateSelfSignedCertAndKey(Options, BasicConfig.m_CACertificate, BasicConfig.m_CAPrivateKey, SignOptions);

					Client2Database(&ICDistributedActorTrustManagerDatabase::f_SetBasicConfig, BasicConfig).f_CallSync(pRunLoop, g_Timeout);
				}

				CDistributedActorTrustManager::COptions Options;
				Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
					{
						return fg_ConstructActor<CActorDistributionManager>(_Settings);
					}
				;
				Options.m_KeySetting = CDistributedActorTestKeySettings{};
				Options.m_ListenFlags = NNetwork::ENetFlag_None;
				Options.m_FriendlyName = "ClientTrustManager2";

				TCActor<CDistributedActorTrustManager> Client2TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(Client2Database, fg_Move(Options));

				auto TrustTicket2 = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
				// Test fraudulent client add
				DMibExpectExceptionType(Client2TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket2.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout), NException::CException);

				{
					DMibTestPath("After fraudulent try");
					fp_DoTests(ServerTrustManager, ClientTrustManager, pRunLoop);
				}
				Client2TrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				Client2Database->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
			{
				DMibTestPath("Multiple Enclaves");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustMultipleEnclaves");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
				}
				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
				CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");
				TCDistributedActor<CTestActor> Actor = ClientHelper.f_GetRemoteActor<CTestActor>(Subscription);

				TCVector<TCActor<CDistributedActorTrustManager>> ClientTrustManagers;
				for (mint i = 0; i < 128; ++i)
				{
					CDistributedActorTrustManager::COptions Options;
					Options.m_Enclave = NCryptography::fg_RandomID();
					ClientTrustManagers.f_Insert() = State.f_CreateClientTrustManager(fg_Move(Options));
				}

				TCLinkedList<CDistributedActorTestHelper> ClientHelpers;
				TCLinkedList<TCDistributedActor<CTestActor>> ClientActors;

				for (auto &ClientManager : ClientTrustManagers)
				{
					auto &ClientHelper = ClientHelpers.f_Insert(fg_Construct(ClientManager));

					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");
					ClientActors.f_Insert(ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
				}

				TCActorResultVector<uint32> Results;

				Actor.f_CallActor(&CTestActor::f_Test)() > Results.f_AddResult();

				for (auto &ClientActor : ClientActors)
					ClientActor.f_CallActor(&CTestActor::f_Test)() > Results.f_AddResult();

				auto ResultsVector = Results.f_GetResults().f_CallSync(g_Timeout / 6);
				for (auto &Result : ResultsVector)
					DMibExpect(*Result, ==, 5)(ETestFlag_Aggregated);

				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ClientActors.f_Clear();
				ClientHelpers.f_Clear();
				for (auto &ClientManager : ClientTrustManagers)
					ClientManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
			static constexpr auto c_WaitForSubscriptions = EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions;
			{
				DMibTestPath("Subscriptions");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				TCActor<CDistributedActorTrustManager> ServerTrustManager = State.f_CreateServerTrustManager();
				TCActor<CDistributedActorTrustManager> ClientTrustManager = State.f_CreateClientTrustManager();

				CStr ServerHostID = ServerTrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);

				CDistributedActorTrustManager_Address ServerAddress;
				ServerAddress.m_URL = fg_GetSocketUrl("TrustSubscriptions");
				ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

				{
					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
				}

				CDistributedActorTestHelper ServerHelper{ServerTrustManager};
				CDistributedActorTestHelper ClientHelper{ClientTrustManager};

				TCSet<CStr> ServerHosts;
				ServerHosts[ServerHostID];

				TCSet<CStr> Published;

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
							auto Subscription = ClientTrustManager
								(
									&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
									, _Namespace
									, HelperActor
									, 0
									, TCLimitsInt<uint32>::mc_Max
								)
								.f_CallSync(pRunLoop, g_Timeout)
							;

							Subscription.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
						}
					;

					DMibExpectException(fSubscribeTrusted("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fSubscribeTrusted("Test/55");

					auto fAllowNamespace = [&](CStr const &_Namespace)
						{
							ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
						}
					;
					auto fDisallowNamespace = [&](CStr const &_Namespace)
						{
							ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
						}
					;

					DMibExpectException(fAllowNamespace("Test$55"), DMibErrorInstance("Invalid namespace name"));
					DMibExpectException(fDisallowNamespace("Test$55"), DMibErrorInstance("Invalid namespace name"));
					fAllowNamespace("Test/55");
					fDisallowNamespace("Test/55");

					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("General");
					auto TrustedSubscription = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
							, "com.malterlib/Test"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;
					{
						DMibTestPath("Before publish");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
					}
					Published[ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test")];
					CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test");

					// Make sure that queue has been processed on test actor
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
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

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After allow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After allow again");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 1);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After disallow");
						DMibAssert(TrustedSubscription.m_Actors.f_GetLen(), ==, 0);
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						NContainer::TCMap<CStr, CDistributedActorTrustManager::CNamespacePermissions> ExpectedAllowedEnum;
						{
							auto &Host = ExpectedAllowedEnum["com.malterlib/Test"].m_DisallowedHosts[ServerHostID];
							Host.m_HostID = ServerHostID;
							Host.m_FriendlyName = "TestServer";
						}
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					TrustedSubscription.f_Destroy().f_CallSync(pRunLoop, g_Timeout);

					{
						DMibTestPath("After clear subscription");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnumEmpty);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After allow without subscription");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					{
						TCSet<CStr> NonExistantHosts;
						NonExistantHosts["NonHost"];
						ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", NonExistantHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					}
					{
						DMibTestPath("After nonexistant disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnum);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					 // Test disallow without namespace
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After double disallow");
						auto AllowedEnum = ClientTrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, true).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(AllowedEnum, ==, ExpectedAllowedEnumEmpty);
					}
					TrustedSubscription.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Double");
					Published[ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test")];
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);

					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto TrustedSubscription0 = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
							, "com.malterlib/Test"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;
					auto TrustedSubscription1 = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
							, "com.malterlib/Test"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						).f_CallSync(pRunLoop, g_Timeout)
					;
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
						DMibExpect(TrustedSubscription1.m_Actors.f_GetLen(), ==, 2);
					}
					TrustedSubscription0.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
					TrustedSubscription1.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}
				auto fWaitForSubscribed = [&](auto &_Subscription, mint _nSubScribed)
					{
						NTime::CClock Clock{true};
						mint nSubscribed = 0;
						while (Clock.f_GetTime() < 10.0)
						{
							nSubscribed = fg_Dispatch
								(
									HelperActor
									, [&]
									{
										return _Subscription.m_Actors.f_GetLen();
									}
								).f_CallSync(pRunLoop, g_Timeout)
							;
							if (nSubscribed == _nSubScribed)
								break;
						}
						return nSubscribed;
					}
				;
				{
					DMibTestPath("Concurrent subscribe");
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);

					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto Subscriptions =
						(
							ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", HelperActor, 0, TCLimitsInt<uint32>::mc_Max)
							+ ClientTrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>, "com.malterlib/Test", HelperActor, 0, TCLimitsInt<uint32>::mc_Max)
						).f_CallSync()
					;
					auto &TrustedSubscription0 = fg_Get<0>(Subscriptions);
					auto &TrustedSubscription1 = fg_Get<1>(Subscriptions);
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
						DMibExpect(TrustedSubscription1.m_Actors.f_GetLen(), ==, 2);
					}
					TrustedSubscription0.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
					TrustedSubscription1.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Publish while subscribed");
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);

					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen());
					auto TrustedSubscription0 = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
							, "com.malterlib/Test"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 2);
					}
					CStr NewPublish = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
					Published[NewPublish];
					DMibExpect(fWaitForSubscribed(TrustedSubscription0, 3), ==, 3);

					{
						DMibTestPath("2 types");
						auto TrustedSubscription2 = ClientTrustManager
							(
								&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>
								, "com.malterlib/Test"
								, HelperActor
								, 0
								, TCLimitsInt<uint32>::mc_Max
							)
							.f_CallSync(pRunLoop, g_Timeout)
						;
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test");
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						ServerHelper.f_Unpublish(NewPublish2);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
						TrustedSubscription2.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
					}
					{
						DMibTestPath("2 types no sub unpublish");
						auto TrustedSubscription2 = ClientTrustManager
							(
								&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>
								, "com.malterlib/Test"
								, HelperActor
								, 0
								, TCLimitsInt<uint32>::mc_Max
							)
							.f_CallSync(pRunLoop, g_Timeout)
						;
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test");
						CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test", Published.f_GetLen() + 1);
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription));
						fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 1), ==, 1);
						TrustedSubscription2.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
						ServerHelper.f_Unpublish(NewPublish2);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription))
								break;
						}
						DMibExpect(fWaitForSubscribed(TrustedSubscription2, 0), ==, 0);
						fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
						ClientHelper.f_Unsubscribe(Subscription);
					}
					{
						DMibTestPath("2 types not allowed");
						auto TrustedSubscription2 = ClientTrustManager
							(
								&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>
								, "com.malterlib/Test2"
								, HelperActor
								, 0
								, TCLimitsInt<uint32>::mc_Max
							)
							.f_CallSync(pRunLoop, g_Timeout)
						;
						auto TrustedSubscription3 = ClientTrustManager
							(
								&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
								, "com.malterlib/Test2"
								, HelperActor
								, 0
								, TCLimitsInt<uint32>::mc_Max
							)
							.f_CallSync(pRunLoop, g_Timeout)
						;
						CStr NewPublish2 = ServerHelper.f_Publish<CTestActor2>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor2>(), "com.malterlib/Test2");
						CStr NewPublish3 = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test2");
						CStr Subscription = ClientHelper.f_Subscribe("com.malterlib/Test2", 2);
						DMibExpectTrue(ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
						fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
						ServerHelper.f_Unpublish(NewPublish2);
						ServerHelper.f_Unpublish(NewPublish3);
						NTime::CClock Clock{true};
						while (Clock.f_GetTime() < 10.0)
						{
							if (!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && !ClientHelper.f_GetRemoteActor<CTestActor>(Subscription))
								break;
						}
						DMibExpectTrue(!ClientHelper.f_GetRemoteActor<CTestActor2>(Subscription) && !ClientHelper.f_GetRemoteActor<CTestActor>(Subscription));
						fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
						ClientHelper.f_Unsubscribe(Subscription);
						TrustedSubscription2.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
						TrustedSubscription3.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
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
					TrustedSubscription0.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("2 hosts no sub");
					CState State2{pRunLoop, _fDatabaseFactory, nullptr, "2"};
					TCActor<CDistributedActorTrustManager> ServerTrustManager2 = State2.f_CreateServerTrustManager();
					CDistributedActorTestHelper ServerHelper2{ServerTrustManager};
					{
						CDistributedActorTrustManager_Address ServerAddress;
						ServerAddress.m_URL = fg_GetSocketUrl("Trust2HostsNoSub");
						ServerTrustManager2(&CDistributedActorTrustManager::f_AddListen, ServerAddress).f_CallSync(pRunLoop, g_Timeout);

						auto TrustTicket = ServerTrustManager2(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr).f_CallSync(pRunLoop, g_Timeout);
						ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, -1).f_CallSync(pRunLoop, g_Timeout);
					}
					CStr ServerHostID2 = ServerTrustManager2(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(pRunLoop, g_Timeout);
					TCSet<CStr> ServerHosts2;
					ServerHosts2[ServerHostID];

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test3", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test3", ServerHosts2, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					auto TrustedSubscription2 = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor2>
							, "com.malterlib/Test3"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;
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
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test3", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					ServerTrustManager2->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					TrustedSubscription2.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Subscribe stress");
					TCActorResultVector<void> Dispatches;
#if DMibConfig_RefCountDebugging
					constexpr mint c_nLoops = 100;
#else
					constexpr mint c_nLoops = 100000;
#endif
					for (mint i = 0; i < c_nLoops; ++i)
					{
						fg_ConcurrentDispatch
							(
								[&]
								{
									ClientTrustManager
										(
											&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
											, "com.malterlib/Test"
											, HelperActor
											, 0
											, TCLimitsInt<uint32>::mc_Max
										)
										> fg_DiscardResult()
									;
								}
							)
							> Dispatches.f_AddResult();
						;
					}
					Dispatches.f_GetResults().f_CallSync(pRunLoop, g_Timeout);
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Notifications");
					NAtomic::TCAtomic<mint> nActors{0};
					CStr Published = ServerHelper.f_Publish<CTestActor>(ServerHelper.f_GetManager()->f_ConstructActor<CTestActor>(), "com.malterlib/Test");
					CStr Subscription0 = ClientHelper.f_Subscribe("com.malterlib/Test");
					auto TrustedSubscription0 = ClientTrustManager
						(
							&CDistributedActorTrustManager::f_SubscribeTrustedActors<CTestActor>
							, "com.malterlib/Test"
							, HelperActor
							, 0
							, TCLimitsInt<uint32>::mc_Max
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;
					{
						DMibTestPath("After subscribe");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 0);
					}
					TCSharedPointer<bool> pExited = fg_Construct(false);
					auto Cleanup = g_OnScopeExit / [&]
						{
							*pExited = true;
						}
					;
					fg_Dispatch
						(
							[&, pExited]() -> TCFuture<void>
							{
								auto OnResume = g_OnResume / [&]
									{
										if (*pExited)
											DMibError("Aborted");
									}
								;

								co_await TrustedSubscription0.f_OnActor
									(
										g_ActorFunctor / [&](TCDistributedActor<CTestActor> const &_Actor, CTrustedActorInfo const &_ActorInfo) -> TCFuture<void>
										{
											++nActors;

											co_return {};
										}
										, g_ActorFunctor / [&](TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_ActorInfo) -> TCFuture<void>
										{
											--nActors;

											co_return {};
										}
									)
								;

								co_return {};
							}
						)
						.f_CallSync(pRunLoop, g_Timeout)
					;

					ClientTrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);

					{
						DMibTestPath("After allow");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 1);
						DMibExpect(nActors.f_Load(), ==, 1);
					}

					ClientTrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, "com.malterlib/Test", ServerHosts, c_WaitForSubscriptions).f_CallSync(pRunLoop, g_Timeout);
					fg_Dispatch(HelperActor, []{}).f_CallSync(pRunLoop, g_Timeout);

					{
						DMibTestPath("After disallow");
						DMibExpect(TrustedSubscription0.m_Actors.f_GetLen(), ==, 0);
						DMibExpect(nActors.f_Load(), ==, 0);
					}
					TrustedSubscription0.f_Destroy().f_CallSync(pRunLoop, g_Timeout);
				}

				ClientTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
				ServerTrustManager->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
			{
				DMibTestPath("Permissions");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};

				CPermissionTestState TestState{State, "Permissions"};

				{
					DMibTestPath("General");
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test", "com.malterlib/Test2")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;
					{
						DMibTestPath("Before add permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Before add permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumPermissionsEmpty);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumPermissionsEmpty);
					}

					TestState.f_AddPermissions("com.malterlib/Test");
					TestState.f_AddPermissions("com.malterlib/Test");
					{
						DMibTestPath("After add permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissions);
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumPermissionsNoHostInfo);
					}

					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test");
					{
						DMibTestPath("After remove permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After remove permission", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumPermissionsEmpty);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumPermissionsEmpty);
					}

					TestState.f_AddPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						[[maybe_unused]] auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test2");
					{
						DMibTestPath("After remove double permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
					}
					TestState.f_AddPermissions("com.malterlib/Test3");
					{
						DMibTestPath("After add irrelevant permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
					}
					TestState.f_RemovePermissions("com.malterlib/Test3");
				}
				{
					DMibTestPath("Double wildcards");
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("*", "*", "com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;

					TestState.f_AddPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						[[maybe_unused]] auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test2");
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
						).f_CallSync(pRunLoop, g_Timeout)
					;

					NAtomic::TCAtomic<mint> nPermissions{0};
					TrustedSubscription.f_OnPermissionsAdded
						(
							[&](CPermissionIdentifiers const &, TCSet<CStr> const &_Permissions)
							{
								nPermissions.f_FetchAdd(_Permissions.f_GetLen());
							}
						)
					;
					TrustedSubscription.f_OnPermissionsRemoved
						(
							[&](CPermissionIdentifiers const &, TCSet<CStr> const &_Permissions)
							{
								nPermissions.f_FetchSub(_Permissions.f_GetLen());
							}
						)
					;

					TestState.f_AddPermissions("com.malterlib/Test", "com.malterlib/Test2");
					{
						DMibTestPath("After add double permissions");
						DMibExpectTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibExpectTrue(TrustedSubscription.f_HasPermission("After add double permissions", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibExpect(nPermissions.f_Load(), ==, 2);

					}
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test");
					TestState.f_RemovePermissions("com.malterlib/Test2");
					{
						DMibTestPath("After remove double permission");
						auto &Permissions = TrustedSubscription.f_GetPermissions();
						DMibAssert(Permissions, ==, TestState.m_ExpectedPermissionsEmpty);
						DMibExpect(nPermissions.f_Load(), ==, 0);
					}
				}
				{
					DMibTestPath("Registered permissions");
					TestState.f_AddPermissions("com.malterlib/Test");
					{
						DMibTestPath("After add permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout) .m_Permissions;
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumPermissionsNoHostInfo);
					}

					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RegisterPermissions, fg_CreateSet<CStr>("com.malterlib/Test5")).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After register permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						auto ExpectedEnumPermissions2 = TestState.m_ExpectedEnumPermissions;
						ExpectedEnumPermissions2["com.malterlib/Test5"];
						DMibAssert(EnumPermissions, ==, ExpectedEnumPermissions2);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						auto ExpectedEnumPermissionsNoHostInfo2 = TestState.m_ExpectedEnumPermissionsNoHostInfo;
						ExpectedEnumPermissionsNoHostInfo2["com.malterlib/Test5"];
						DMibAssert(EnumPermissionsNoHostInfo, ==, ExpectedEnumPermissionsNoHostInfo2);
					}

					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_UnregisterPermissions, fg_CreateSet<CStr>("com.malterlib/Test5")).f_CallSync(pRunLoop, g_Timeout);
					{
						DMibTestPath("After unregister permission");
						auto EnumPermissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissions, ==, TestState.m_ExpectedEnumPermissions);
						auto EnumPermissionsNoHostInfo = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, false).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssert(EnumPermissionsNoHostInfo, ==, TestState.m_ExpectedEnumPermissionsNoHostInfo);
					}
					TestState.f_RemovePermissions("com.malterlib/Test");
				}
			}
			{
				DMibTestPath("Permissions Database");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};
				{
					DMibTestPath("Setup");
					CPermissionTestState TestState{State, "PermissionsDatabase"};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;
					TestState.f_AddPermissions("com.malterlib/Test", "com.malterlib/Test2", "com.malterlib/Test3");
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
				}
				{
					DMibTestPath("After Reload");
					CPermissionTestState TestState{State, "PermissionsDatabase"};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Reload", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));

					TestState.f_RemovePermissions("com.malterlib/Test2", "com.malterlib/Test3");

					{
						DMibTestPath("After Remove");
						DMibAssertTrue(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
				}
				{
					DMibTestPath("After Remove Reload");
					CPermissionTestState TestState{State, "PermissionsDatabase"};
					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/Test*")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;
					DMibAssertTrue(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test2"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
					DMibAssertFalse(TrustedSubscription.f_HasPermission("After Remove Reload", {"com.malterlib/Test3"}, TestState.m_ServerHostInfo).f_CallSync(pRunLoop, g_Timeout));
				}
			}
			{
				DMibTestPath("User Database");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				using CMetadata = TCMap<CStr, NEncoding::CEJSON>;
				using CKeys = TCSet<CStr>;
				TCSharedPointer<CCommandLineControl> pCommandLine;
				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};
				CStr const ID1 = "2YAzJPcR2K5QMbJYP";
				CStr const ID2 = "DPYQEvAqw4RQhXRYe";
				CStr const ID3 = "JNsXrbgP3dL6xuFXm";
				CStr const ID4 = "Tao5Dmb6FpMzTCQyD";
				CStr const ID5 = "RSZFNPB5mFEANtCNd";
				TCOptional<NStr::CStr> UnsetUserName;

				CMetadata Empty;
				CMetadata Metadata2{{"Key", "Value"}};
				CMetadata Metadata3{{"Key2", "FunkyStuff"}};
				Metadata3["Key1"] = {"Key1"_= "NewValue1", "Key2"_= "Value2"};
				CMetadata Metadata4{{"Key0", "Value0"}};
				Metadata4["Key1"] = "Value1";

				TCSet<CStr> NoRemove;
				{
					DMibTestPath("Initial");
					CPermissionTestState TestState{State, "UserDatabase"};

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID1, "User1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID2, "User2").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID3, "User3").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID4, "User4").f_CallSync(pRunLoop, g_Timeout);

					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID1, "User1 again").f_CallSync(pRunLoop, g_Timeout)
							, DMibErrorInstance("User '2YAzJPcR2K5QMbJYP' already exists")
						)
					;
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, "**COOL**", "Invalid ID").f_CallSync(pRunLoop, g_Timeout)
							, DMibErrorInstance("Invalid user ID")
						)
					;

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);

					DMibExpect(AllUsers.f_GetLen(), ==, 4);
					DMibExpect(AllUsers[ID1].m_UserName, ==, "User1");
					DMibExpect(AllUsers[ID2].m_UserName, ==, "User2");
					DMibExpect(AllUsers[ID3].m_UserName, ==, "User3");
					DMibExpect(AllUsers[ID4].m_UserName, ==, "User4");

					auto OptionalUserInfo1 = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_TryGetUser, ID1).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(OptionalUserInfo1);
					DMibExpect(OptionalUserInfo1->m_UserName, ==, "User1");
					DMibExpectFalse(TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_TryGetUser, "NotInDB").f_CallSync(pRunLoop, g_Timeout));

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, NoRemove, Metadata2).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID3, UnsetUserName, NoRemove, Metadata3).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID4, UnsetUserName, NoRemove, Metadata4).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, "**COOL**", UnsetUserName, NoRemove, Metadata4).f_CallSync(pRunLoop, g_Timeout)
							, DMibErrorInstance("Invalid user ID")
						)
					;
					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, "1234567890", UnsetUserName, NoRemove, Metadata4).f_CallSync(pRunLoop, g_Timeout)
							, DMibErrorInstance("User '1234567890' does not exist")
						)
					;

					AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(AllUsers[ID1].m_Metadata.f_IsEmpty());
					DMibExpect(AllUsers[ID2].m_Metadata, ==, Metadata2);
					DMibExpect(AllUsers[ID3].m_Metadata, ==, Metadata3);
					DMibExpect(AllUsers[ID4].m_Metadata, ==, Metadata4);
				}
				{
					DMibTestPath("From database");
					CPermissionTestState TestState{State, "UserDatabase"};

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);

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
				auto fGetNames = [&](TCActor<CDistributedActorTrustManager> &_Actor, CStr const &_ID) -> TCSet<CStr>
					{
						auto const &Factors = _Actor(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _ID).f_CallSync(pRunLoop, g_Timeout);
						TCSet<CStr> Results;
						for (auto const &Factor : Factors)
							Results[Factor.m_Name];
						return Results;
					}
				;
				{
					DMibTestPath("Register Authentication");
					CPermissionTestState TestState{State, "UserDatabase"};

					auto Factors = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors).f_CallSync(pRunLoop, g_Timeout);
					DMibExpect(Factors.f_GetLen(), >=, 2u);
					DMibExpectTrue(!!Factors.f_FindEqual("Test1"));
					DMibExpectTrue(!!Factors.f_FindEqual("Test2"));

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID1, "Test1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID1, "Test1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID2, "Test2").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID3, "Test1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID3, "Test2").f_CallSync(pRunLoop, g_Timeout);

					DMibExpect(TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID1).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 2);
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID1), ==, TCSet<CStr>{"Test1"});
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID2), ==, TCSet<CStr>{"Test2"});
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID3), ==, (TCSet<CStr>{"Test1", "Test2"}));
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID4), ==, TCSet<CStr>{});
				}
				{
					DMibTestPath("Export/Import");
					CPermissionTestState TestState{State, "UserDatabase"};

					auto Exported1 = fg_ExportUser(TestState.m_ServerTrustManager, ID1, true).f_CallSync(pRunLoop, g_Timeout);
					auto Exported2 = fg_ExportUser(TestState.m_ServerTrustManager, ID2, true).f_CallSync(pRunLoop, g_Timeout);
					auto Exported3 = fg_ExportUser(TestState.m_ServerTrustManager, ID3, true).f_CallSync(pRunLoop, g_Timeout);
					auto Exported4 = fg_ExportUser(TestState.m_ServerTrustManager, ID4, true).f_CallSync(pRunLoop, g_Timeout);

					fg_ImportUser(TestState.m_ClientTrustManager, Exported1).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported2).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported3).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported4).f_CallSync(pRunLoop, g_Timeout);

					auto AllUsers = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);

					DMibExpect(AllUsers.f_GetLen(), ==, 4);
					DMibExpect(AllUsers[ID1].m_UserName, ==, "User1");
					DMibExpect(AllUsers[ID2].m_UserName, ==, "User2");
					DMibExpect(AllUsers[ID3].m_UserName, ==, "User3");
					DMibExpect(AllUsers[ID4].m_UserName, ==, "User4");
					DMibExpectTrue(AllUsers[ID1].m_Metadata.f_IsEmpty());
					DMibExpect(AllUsers[ID2].m_Metadata, ==, Metadata2);
					DMibExpect(AllUsers[ID3].m_Metadata, ==, Metadata3);
					DMibExpect(AllUsers[ID4].m_Metadata, ==, Metadata4);

					DMibExpect(TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID1).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 2);
					DMibExpect(fGetNames(TestState.m_ClientTrustManager, ID1), ==, TCSet<CStr>{"Test1"});
					DMibExpect(fGetNames(TestState.m_ClientTrustManager, ID2), ==, TCSet<CStr>{"Test2"});
					DMibExpect(fGetNames(TestState.m_ClientTrustManager, ID3), ==, (TCSet<CStr>{"Test1", "Test2"}));
					DMibExpect(fGetNames(TestState.m_ClientTrustManager, ID4), ==, TCSet<CStr>{});

					//
					// Importing should add new stuff to the existing user and not overwrite a factor with private stuff with one with only public stuff
					//
					// Export a new user
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_AddUser, ID5, "User5").f_CallSync(pRunLoop, g_Timeout);
					auto Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, false).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					auto SingleUser1 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_TryGetUser, ID5).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(SingleUser1);
					DMibExpect(SingleUser1->m_UserName, ==, "User5");
					DMibExpectTrue(SingleUser1->m_Metadata.f_IsEmpty());

					// Make sure we can add metadata to existing user
					CMetadata Metadata5{{"Key5", "Value5"}};
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID5, UnsetUserName, NoRemove, Metadata5).f_CallSync(pRunLoop, g_Timeout);
					Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, false).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					SingleUser1 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_TryGetUser, ID5).f_CallSync(pRunLoop, g_Timeout);
					DMibExpect(SingleUser1->m_Metadata, ==, Metadata5);

					// Remove metadata, export again, and check that it wasn't removed on the client side
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID5, UnsetUserName, CKeys{"Key5"}, CMetadata{}).f_CallSync(pRunLoop, g_Timeout);
					Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, false).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					auto SingleUser2 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_TryGetUser, ID5).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(SingleUser2);
					DMibExpect(SingleUser2->m_Metadata, ==, Metadata5);

					// Add authentication factor and export with private data
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID5, "Test1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID5, "Test2").f_CallSync(pRunLoop, g_Timeout);
					Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, true).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					auto Factors1 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout);
					int nNonEmpty1 = 0;
					for (auto &Factor : Factors1)
						nNonEmpty1 += !Factor.m_PrivateData.f_IsEmpty();

					DMibExpect(nNonEmpty1, ==, Factors1.f_GetLen());

					// Add two more, this time only export public data, check all factors exported two still have private data
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID5, "Test1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, pCommandLine, ID5, "Test2").f_CallSync(pRunLoop, g_Timeout);
					Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, false).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					auto Factors2 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout);
					int nNonEmpty2 = 0;
					for (auto &Factor : Factors2)
						nNonEmpty2 += !Factor.m_PrivateData.f_IsEmpty();
					DMibExpect(nNonEmpty2, ==, 2);
					DMibExpect(Factors2.f_GetLen(), ==, 4);

					// Export private data this time, check that all factors exported now have private data
					Exported5 = fg_ExportUser(TestState.m_ServerTrustManager, ID5, true).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(TestState.m_ClientTrustManager, Exported5).f_CallSync(pRunLoop, g_Timeout);
					auto Factors3 = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout);
					int nNonEmpty3 = 0;
					for (auto &Factor : Factors3)
						nNonEmpty3 += !Factor.m_PrivateData.f_IsEmpty();
					DMibExpect(nNonEmpty3, ==, 4);
					DMibExpect(Factors3.f_GetLen(), ==, 4);

					// Cleanup on the client side
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID1).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID2).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID3).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID4).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID5).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID5, UnsetUserName, NoRemove, Metadata5).f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Basic testing of the trust proxy");
					CPermissionTestState TestState{State, "UserDatabase"};
					CDistributedActorTrustManagerProxy::CPermissions Permissions;
					Permissions.m_Permissions = CDistributedActorTrustManagerProxy::EPermission_All;

					CDistributedActorTestHelper ServerHelper{TestState.m_ServerTrustManager};
					CDistributedActorTestHelper ClientHelper{TestState.m_ClientTrustManager};

					TCActor<CDistributedActorTrustManagerInterface> ServerProxy
						= ServerHelper.f_GetManager()->f_ConstructActor<CDistributedActorTrustManagerProxy>(TestState.m_ServerTrustManager, Permissions)
					;
					TCActor<CDistributedActorTrustManagerInterface> ClientProxy
						= ClientHelper.f_GetManager()->f_ConstructActor<CDistributedActorTrustManagerProxy>(TestState.m_ClientTrustManager, Permissions)
					;

					auto Exported5 = fg_ExportUser(ServerProxy, ID5, true).f_CallSync(pRunLoop, g_Timeout);
					fg_ImportUser(ClientProxy, Exported5).f_CallSync(pRunLoop, g_Timeout);

					auto AllUsers = ClientProxy(&CDistributedActorTrustManagerInterface::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);
					DMibExpect(AllUsers.f_GetLen(), ==, 1);
					DMibExpect(AllUsers[ID5].m_UserName, ==, "User5");
					DMibExpect(TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 4);

					ClientProxy
						(&CDistributedActorTrustManagerInterface::f_AddUserAuthenticationFactor
							, ID5
							, "AjPPPXBWJMB8PfZiC"
							, CAuthenticationData{EAuthenticationFactorCategory_Knowledge, "Junk"}
						).f_CallSync(pRunLoop, g_Timeout)
					;
					DMibExpect(ClientProxy(&CDistributedActorTrustManagerInterface::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 5);
					ClientProxy(&CDistributedActorTrustManagerInterface::f_RemoveUserAuthenticationFactor, ID5, "AjPPPXBWJMB8PfZiC").f_CallSync(pRunLoop, g_Timeout);
					DMibExpect(ClientProxy(&CDistributedActorTrustManagerInterface::f_EnumUserAuthenticationFactors, ID5).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 5 - 1);

					ClientProxy(&CDistributedActorTrustManagerInterface::f_AddUser, ID1, "User1").f_CallSync(pRunLoop, g_Timeout);
					DMibExpectTrue(ClientProxy(&CDistributedActorTrustManagerInterface::f_TryGetUser, ID1).f_CallSync(pRunLoop, g_Timeout));
					DMibExpect(ClientProxy(&CDistributedActorTrustManagerInterface::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout).f_GetLen(), ==, 2);
					ClientProxy(&CDistributedActorTrustManagerInterface::f_RemoveUser, ID1).f_CallSync(pRunLoop, g_Timeout);
					DMibExpectFalse(ClientProxy(&CDistributedActorTrustManagerInterface::f_TryGetUser, ID1).f_CallSync(pRunLoop, g_Timeout));

					ClientProxy(&CDistributedActorTrustManagerInterface::f_SetUserInfo, ID5, UnsetUserName, CKeys{"Key5"}, CMetadata{}).f_CallSync(pRunLoop, g_Timeout);

					// Cleanup
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID5).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID5).f_CallSync(pRunLoop, g_Timeout);
				}
				{
					DMibTestPath("Set userinfo and remove metadata");
					CPermissionTestState TestState{State, "UserDatabase"};

					TCOptional<NStr::CStr> SetUserName{"NewUser1"};
					CMetadata UnsetMetadata;
					CMetadata SetMetadata;
					SetMetadata["Key1"] = "NewValue";
					SetMetadata["Key2"] = "Value2";
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID1, SetUserName, CKeys{}, UnsetMetadata).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID4, UnsetUserName, CKeys{}, SetMetadata).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, CKeys{"Key"}, CMetadata{}).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID3, UnsetUserName, CKeys{"Key1"}, CMetadata{}).f_CallSync(pRunLoop, g_Timeout);

					Metadata3.f_Remove("Key1");
					SetMetadata["Key0"] = "Value0";
					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);
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
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_SetUserInfo, ID2, UnsetUserName, CKeys{"Key"}, CMetadata{}).f_CallSync(pRunLoop, g_Timeout);
							, DMibErrorInstance("Key 'Key' does not exist")
						)
					;
				}
				{
					DMibTestPath("Unregister Authentication");
					CPermissionTestState TestState{State, "UserDatabase"};
					TCSharedPointer<CCommandLineControl> pCommandLine;

					auto Factors = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID1).f_CallSync(pRunLoop, g_Timeout);
					for (auto &Factor : Factors)
						TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, ID1, Factors.fs_GetKey(Factor)).f_CallSync(pRunLoop, g_Timeout);
					Factors = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID2).f_CallSync(pRunLoop, g_Timeout);
					for (auto &Factor : Factors)
						TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, ID2, Factors.fs_GetKey(Factor)).f_CallSync(pRunLoop, g_Timeout);
					Factors = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, ID3).f_CallSync(pRunLoop, g_Timeout);
					for (auto &Factor : Factors)
						TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, ID3, Factors.fs_GetKey(Factor)).f_CallSync(pRunLoop, g_Timeout);

					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID1), ==, TCSet<CStr>{});
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID2), ==, TCSet<CStr>{});
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID3), ==, TCSet<CStr>{});
					DMibExpect(fGetNames(TestState.m_ServerTrustManager, ID4), ==, TCSet<CStr>{});
				}
				{
					DMibTestPath("Remove user");
					CPermissionTestState TestState{State, "UserDatabase"};

					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID2).f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID4).f_CallSync(pRunLoop, g_Timeout);

					auto AllUsers = TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_EnumUsers, true).f_CallSync(pRunLoop, g_Timeout);

					DMibExpect(AllUsers.f_GetLen(), ==, 2);
					DMibExpect("NewUser1", ==, AllUsers[ID1].m_UserName);
					DMibExpect("User3", ==, AllUsers[ID3].m_UserName);

					DMibExpectException
						(
							TestState.m_ServerTrustManager(&CDistributedActorTrustManager::f_RemoveUser, ID2).f_CallSync(pRunLoop, g_Timeout)
							, DMibErrorInstance("No user with ID 'DPYQEvAqw4RQhXRYe'")
						)
					;
				}
			}
			{
				DMibTestPath("Permissions for HostID+UserID combos");
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				CStr const ID1 = "2YAzJPcR2K5QMbJYP";
				CStr const ID2 = "DPYQEvAqw4RQhXRYe";
				CStr const ID3 = "JNsXrbgP3dL6xuFXm";
				CStr const ID4 = "Tao5Dmb6FpMzTCQyD";
				CStr const ID5 = "RSZFNPB5mFEANtCNd";

				CState State{pRunLoop, _fDatabaseFactory, _fCleanup};
				{
					DMibTestPath("Basic HostID+UserID permission checks");
					CPermissionTestState TestState{State, "PermissionsCombos"};

					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_AddUser, ID1, "User1").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_AddUser, ID2, "User2").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_AddUser, ID3, "User3").f_CallSync(pRunLoop, g_Timeout);
					TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_AddUser, ID4, "User4").f_CallSync(pRunLoop, g_Timeout);

					TestState.f_AddPermissions("com.malterlib/Host");
					TestState.f_AddUserPermission(ID1, "com.malterlib/User1");
					TestState.f_AddUserPermission(ID2, "com.malterlib/User2");
					TestState.f_AddUserPermission(ID1, "com.malterlib/User12");
					TestState.f_AddUserPermission(ID2, "com.malterlib/User12");
					TestState.f_AddHostUserPermission(ID3, "com.malterlib/HostUser3");
					TestState.f_AddHostUserPermission(ID4, "com.malterlib/HostUser4");
					TestState.f_AddHostUserPermission(ID3, "com.malterlib/HostUser34");
					TestState.f_AddHostUserPermission(ID4, "com.malterlib/HostUser34");

					//This doesn't make sense, but you can do it
					TestState.f_AddHostUserPermission(ID4, "com.malterlib/ThreeWay");
					TestState.f_AddUserPermission(ID4, "com.malterlib/ThreeWay");
					TestState.f_AddPermissions("com.malterlib/ThreeWay");

					auto HostID = TestState.m_ServerHostID;
					auto Permissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;

					DMibAssertTrue(Permissions.f_GetLen() == 8);
					DMibAssertTrue(Permissions["com.malterlib/Host"].f_GetLen() == 1);
					DMibAssertTrue(!!Permissions["com.malterlib/Host"].f_FindEqual(CPermissionIdentifiers(HostID, "")));

					DMibAssertTrue(Permissions["com.malterlib/User1"].f_GetLen() == 1);
					DMibAssertTrue(!!Permissions["com.malterlib/User1"].f_FindEqual(CPermissionIdentifiers("", ID1)));

					DMibAssertTrue(Permissions["com.malterlib/User2"].f_GetLen() == 1);
					DMibAssertTrue(!!Permissions["com.malterlib/User2"].f_FindEqual(CPermissionIdentifiers("", ID2)));

					DMibAssertTrue(Permissions["com.malterlib/User12"].f_GetLen() == 2);
					DMibAssertTrue(!!Permissions["com.malterlib/User12"].f_FindEqual(CPermissionIdentifiers("", ID1)));
					DMibAssertTrue(!!Permissions["com.malterlib/User12"].f_FindEqual(CPermissionIdentifiers("", ID2)));

					DMibAssertTrue(Permissions["com.malterlib/HostUser3"].f_GetLen() == 1);
					DMibAssertTrue(!!Permissions["com.malterlib/HostUser3"].f_FindEqual(CPermissionIdentifiers(HostID, ID3)));

					DMibAssertTrue(Permissions["com.malterlib/HostUser4"].f_GetLen() == 1);
					DMibAssertTrue(!!Permissions["com.malterlib/HostUser4"].f_FindEqual(CPermissionIdentifiers(HostID, ID4)));

					DMibAssertTrue(Permissions["com.malterlib/HostUser34"].f_GetLen() == 2);
					DMibAssertTrue(!!Permissions["com.malterlib/HostUser34"].f_FindEqual(CPermissionIdentifiers(HostID, ID3)));
					DMibAssertTrue(!!Permissions["com.malterlib/HostUser34"].f_FindEqual(CPermissionIdentifiers(HostID, ID4)));

					DMibAssertTrue(Permissions["com.malterlib/ThreeWay"].f_GetLen() == 3);
					DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers(HostID, "")));
					DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers(HostID, ID4)));
					DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers("", ID4)));

					{
						DMibTestPath("Remove 1st");

						TestState.f_RemoveHostUserPermission(ID4, "com.malterlib/ThreeWay");
						Permissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssertTrue(Permissions["com.malterlib/ThreeWay"].f_GetLen() == 2);
						DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers(HostID, "")));
						DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers("", ID4)));
					}
					{
						DMibTestPath("Remove 2nd");

						TestState.f_RemoveUserPermission(ID4, "com.malterlib/ThreeWay");
						Permissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssertTrue(Permissions["com.malterlib/ThreeWay"].f_GetLen() == 1);
						DMibAssertTrue(!!Permissions["com.malterlib/ThreeWay"].f_FindEqual(CPermissionIdentifiers(HostID, "")));
					}
					{
						DMibTestPath("Remove 3rd");

						TestState.f_RemovePermissions("com.malterlib/ThreeWay");
						Permissions = TestState.m_ClientTrustManager(&CDistributedActorTrustManager::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout).m_Permissions;
						DMibAssertTrue(Permissions.f_GetLen() == 7);
					}

					auto TrustedSubscription = TestState.m_ClientTrustManager
						(
							 &CDistributedActorTrustManager::f_SubscribeToPermissions
							 , fg_CreateVector<CStr>("com.malterlib/*")
							 , TestState.m_TestActor
						).f_CallSync(pRunLoop, g_Timeout)
					;

					auto fHostInfo = [&Info = TestState.m_ServerHostInfo](CStr const &_UserID, CStr const &_HostID = "")
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

					// Test the simplest form of permission checking: f_HasPermission - true if any permission matches
					{
						DMibTestPath("Test ID1 f_HasPermission");
						auto HostInfo = fHostInfo(ID1);

						// These should all be true if we have permission for one or more of the permissions in the vector
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host", "com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host", "com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/Nomatch"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Nomatch", "com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Nomatch"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User2", "com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					{
						DMibTestPath("Test ID3 f_HasPermission");
						auto HostInfo = fHostInfo(ID3);

						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));

						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/HostUser4"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}

					{
						DMibTestPath("Test ID1 f_HasPermission, other host");
						auto HostInfo = fHostInfo(ID1, ID5);

						// We no longer have the host based permissions
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host", "com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/Nomatch"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Nomatch", "com.malterlib/User1"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host", "com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User2"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Nomatch"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User2", "com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					{
						DMibTestPath("Test ID3 f_HasPermission, other host");
						auto HostInfo = fHostInfo(ID3, ID5);

						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/Host"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/User1", "com.malterlib/HostUser3"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermission("Setup", {"com.malterlib/HostUser4"}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}

					// Test the slightly more complex form of permission checking: f_HasPermissions (variant 1).
					//
					// In one call we can do multiple permission queries. Each query can have multiple permissions where having either one of them is enough.
					// For example (CommandAll or Command/ThisCommand) and (ReadAll or Read/ThisData).
					using VQ = TCVector<CPermissionQuery>;
					{
						DMibTestPath("Test ID1 f_HasPermissions (1)");
						auto HostInfo = fHostInfo(ID1);

						// This test should behave exactly as f_HasPermission if there is only one query, so we repeat those tests

						// These should all be true if we have permission for one or more of the permissions in the vector
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host", "com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host", "com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/Nomatch"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Nomatch", "com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Nomatch"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User2", "com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					{
						DMibTestPath("Test ID3 f_HasPermissions (1)");
						auto HostInfo = fHostInfo(ID3);

						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));

						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/HostUser4"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}

					{
						DMibTestPath("Test ID1 f_HasPermissions (1), other host");
						auto HostInfo = fHostInfo(ID1, ID5);

						// We no longer have the host based permissions
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host", "com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/Nomatch"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Nomatch", "com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host", "com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User2"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Nomatch"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User2", "com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}
					{
						DMibTestPath("Test ID3 f_HasPermissions (1), other host");
						auto HostInfo = fHostInfo(ID3, ID5);

						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/User1", "com.malterlib/HostUser3"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/HostUser4"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
					}

					{
						DMibTestPath("Test ID1 f_HasPermissions (1), multiple queries");
						auto HostInfo = fHostInfo(ID1);

						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}, {"com.malterlib/User1"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", VQ{{"com.malterlib/Host"}, {"com.malterlib/Host"}}, HostInfo).f_CallSync(pRunLoop, g_Timeout));

						DMibAssertTrue(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User12"}}
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout))
						;

						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, VQ{{"com.malterlib/NoMatch", "com.malterlib/User2"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User12"}}
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout))
						;
						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/NoMatch", "com.malterlib/User2"}, {"com.malterlib/User12"}}
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout))
						;
						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User34"}}
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout))
						;
					}

					// Test the even more complex form of permission checking: f_HasPermissions (variant 2).
					//
					// As above we can pose multiple queries in one call and each query can have multiple forms of the permission. As in the  (CommandAll or Command/ThisCommand) and
					// (ReadAll or Read/ThisData) example, but in this case it is possible to combine several such query vectors in one call. Each query is tagged,
					// so the caller can tell which query that failed.
					auto fMQ1 = [](CStr _Tag, VQ &&_Query) -> TCMap<CStr, TCVector<CPermissionQuery>>
						{
							TCMap<CStr, TCVector<CPermissionQuery>> Result;
							Result[_Tag] = fg_Move(_Query);
							return Result;
						}
					;
					auto fMQ2 = [](CStr _Tag1, VQ &&_Query1, CStr _Tag2, VQ &&_Query2) -> TCMap<CStr, TCVector<CPermissionQuery>>
						{
							TCMap<CStr, TCVector<CPermissionQuery>> Result;
							Result[_Tag1] = fg_Move(_Query1);
							Result[_Tag2] = fg_Move(_Query2);
							return Result;
						}
					;
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
						DMibTestPath("Test ID1 f_HasPermissions (2)");
						auto HostInfo = fHostInfo(ID1);

						// This test should behave exactly as f_HasPermission if there is only one query, so we repeat those tests

						// These should all be true if we have permission for one or more of the permissions in the vector
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host", "com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host", "com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/Nomatch"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Nomatch", "com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Nomatch"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User2", "com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
					}
					{
						DMibTestPath("Test ID3 f_HasPermissions (2)");
						auto HostInfo = fHostInfo(ID3);

						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);

						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/HostUser4"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
					}

					{
						DMibTestPath("Test ID1 f_HasPermissions (2), other host");
						auto HostInfo = fHostInfo(ID1, ID5);

						// We no longer have the host based permissions
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host", "com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/Nomatch"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Nomatch", "com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						// But we must have permission for at least one
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host", "com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User2"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Nomatch"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User2", "com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
					}
					{
						DMibTestPath("Test ID3 f_HasPermissions (2), other host");
						auto HostInfo = fHostInfo(ID3, ID5);

						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/User1", "com.malterlib/HostUser3"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertFalse(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/HostUser4"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
					}

					{
						DMibTestPath("Test ID1 f_HasPermissions (2), multiple queries");
						auto HostInfo = fHostInfo(ID1);

						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}, {"com.malterlib/User1"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);
						DMibAssertTrue(TrustedSubscription.f_HasPermissions("Setup", fMQ1("Tag", VQ{{"com.malterlib/Host"}, {"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout)["Tag"]);

						DMibAssertTrue(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ1("Tag", VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)["Tag"])
						;

						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ1("Tag", VQ{{"com.malterlib/NoMatch", "com.malterlib/User2"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)["Tag"])
						;
						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ1("Tag", VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/NoMatch", "com.malterlib/User2"}, {"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)["Tag"])
						;
						DMibAssertFalse(TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ1("Tag", VQ{{"com.malterlib/NoMatch", "com.malterlib/User1"}, {"com.malterlib/Host", "com.malterlib/User2"}, {"com.malterlib/User34"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)["Tag"])
						;
					}
					{
						DMibTestPath("Test ID1 f_HasPermissions (2), multiple named queries");
						auto HostInfo = fHostInfo(ID1);

						auto Res1 = TrustedSubscription.f_HasPermissions("Setup", fMQ2("Tag1", VQ{{"com.malterlib/User1"}}, "Tag2", VQ{{"com.malterlib/Host"}}), HostInfo).f_CallSync(pRunLoop, g_Timeout);
						DMibAssertTrue(Res1["Tag1"]);
						DMibAssertTrue(Res1["Tag2"]);

						auto Res2 = TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ3("Tag1", VQ{{"com.malterlib/User2", "com.malterlib/User1"}}, "Tag2", VQ{{"com.malterlib/Host"}}, "Tag3", VQ{{"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)
						;
						DMibAssertTrue(Res2["Tag1"]);
						DMibAssertTrue(Res2["Tag2"]);
						DMibAssertTrue(Res2["Tag3"]);

						// For the rest of the test we will only test the tag that we know will fail. The check gives up early if it finds one that fails so we don't know which
						// of the other tags that might have been tested (well, it tests the named queries in the iteration order of the map, but we should write tests that relies
						// on that behavior).
						auto Res3 = TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ3("Tag1", VQ{{"com.malterlib/NoMatch"}}, "Tag2", VQ{{"com.malterlib/Host"}}, "Tag3", VQ{{"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)
						;
						DMibAssertFalse(Res3["Tag1"]);

						auto Res4 = TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ3("Tag1", VQ{{"com.malterlib/User1"}}, "Tag2", VQ{{"com.malterlib/NoMatch"}}, "Tag3", VQ{{"com.malterlib/User12"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)
						;
						DMibAssertFalse(Res4["Tag2"]);

						auto Res5 = TrustedSubscription.f_HasPermissions
							(
								"Setup"
								, fMQ3("Tag1", VQ{{"com.malterlib/User1"}}, "Tag2", VQ{{"com.malterlib/Host"}}, "Tag3", VQ{{"com.malterlib/NoMatch"}})
								, HostInfo
							).f_CallSync(pRunLoop, g_Timeout)
						;
						DMibAssertFalse(Res5["Tag3"]);
					}
				}
			}
		}

		void fp_DoDatabaseConversionTests()
		{
			DMibTestSuite("Host Permissions to Permissions")
			{
				TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
				auto CleanupRunLoop = g_OnScopeExit / [&]
					{
						while (pRunLoop->m_RefCount.f_Get() > 0)
							pRunLoop->f_WaitOnceTimeout(0.1);
					}
				;
				TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
				auto CleanupHelperActor = g_OnScopeExit / [&]
					{
						HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
					}
				;
				CCurrentlyProcessingActorScope CurrentActor{HelperActor};

				NStr::CStr BaseDirectory = NFile::CFile::fs_GetProgramDirectory() + "/TestTrustManager/TestDatababseConversion";
				if (NFile::CFile::fs_FileExists(BaseDirectory))
					NFile::CFile::fs_DeleteDirectoryRecursive(BaseDirectory);

				NFile::CFile::fs_CreateDirectory(BaseDirectory / "HostPermissions");
				NFile::CFile::fs_WriteStringToFile
					(
						BaseDirectory / "BasicConfig.json"
						, R"---(
							{
								"HostID": "gib5oMQHBfS6hpSDv",
								"CAPrivateKey": {
									"$binary": ""
								},
								"CACertificate": {
									"$binary": ""
								}
							}
						)---"
					)
				;
				NFile::CFile::fs_WriteStringToFile
					(
						BaseDirectory / "HostPermissions/8MJEEHW9rbRfQKcf8.json"
						, R"---(
							{
								"Permissions": [
									"Application/ReadAll",
									"Application/TagAll",
									"Application/WriteAll"
								]
							}
						)---"
					)
				;

				auto DatabaseActor = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(BaseDirectory);
				DatabaseActor(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig).f_CallSync(pRunLoop, g_Timeout);
				auto Permissions = DatabaseActor(&ICDistributedActorTrustManagerDatabase::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout);

				DMibAssertTrue(Permissions.f_FindEqual(CPermissionIdentifiers{"8MJEEHW9rbRfQKcf8", ""}));
				DMibExpectFalse(Permissions.f_FindEqual(CPermissionIdentifiers{"8MJEEHW9rbRfQKcf8", ""})->m_Permissions.f_IsEmpty());

				DMibExpect(NFile::CFile::fs_FindFiles(BaseDirectory / "HostPermissions/*").f_GetLen(), ==, 0);
				auto PermissionFiles = NFile::CFile::fs_FindFiles(BaseDirectory / "Permissions/*");
				DMibAssert(PermissionFiles.f_GetLen(), ==, 1);
				DMibExpect(NFile::CFile::fs_GetFile(PermissionFiles[0]), ==, "H_8MJEEHW9rbRfQKcf8.json");

				DatabaseActor.f_Destroy().f_CallSync(pRunLoop, g_Timeout);

				{
					DMibTestPath("After reload");
					auto DatabaseActor = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(BaseDirectory);
					DatabaseActor(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig).f_CallSync(pRunLoop, g_Timeout);

					auto Permissions = DatabaseActor(&ICDistributedActorTrustManagerDatabase::f_EnumPermissions, true).f_CallSync(pRunLoop, g_Timeout);
					DMibAssertTrue(Permissions.f_FindEqual(CPermissionIdentifiers{"8MJEEHW9rbRfQKcf8", ""}));
					DMibExpectFalse(Permissions.f_FindEqual(CPermissionIdentifiers{"8MJEEHW9rbRfQKcf8", ""})->m_Permissions.f_IsEmpty());
				}
			};
		}

		void f_DoTests()
		{
			DMibTestCategory("Interface Conversion") -> TCFuture<void>
			{
				TCDistributedActor<CAuthenticationActorTestSucceed> TestEmpty;

				co_await fg_CallSafe(&fg_TestIt, fg_TempCopy(TestEmpty)).f_Timeout(g_Timeout, "Timeout");

				TCActor<CTestInterfaceConversion> Actor = fg_Construct();
				co_await Actor(&CTestInterfaceConversion::f_TestIt, fg_TempCopy(TestEmpty)).f_Timeout(g_Timeout, "Timeout");
				co_await Actor(&CTestInterfaceConversion::f_TestIt2, TestEmpty).f_Timeout(g_Timeout, "Timeout");

				co_return {};
			};

			DMibTestCategory("Database Conversion")
			{
				fp_DoDatabaseConversionTests();
			};

			DMibTestSuite("Tests")
			{
				{
					DMibTestPath("Dummy Database");
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
				}
				{
					DMibTestPath("JSON Directory Database");
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
				}
			};
		}
	};

	DMibTestRegister(CDistributedActorTrustManager_Tests, Malterlib::Concurrency);
}
