// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JsonDirectory>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Cryptography/RandomID>
#include <Mib/Process/ProcessLaunch>

#ifdef DPlatformFamily_Windows
#	include <Windows.h>
#endif

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NEncoding;
using namespace NMib::NStr;
using namespace NMib::NFunction;
using namespace NMib::NCommandLine;

namespace
{
	struct CInitReconnectDelay
	{
		CInitReconnectDelay();
	};

	CInitReconnectDelay::CInitReconnectDelay()
	{
		// Override reconnect delay for whole process
		CDistributedAppActor_Settings::fs_GetGlobalDefaultSettings().m_ReconnectDelay = 1_ms;
	}

	assure_used CInitReconnectDelay g_InitReconnectDelay;

	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp(CStr const &_Name, bool _bInProcessOnly = false, CStr const &_Enclave = {}, CStr const &_RootDirectory = {});

		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override;

		TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) override;

		TCFuture<void> fp_StopApp() override;
	};

	CTestDistributedApp::CTestDistributedApp(CStr const &_Name, bool _bInProcessOnly, CStr const &_Enclave, CStr const &_RootDirectory)
		: CDistributedAppActor
		(
			CDistributedAppActor_Settings(_Name)
			.f_RootDirectory(_RootDirectory.f_IsEmpty() ? NFile::CFile::fs_GetProgramDirectory() / _Name : _RootDirectory)
			.f_Enclave(_Enclave)
			.f_SeparateDistributionManager(true)
			.f_InProcessCommandLineOnly(_bInProcessOnly)
			.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			.f_MinimumCryptoStrength(NCryptography::ECryptoStrength::mc_EquivalentSymmetric128bit)
			.f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality_None)
		)
	{
	}

	void CTestDistributedApp::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		auto Section = o_CommandLine.f_AddSection("Test1", "Testing test 2");

		Section.f_RegisterCommand
			(
				{
					"Names"_o= _o["--test-actor"]
					, "Description"_o= "Test 3."
					, "Options"_o=
					{
						"Integer?"_o=
						{
							"Names"_o= _o["--integer"]
							, "Default"_o= 5
							, "Description"_o= "Test 1"
						}
					}
				}
				, [](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					co_return _Params["Integer"].f_Integer();
				}
			)
		;

		Section.f_RegisterCommand
			(
				{"Names"_o= _o["--test-remote-session"], "Description"_o= "Return 1 for a session through the app's listener."}
				, [](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					co_return _pCommandLine->m_ClientInfo.m_UTCOffsetSeconds ? 1 : 0;
				}
			)
		;

		Section.f_RegisterCommand
			(
				{"Names"_o= _o["--test-calling-host"], "Description"_o= "Return 1 when the command runs as the app's own host."}
				, [this](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					auto &CallingHostInfo = fg_GetCallingHostInfo();
					bool bOwnHost = CallingHostInfo.f_GetRealHostID() == mp_State.m_HostID;
					bool bLocalName = CallingHostInfo.f_GetHostInfo().m_FriendlyName.f_EndsWith("(Local Command line)");

					co_return bOwnHost && bLocalName ? 1 : 0;
				}
			)
		;

		Section.f_RegisterCommand
			(
				{"Names"_o= _o["--test-input"], "Description"_o= "Exercise redirected input subscriptions."}
				, [](CEJsonSorted _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					auto fTextInput = []
						{
							return g_ActorFunctor / [](NProcess::EStdInReaderOutputType _Type, CStrIO _Input) -> TCFuture<void>
								{
									co_return {};
								}
							;
						}
					;
					CActorSubscription Input = (co_await _pCommandLine->f_RegisterForStdIn(fTextInput(), NProcess::EStdInReaderFlag_None)).f_GetSubscription();
					auto DestroyInput = co_await fg_AsyncDestroy
						(
							[&Input]() -> TCFuture<void>
							{
								if (Input)
									co_await fg_Exchange(Input, nullptr)->f_Destroy();

								co_return {};
							}
						)
					;

					auto fOnResize = g_ActorFunctor / [](ICCommandLineControl::CScreenChange _ScreenChange) -> TCFuture<void>
						{
							co_return {};
						}
					;
					CActorSubscription Screen = (co_await _pCommandLine->f_RegisterForScreenChange(fg_Move(fOnResize))).f_GetSubscription();
					DMibExpectTrue(Screen);
					auto DestroyScreen = co_await fg_AsyncDestroy(fg_Move(Screen));

					co_await Input->f_Destroy();
					Input.f_Clear();

					for (bool bBinary : {false, true})
					{
						DMibTestPath(bBinary ? "Binary" : "Text");
						if (bBinary)
						{
							auto fOnInput = g_ActorFunctor / [](NProcess::EStdInReaderOutputType _Type, CIOByteVector _Input, CStr _Error) -> TCFuture<void>
								{
									co_return {};
								}
							;
							Input = (co_await _pCommandLine->f_RegisterForStdInBinary(fg_Move(fOnInput), NProcess::EStdInReaderFlag_None)).f_GetSubscription();
						}
						else
							Input = (co_await _pCommandLine->f_RegisterForStdIn(fTextInput(), NProcess::EStdInReaderFlag_None)).f_GetSubscription();

						DMibExpectTrue(Input);
						co_await Input->f_Destroy();
						Input.f_Clear();
					}

					co_return 0;
				}
			)
		;
	}

	TCFuture<void> CTestDistributedApp::fp_StartApp(NEncoding::CEJsonSorted const _Params)
	{
		co_return {};
	}

	TCFuture<void> CTestDistributedApp::fp_StopApp()
	{
		co_return {};
	}

	struct CDistributedApp_Tests : public NMib::NTest::CTest
	{
		void f_DoTests();

	private:
		void fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name, bool _bInProcessOnly = false);
	};

	void CDistributedApp_Tests::f_DoTests()
	{
		DMibTestSuite("Local socket paths")
		{
			using NFile::CFile;

			CDistributedAppActor_Settings Settings("SocketPaths");
			Settings.m_Enclave = "c2x3u37jzzYBtQGDh";
			Settings.m_RootDirectory = CFile::fs_GetProgramDirectory();
			while (Settings.m_RootDirectory.f_GetLen() < 200)
				Settings.m_RootDirectory /= "LongDeploymentDirectory";

			CStr PlainPath;
			for (bool bAuthenticated : {false, true})
			{
				DMibTestPath(bAuthenticated ? "Authenticated" : "Plain");
				auto Flags = ELocalSocketFlag::mc_EnclaveSpecific;
				if (bAuthenticated)
					Flags |= ELocalSocketFlag::mc_AuthenticatedUnix;

				CStr Path = Settings.f_GetLocalSocketFileName(Flags, Settings.m_Enclave);
				DMibExpect(Path.f_GetLen(), <=, aint(NSys::NNetwork::fg_GetMaxUnixSocketNameLength()));
				DMibExpect(Settings.f_GetLocalSocketWildcard(Flags), ==, Path.f_Replace(Settings.m_Enclave, "*"));
				if (bAuthenticated)
					DMibExpect(Path, !=, PlainPath);
				else
					PlainPath = Path;
#ifdef DPlatformFamily_Windows
				DMibExpectFalse(CFile::fs_GetDrive(Path).f_IsEmpty());
				DMibExpect(Path, ==, CFile::fs_GetExpandedPath(Path));
#endif
				CDistributedAppActor_Settings OtherRoot = Settings;
				OtherRoot.m_RootDirectory /= "Other";
				DMibExpect(OtherRoot.f_GetLocalSocketFileName(Flags, OtherRoot.m_Enclave), !=, Path);
				CDistributedAppActor_Settings OtherApp = Settings;
				OtherApp.m_AppName += "Other";
				DMibExpect(OtherApp.f_GetLocalSocketFileName(Flags, OtherApp.m_Enclave), !=, Path);
			}

			for (bool bAuthenticated : {false, true})
			{
				DMibTestPath(bAuthenticated ? "Earlier location authenticated" : "Earlier location plain");
				auto Flags = bAuthenticated ? ELocalSocketFlag::mc_AuthenticatedUnix : ELocalSocketFlag::mc_None;
				auto OtherFlags = bAuthenticated ? ELocalSocketFlag::mc_None : ELocalSocketFlag::mc_AuthenticatedUnix;

				// The long root directory puts the socket outside the root, in a directory named for the root
				CStr Path = Settings.f_GetLocalSocketFileName(Flags, Settings.m_Enclave);
				CStr FileName = CFile::fs_GetFile(Path);
				DMibExpectTrue(Settings.f_IsLocalSocketFileNameOfRoot(Path, Flags));
				DMibExpectTrue(Settings.f_IsLocalSocketFileNameOfRoot(Settings.m_RootDirectory / FileName, Flags));
				DMibExpectFalse(Settings.f_IsLocalSocketFileNameOfRoot(CFile::fs_GetProgramDirectory() / "Elsewhere" / FileName, Flags));
				DMibExpectFalse(Settings.f_IsLocalSocketFileNameOfRoot(Path, OtherFlags));

				DMibExpectTrue(Settings.f_IsLocalSocketHostnameOfRoot(Settings.f_GetLocalSocketHostname(Flags), Flags));
				DMibExpectTrue(Settings.f_IsLocalSocketHostnameOfRoot("UNIX(666):" + Settings.m_RootDirectory / FileName, Flags));
				DMibExpectFalse(Settings.f_IsLocalSocketHostnameOfRoot("UNIX(660):" + Settings.m_RootDirectory / FileName, Flags));
				DMibExpectFalse(Settings.f_IsLocalSocketHostnameOfRoot("UNIX:" + Path, Flags));

				CStr EarlierHostname = "UNIX(666):" + Settings.m_RootDirectory / FileName;
				auto EarlierURL = CDistributedAppActor_Settings::fs_GetLocalSocketURL(EarlierHostname, Flags);
				DMibExpectTrue(Settings.f_IsLocalSocketListenOfRoot(EarlierURL, Flags));
				DMibExpectFalse(Settings.f_IsLocalSocketListenOfRoot(CDistributedAppActor_Settings::fs_GetLocalSocketURL(EarlierHostname, OtherFlags), Flags));

				auto CustomPathURL = EarlierURL;
				CustomPathURL.f_SetPath(fg_CreateVector<CStr>("Custom"));
				DMibExpectFalse(Settings.f_IsLocalSocketListenOfRoot(CustomPathURL, Flags));

				CStr RelativeRoot = CFile::fs_MakePathRelative(Settings.m_RootDirectory, CFile::fs_GetCurrentDirectory());
				DMibExpectTrue(Settings.f_IsLocalSocketFileNameOfRoot(RelativeRoot / FileName, Flags));
				DMibExpectTrue(Settings.f_IsLocalSocketFileNameOfRoot(Settings.m_RootDirectory / "." / FileName, Flags));

				CDistributedAppActor_Settings RelativeSettings = Settings;
				RelativeSettings.m_RootDirectory = RelativeRoot;
				DMibExpectTrue(RelativeSettings.f_IsLocalSocketFileNameOfRoot(Settings.m_RootDirectory / FileName, Flags));
			}

#ifndef DPlatformFamily_Windows
			{
				DMibTestPath("Earlier location through a link");

				CStr Base = CFile::fs_GetProgramDirectory() / "TestDistAppSocketRootLink";
				fg_TestAddCleanupPath(Base);
				if (CFile::fs_FileExists(Base, NFile::EFileAttrib_Directory | NFile::EFileAttrib_File | NFile::EFileAttrib_Link))
					CFile::fs_DeleteDirectoryRecursive(Base);

				CFile::fs_CreateDirectory(Base / "Root");
				CFile::fs_CreateSymbolicLink(Base / "Root", Base / "Link", NFile::EFileAttrib_Directory, NFile::ESymbolicLinkFlag_None);

				CDistributedAppActor_Settings LinkSettings("SocketPaths");
				CDistributedAppActor_Settings RootSettings("SocketPaths");
				LinkSettings.m_RootDirectory = Base / "Link";
				RootSettings.m_RootDirectory = Base / "Root";

				for (bool bAuthenticated : {false, true})
				{
					DMibTestPath(bAuthenticated ? "Authenticated" : "Plain");
					auto Flags = bAuthenticated ? ELocalSocketFlag::mc_AuthenticatedUnix : ELocalSocketFlag::mc_None;
					CStr FileName = bAuthenticated ? "SocketPaths.wsa.socket" : "SocketPaths.socket";

					DMibExpectTrue(LinkSettings.f_IsLocalSocketFileNameOfRoot(Base / "Root" / FileName, Flags));
					DMibExpectTrue(RootSettings.f_IsLocalSocketFileNameOfRoot(Base / "Link" / FileName, Flags));
					DMibExpectFalse(LinkSettings.f_IsLocalSocketFileNameOfRoot(Base / FileName, Flags));
					DMibExpectFalse(LinkSettings.f_IsLocalSocketFileNameOfRoot(Base / "Missing" / FileName, Flags));
				}

				CFile::fs_DeleteFile(Base / "Link");
				CFile::fs_DeleteDirectoryRecursive(Base);
			}

			for (bool bAuthenticated : {false, true})
			{
				DMibTestPath(bAuthenticated ? "Staged wildcard below a directory with a wildcard authenticated" : "Staged wildcard below a directory with a wildcard plain");
				auto Flags = ELocalSocketFlag::mc_EnclaveSpecific;
				if (bAuthenticated)
					Flags |= ELocalSocketFlag::mc_AuthenticatedUnix;

				// The paths are only resolved, so the root doesn't exist. It is short, so that the socket stays in it wherever the test is deployed
				CDistributedAppActor_Settings WildcardSettings("SocketPaths");
				WildcardSettings.m_Enclave = Settings.m_Enclave;
				WildcardSettings.m_RootDirectory = "/tmp/TestDistApp*Root";

				CStr Path = WildcardSettings.f_GetLocalSocketFileName(Flags, WildcardSettings.m_Enclave);
				CStr Directory = CFile::fs_GetPath(Path);

				// A socket prefix moves the socket to a directory under the prefix, which the test doesn't choose
				if (WildcardSettings.f_GetLocalSocketPrefixDirectory().f_IsEmpty())
					DMibAssert(Directory, ==, WildcardSettings.m_RootDirectory);

				CStr StagedWildcard = WildcardSettings.f_GetStagedLocalSocketWildcard(Flags);
				DMibExpect(CFile::fs_GetPath(StagedWildcard), ==, Directory);
				DMibExpect
					(
						NStr::fg_StrMatchWildcard(CFile::fs_GetFile(NSys::NNetwork::fg_GetStagedUnixSocketPath(Path)).f_GetStr(), CFile::fs_GetFile(StagedWildcard).f_GetStr())
						, ==
						, NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted
					)
				;
			}
#endif

			for (bool bAuthenticated : {false, true})
			{
				DMibTestPath(bAuthenticated ? "Reserved URL characters authenticated" : "Reserved URL characters plain");
				auto Flags = bAuthenticated ? ELocalSocketFlag::mc_AuthenticatedUnix : ELocalSocketFlag::mc_None;

				CDistributedAppActor_Settings ReservedSettings("SocketPaths");
				ReservedSettings.m_RootDirectory = CFile::fs_GetProgramDirectory() / "a@b%41[c]";

				DMibExpect(ReservedSettings.f_GetLocalSocketHostname(Flags), ==, "UNIX(666):" + ReservedSettings.f_GetLocalSocketFileName(Flags, {}));

				// Build the host in the root directory, so that the test covers the reserved characters even where a socket prefix or a long root would place
				// the socket elsewhere
				CStr Hostname = "UNIX(666):" + ReservedSettings.m_RootDirectory / (bAuthenticated ? "SocketPaths.wsa.socket" : "SocketPaths.socket");
				DMibExpectTrue(ReservedSettings.f_IsLocalSocketHostnameOfRoot(Hostname, Flags));

				auto URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL(Hostname, Flags);
				DMibExpectTrue(URL.f_IsValid());
				DMibExpectFalse(URL.f_HasUsername());
				DMibExpect(URL.f_GetHost(), ==, Hostname);
				DMibExpectTrue(ReservedSettings.f_IsLocalSocketListenOfRoot(URL, Flags));

				NWeb::NHTTP::CURL Stored(URL.f_Encode());
				DMibExpect(Stored, ==, URL);
				DMibExpectTrue(ReservedSettings.f_IsLocalSocketListenOfRoot(Stored, Flags));
			}

#ifndef DPlatformFamily_Windows
			DMibTestCategory("Prepare missing prefix")
			{
				CStr Prefix = CFile::fs_GetProgramDirectory() / "TestDistAppSocketPrefix";
				fg_TestAddCleanupPath(Prefix);
				if (CFile::fs_FileExists(Prefix))
					CFile::fs_DeleteDirectoryRecursive(Prefix);

				CStr Directory = Prefix / "Nested" / "Root";
				CDistributedAppActor_Settings::fs_PrepareLocalSocketDirectory(Directory);

				for (auto &Path : {Prefix, Prefix / "Nested", Directory})
				{
					DMibTestPath(CFile::fs_MakePathRelative(Path, CFile::fs_GetProgramDirectory()));
					auto Attributes = CFile::fs_GetAttributesOnLink(Path);
					DMibExpectTrue(Attributes & NFile::EFileAttrib_Directory);
					DMibExpectFalse(Attributes & (NFile::EFileAttrib_GroupWrite | NFile::EFileAttrib_EveryoneWrite));
					DMibExpectFalse(Attributes & (NFile::EFileAttrib_GroupRead | NFile::EFileAttrib_EveryoneRead));
					DMibExpectTrue(Attributes & NFile::EFileAttrib_EveryoneExecute);
				}

				CFile::fs_DeleteDirectoryRecursive(Prefix);
			};
#endif
		};

		DMibTestSuite("Enclave socket sweep")
		{
			using NFile::CFile;

			CStr Name = "TestDistAppEnclaveSweep";
			CStr RootDirectory = CFile::fs_GetProgramDirectory() / Name;
			fg_TestAddCleanupPath(RootDirectory);
			if (CFile::fs_FileExists(RootDirectory))
				CFile::fs_DeleteDirectoryRecursive(RootDirectory);
			CFile::fs_CreateDirectory(RootDirectory);

			// Other instances' paths are only enclave specific with an enclave of their own
			CDistributedAppActor_Settings Settings(Name);
			Settings.m_RootDirectory = RootDirectory;
			Settings.m_Enclave = NCryptography::fg_RandomID();
			auto fOtherSocketPath = [&]
				{
					return Settings.f_GetLocalSocketFileName(ELocalSocketFlag::mc_EnclaveSpecific, NCryptography::fg_RandomID());
				}
			;

			CStr StalePath = fOtherSocketPath();
			CFile::fs_CreateDirectoryForFile(StalePath); // Long root directories put sockets in a fallback directory
			CFile::fs_WriteStringToFile(StalePath, "Stale", false);

			CStr LivePath = fOtherSocketPath();
			NNetwork::CSocket Live;
			Live.f_Listen(NNetwork::CSocket::fs_ResolveAddress("UNIX:" + LivePath), nullptr, NNetwork::ENetFlag_None);

			CStr OldStagedPath = NSys::NNetwork::fg_GetStagedUnixSocketPath(fOtherSocketPath());
			CFile::fs_WriteStringToFile(OldStagedPath, "Leftover", false);
			CFile::fs_SetWriteTime(OldStagedPath, NTime::CTime::fs_NowUTC() - NTime::CTimeSpanConvert::fs_CreateMinuteSpan(10));

			CStr StartingStagedPath = NSys::NNetwork::fg_GetStagedUnixSocketPath(fOtherSocketPath());
			CFile::fs_WriteStringToFile(StartingStagedPath, "Starting", false);

			CActorRunLoopTestHelper RunLoopHelper;
			auto AppActor = fg_ConstructActor<CTestDistributedApp>(Name, false, NCryptography::fg_RandomID());
			AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJsonSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess).f_CallSync(RunLoopHelper.m_pRunLoop);

			// The sweep runs in the background of the startup
			NTime::CStopwatch Stopwatch{true};
			while ((CFile::fs_FileExists(StalePath) || CFile::fs_FileExists(OldStagedPath)) && Stopwatch.f_GetTime() < 60.0)
				NSys::fg_Thread_Sleep(0.05f);

			DMibExpectFalse(CFile::fs_FileExists(StalePath));
			DMibExpectFalse(CFile::fs_FileExists(OldStagedPath));
			DMibExpectTrue(CFile::fs_FileExists(LivePath));
			DMibExpectTrue(CFile::fs_FileExists(StartingStagedPath));

			AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
			Live.f_Close();
		};

		DMibTestSuite("Relocated listen send window")
		{
			using NFile::CFile;

			CStr Name = "TestDistAppRelocatedSendWindow";
			CStr TestDirectory = CFile::fs_GetProgramDirectory() / Name;
			fg_TestAddCleanupPath(TestDirectory);
			if (CFile::fs_FileExists(TestDirectory))
				CFile::fs_DeleteDirectoryRecursive(TestDirectory);

			// A root too long to hold the socket puts it outside the root, which makes the socket in the root an earlier location
			CStr RootDirectory = TestDirectory;
			while (RootDirectory.f_GetLen() < 200)
				RootDirectory /= "LongDeploymentDirectory";

			CActorRunLoopTestHelper RunLoopHelper;

			auto fStartAndStopApp = [&]() -> bool
				{
					auto AppActor = fg_ConstructActor<CTestDistributedApp>(Name, false, CStr(), RootDirectory);
					bool bStarted = true;
					try
					{
						AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJsonSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess)
							.f_CallSync(RunLoopHelper.m_pRunLoop)
						;
					}
					catch (NException::CException const &)
					{
						bStarted = false;
					}

					AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
					return bStarted;
				}
			;

			using CListenConfigs = TCMap<CDistributedActorTrustManager_Address, ICDistributedActorTrustManagerDatabase::CListenConfig>;

			// Call only after the app is destroyed, so that this database actor doesn't race the app's
			auto fUpdateListens = [&](TCFunction<void (TCActor<ICDistributedActorTrustManagerDatabase> const &_Database)> const &_fUpdate) -> CListenConfigs
				{
					TCActor<ICDistributedActorTrustManagerDatabase> Database = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JsonDirectory>
						(
							RootDirectory / fg_Format("TrustDatabase.{}", Name)
						)
					;

					// The JSON directory database rejects other operations until f_GetBasicConfig has run
					Database(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig).f_CallSync(RunLoopHelper.m_pRunLoop);

					if (_fUpdate)
						_fUpdate(Database);

					auto Listens = Database(&ICDistributedActorTrustManagerDatabase::f_EnumListenConfigs).f_CallSync(RunLoopHelper.m_pRunLoop);

					Database->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
					return Listens;
				}
			;

			DMibAssertTrue(fStartAndStopApp());

			CDistributedAppActor_Settings Settings(Name);
			Settings.m_RootDirectory = RootDirectory;

			auto Listens = fUpdateListens({});

			auto Flags = ELocalSocketFlag::mc_None;
			CDistributedActorTrustManager_Address CurrentAddress;
			for (auto TransportFlags : {ELocalSocketFlag::mc_None, ELocalSocketFlag::mc_AuthenticatedUnix})
			{
				CDistributedActorTrustManager_Address Address;
				Address.m_URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL(Settings.f_GetLocalSocketHostname(TransportFlags), TransportFlags);
				if (!Listens.f_FindEqual(Address))
					continue;

				Flags = TransportFlags;
				CurrentAddress = Address;
			}

			DMibAssertTrue(Listens.f_FindEqual(CurrentAddress) != nullptr);

			CStr SocketPath = Settings.f_GetLocalSocketFileName(Flags, {});
			CStr SocketDirectory = CFile::fs_GetPath(SocketPath);
			DMibAssert(SocketDirectory, !=, RootDirectory);
			fg_TestAddCleanupPath(SocketDirectory);

			CDistributedActorTrustManager_Address EarlierAddress;
			EarlierAddress.m_URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL("UNIX(666):" + RootDirectory / CFile::fs_GetFile(SocketPath), Flags);

			// An earlier listen of the unused transport is removed too, and its send window stays pending
			auto OtherFlags = Flags == ELocalSocketFlag::mc_None ? ELocalSocketFlag::mc_AuthenticatedUnix : ELocalSocketFlag::mc_None;
			CDistributedActorTrustManager_Address EarlierOtherAddress;
			EarlierOtherAddress.m_URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL
				(
					"UNIX(666):" + RootDirectory / CFile::fs_GetFile(Settings.f_GetLocalSocketFileName(OtherFlags, {}))
					, OtherFlags
				)
			;

			constexpr uint64 c_SendWindowBytes = uint64(3) << 20;
			constexpr uint64 c_OtherSendWindowBytes = uint64(5) << 20;

			ICDistributedActorTrustManagerDatabase::CListenConfig EarlierConfig;
			EarlierConfig.m_SendWindowBytes = c_SendWindowBytes;

			ICDistributedActorTrustManagerDatabase::CListenConfig EarlierOtherConfig;
			EarlierOtherConfig.m_SendWindowBytes = c_OtherSendWindowBytes;

			fUpdateListens
				(
					[&](TCActor<ICDistributedActorTrustManagerDatabase> const &_Database)
					{
						_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveListenConfig, CurrentAddress).f_CallSync(RunLoopHelper.m_pRunLoop);
						_Database(&ICDistributedActorTrustManagerDatabase::f_AddListenConfig, EarlierAddress, EarlierConfig).f_CallSync(RunLoopHelper.m_pRunLoop);
						_Database(&ICDistributedActorTrustManagerDatabase::f_AddListenConfig, EarlierOtherAddress, EarlierOtherConfig).f_CallSync(RunLoopHelper.m_pRunLoop);
					}
				)
			;

			CStr StatePath = RootDirectory / fg_Format("{}State.json", Name);
			CStr OtherScheme = CurrentAddress.m_URL.f_GetScheme() == "wsa" ? "wss" : "wsa";

			{
				DMibTestPath("Replacement fails to listen");

				if (CFile::fs_FileExists(SocketPath))
					CFile::fs_DeleteFile(SocketPath);
				CFile::fs_CreateDirectory(SocketPath);

				DMibExpectFalse(fStartAndStopApp());

				CFile::fs_DeleteDirectory(SocketPath);

				auto ListensAfterFailure = fUpdateListens({});
				DMibExpectFalse(ListensAfterFailure.f_FindEqual(EarlierAddress));
				DMibExpectFalse(ListensAfterFailure.f_FindEqual(EarlierOtherAddress));
				DMibExpectFalse(ListensAfterFailure.f_FindEqual(CurrentAddress));
			}

			auto fGetPendingSendWindow = [&](CStr const &_Scheme) -> uint64
				{
					auto State = NEncoding::CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(StatePath), StatePath);
					auto const *pSendWindows = State.f_GetMember("RelocatedCommandLineListenSendWindow", NEncoding::EJsonType_Object);
					if (!pSendWindows)
						return 0;

					auto const *pSendWindow = pSendWindows->f_GetMember(_Scheme, NEncoding::EJsonType_Integer);
					if (!pSendWindow)
						return 0;

					return uint64(pSendWindow->f_Integer());
				}
			;

			{
				DMibTestPath("Pending windows of both transports");

				DMibExpect(fGetPendingSendWindow(CurrentAddress.m_URL.f_GetScheme()), ==, c_SendWindowBytes);
				DMibExpect(fGetPendingSendWindow(OtherScheme), ==, c_OtherSendWindowBytes);
			}

			{
				DMibTestPath("Replacement listens");

				DMibAssertTrue(fStartAndStopApp());

				auto ListensAfterStart = fUpdateListens({});
				DMibExpectFalse(ListensAfterStart.f_FindEqual(EarlierAddress));

				auto *pCurrent = ListensAfterStart.f_FindEqual(CurrentAddress);
				DMibAssertTrue(pCurrent != nullptr);
				DMibExpect(pCurrent->m_SendWindowBytes, ==, c_SendWindowBytes);
			}

			{
				DMibTestPath("Pending window of the other transport");

				auto State = NEncoding::CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(StatePath), StatePath);
				State["RelocatedCommandLineListenSendWindow"] = NEncoding::CEJsonSorted(NEncoding::EJsonType_Object);
				State["RelocatedCommandLineListenSendWindow"][OtherScheme] = int64(c_SendWindowBytes);
				CFile::fs_WriteStringToFile(StatePath, State.f_ToString(), false);

				fUpdateListens
					(
						[&](TCActor<ICDistributedActorTrustManagerDatabase> const &_Database)
						{
							_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveListenConfig, CurrentAddress).f_CallSync(RunLoopHelper.m_pRunLoop);
						}
					)
				;

				DMibAssertTrue(fStartAndStopApp());

				auto ListensAfterStart = fUpdateListens({});
				auto *pCurrent = ListensAfterStart.f_FindEqual(CurrentAddress);
				DMibAssertTrue(pCurrent != nullptr);
				DMibExpect(pCurrent->m_SendWindowBytes, ==, uint64(0));

				auto StateAfterStart = NEncoding::CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(StatePath), StatePath);
				auto const *pSendWindows = StateAfterStart.f_GetMember("RelocatedCommandLineListenSendWindow", NEncoding::EJsonType_Object);
				DMibAssertTrue(pSendWindows != nullptr);
				auto const *pOtherWindow = pSendWindows->f_GetMember(OtherScheme, NEncoding::EJsonType_Integer);
				DMibAssertTrue(pOtherWindow != nullptr);
				DMibExpect(uint64(pOtherWindow->f_Integer()), ==, c_SendWindowBytes);
			}

			CFile::fs_DeleteDirectoryRecursive(SocketDirectory);
			CFile::fs_DeleteDirectoryRecursive(TestDirectory);
		};

#ifndef DPlatformFamily_Windows
		DMibTestSuite("Relocated listen through another root spelling")
		{
			using NFile::CFile;

			CStr Name = "TestDistAppRelocatedSpelling";
			CStr TestDirectory = CFile::fs_GetProgramDirectory() / Name;
			fg_TestAddCleanupPath(TestDirectory);
			if (CFile::fs_FileExists(TestDirectory, NFile::EFileAttrib_Directory | NFile::EFileAttrib_File | NFile::EFileAttrib_Link))
				CFile::fs_DeleteDirectoryRecursive(TestDirectory);

			// Long roots put the sockets in directories named for the spelling of the root, in a fallback or under a socket prefix
			CStr SubPath = "LongDeploymentDirectory";
			while (SubPath.f_GetLen() < 200)
				SubPath /= "LongDeploymentDirectory";

			CStr RealRoot = TestDirectory / "Root" / SubPath;
			CStr LinkRoot = TestDirectory / "Link" / SubPath;
			CFile::fs_CreateDirectory(RealRoot);
			CFile::fs_CreateSymbolicLink(TestDirectory / "Root", TestDirectory / "Link", NFile::EFileAttrib_Directory, NFile::ESymbolicLinkFlag_None);

			CActorRunLoopTestHelper RunLoopHelper;

			auto fStartAndStopApp = [&](CStr const &_RootDirectory)
				{
					auto AppActor = fg_ConstructActor<CTestDistributedApp>(Name, false, CStr(), _RootDirectory);
					auto Destroy = g_OnScopeExit / [&]
						{
							AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
						}
					;

					AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJsonSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess)
						.f_CallSync(RunLoopHelper.m_pRunLoop)
					;
				}
			;

			// Call only after the app is destroyed, so that this database actor doesn't race the app's
			auto fGetListens = [&]
				{
					TCActor<ICDistributedActorTrustManagerDatabase> Database = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JsonDirectory>
						(
							RealRoot / fg_Format("TrustDatabase.{}", Name)
						)
					;

					Database(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig).f_CallSync(RunLoopHelper.m_pRunLoop);
					auto Listens = Database(&ICDistributedActorTrustManagerDatabase::f_EnumListenConfigs).f_CallSync(RunLoopHelper.m_pRunLoop);

					Database->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
					return Listens;
				}
			;

			CDistributedAppActor_Settings LinkSettings(Name);
			LinkSettings.m_RootDirectory = LinkRoot;
			CDistributedAppActor_Settings RealSettings(Name);
			RealSettings.m_RootDirectory = RealRoot;

			for (auto Flags : {ELocalSocketFlag::mc_None, ELocalSocketFlag::mc_AuthenticatedUnix})
			{
				for (auto *pSettings : {&LinkSettings, &RealSettings})
				{
					DMibTestPath(fg_Format("{} {}", Flags == ELocalSocketFlag::mc_None ? "Plain" : "Authenticated", pSettings == &LinkSettings ? "link" : "real path"));
					CStr SocketDirectory = CFile::fs_GetPath(pSettings->f_GetLocalSocketFileName(Flags, {}));
					DMibAssert(SocketDirectory, !=, pSettings->m_RootDirectory);
					fg_TestAddCleanupPath(SocketDirectory);
				}
			}

			fStartAndStopApp(LinkRoot);

			auto Listens = fGetListens();

			auto Flags = ELocalSocketFlag::mc_None;
			CDistributedActorTrustManager_Address LinkAddress;
			for (auto TransportFlags : {ELocalSocketFlag::mc_None, ELocalSocketFlag::mc_AuthenticatedUnix})
			{
				CDistributedActorTrustManager_Address Address;
				Address.m_URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL(LinkSettings.f_GetLocalSocketHostname(TransportFlags), TransportFlags);
				if (!Listens.f_FindEqual(Address))
					continue;

				Flags = TransportFlags;
				LinkAddress = Address;
			}

			DMibAssertTrue(Listens.f_FindEqual(LinkAddress) != nullptr);

			CDistributedActorTrustManager_Address RealAddress;
			RealAddress.m_URL = CDistributedAppActor_Settings::fs_GetLocalSocketURL(RealSettings.f_GetLocalSocketHostname(Flags), Flags);
			DMibAssertFalse(RealAddress == LinkAddress);

			fStartAndStopApp(RealRoot);

			auto ListensAfterRealStart = fGetListens();
			DMibExpectFalse(ListensAfterRealStart.f_FindEqual(LinkAddress));
			DMibExpectTrue(ListensAfterRealStart.f_FindEqual(RealAddress));

			for (auto TransportFlags : {ELocalSocketFlag::mc_None, ELocalSocketFlag::mc_AuthenticatedUnix})
			{
				for (auto *pSettings : {&LinkSettings, &RealSettings})
				{
					CStr SocketDirectory = CFile::fs_GetPath(pSettings->f_GetLocalSocketFileName(TransportFlags, {}));
					if (CFile::fs_FileExists(SocketDirectory, NFile::EFileAttrib_Directory))
						CFile::fs_DeleteDirectoryRecursive(SocketDirectory);
				}
			}

			CFile::fs_DeleteFile(TestDirectory / "Link");
			CFile::fs_DeleteDirectoryRecursive(TestDirectory);
		};
#endif

		DMibTestCategory("Send window")
		{
			DMibTestSuite("Parse")
			{
				auto fParse = [](CStr const &_Text) -> uint64
					{
						DMibTestPath(_Text.f_IsEmpty() ? CStr("Empty") : _Text);

						uint64 nBytes = 0;
						CStr Error;
						DMibExpectTrue(fg_ParseSendWindow(_Text, nBytes, Error));
						return nBytes;
					}
				;

				DMibExpect(fParse(""), ==, uint64(0));
				DMibExpect(fParse("default"), ==, uint64(0));
				DMibExpect(fParse("1048576"), ==, uint64(1048576));
				DMibExpect(fParse("20 MiB"), ==, uint64(20) << 20);
				DMibExpect(fParse("2m"), ==, uint64(2) << 20);
				DMibExpect(fParse("512KB"), ==, uint64(512000));
				DMibExpect(fParse("1gib"), ==, uint64(1) << 30);

				DMibExpect(fParse("10gbit@10ms"), ==, uint64(25000000));
				DMibExpect(fParse("1gbps@20"), ==, uint64(5000000));
				DMibExpect(fParse("100mbit@1s"), ==, uint64(25000000));
				DMibExpect(fParse("8 Mbit @ 500 us"), ==, uint64(1000));

				{
					DMibTestPath("Invalid");

					uint64 nBytes = 0;
					CStr Error;
					DMibExpectTrue(!fg_ParseSendWindow("10 parsecs", nBytes, Error));
					DMibExpectTrue(!fg_ParseSendWindow("10gbit@", nBytes, Error));
					DMibExpectTrue(!fg_ParseSendWindow("5 TiB", nBytes, Error));
				}

				{
					DMibTestPath("Format");

					DMibExpect(fg_FormatSendWindow(0), ==, CStr("default"));
					DMibExpect(fg_FormatSendWindow(uint64(20) << 20), ==, CStr("20 MiB"));
					DMibExpect(fg_FormatSendWindow(1536), ==, CStr("1536 B"));
					DMibExpect(fg_FormatSendWindow(25000000), ==, CStr("25000000 B"));
				}
			};
		};

		DMibTestCategory("Command line")
		{
			DMibTestSuite("RedirectedInputResize")
			{
				if (!(NTest::fg_TestReportFlags() & NTest::ETestReportFlag_ProcessRecursive))
				{
					CStr Output;
					CStr Error;
					uint32 ExitCode = 1;
					bool bLaunched = NProcess::CProcessLaunch::fs_LaunchBlock
						(
							NFile::CFile::fs_GetProgramPath()
							, {"--test", NTest::fg_TestGetCurrentPath(), "--process-recursive"}
							, Output
							, Error
							, ExitCode
						)
					;
					DMibExpectTrue(bLaunched);
					DMibExpect(ExitCode, ==, 0);
					if (ExitCode)
						DMibConErrOut("{}{}", Output, Error);

					return;
				}

#ifdef DPlatformFamily_Windows
				DMibAssert(GetFileType(GetStdHandle(STD_INPUT_HANDLE)), ==, DWORD(FILE_TYPE_PIPE));
#endif
				fp_RunTests
					(
						[](CDistributedAppCommandLineClient &_Client)
						{
							auto Result = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-input"));
							DMibExpect(Result, ==, 0);
						}
						, "TestDistAppRedirectedInput-{}"_f << NCryptography::fg_FastRandomID()
						, true
					)
				;
			};

			DMibTestSuite("Actor")
			{
				fp_RunTests
					(
						[](CDistributedAppCommandLineClient &_Client)
						{
							CEJsonSorted RunParams;

							DMibTestPath("General");
							aint Ret = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-actor", "--integer", "66"));
							DMibExpect(Ret, ==, 66);
						}
						, "TestDistAppCommandLineActor"
					)
				;
			};

			DMibTestSuite("In process")
			{
				// An app that runs in the client's process runs its commands without connecting to its own listener
				fp_RunTests
					(
						[](CDistributedAppCommandLineClient &_Client)
						{
							aint Ret = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-remote-session"));
							DMibExpect(Ret, ==, 0);

							aint OwnHost = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-calling-host"));
							DMibExpect(OwnHost, ==, 1);
						}
						, "TestDistAppCommandLineInProcess"
					)
				;
			};
		};
	}

	void CDistributedApp_Tests::fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name, bool _bInProcessOnly)
	{
		CStr ConfigDir = NFile::CFile::fs_GetProgramDirectory() / _Name;
		fg_TestAddCleanupPath(ConfigDir);

		for (umint iAttempt = 0; iAttempt < 5; ++iAttempt)
		{
			try
			{
				if (NFile::CFile::fs_FileExists(ConfigDir))
					NFile::CFile::fs_DeleteDirectoryRecursive(ConfigDir);
				break;
			}
			catch (NFile::CExceptionFile const &)
			{
			}
		}

		CActorRunLoopTestHelper RunLoopHelper;

		auto AppActor = fg_ConstructActor<CTestDistributedApp>(_Name, _bInProcessOnly);
		AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJsonSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess).f_CallSync(RunLoopHelper.m_pRunLoop);
		CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient, RunLoopHelper.m_pRunLoop, nullptr).f_CallSync(RunLoopHelper.m_pRunLoop);

		_fTests(CommandLineClient);

		AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
	}

	DMibTestRegister(CDistributedApp_Tests, Malterlib::Concurrency);
}
