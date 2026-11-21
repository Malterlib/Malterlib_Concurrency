// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTestHelpers>
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

	umint g_RequestRunLoopThreadID = 0; // Written and read only on the thread that waits on the run loop of the command line client

	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp(CStr const &_Name, bool _bInProcessOnly = false, CStr const &_Enclave = {});

		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override;

		TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) override;

		TCFuture<void> fp_StopApp() override;
	};

	CTestDistributedApp::CTestDistributedApp(CStr const &_Name, bool _bInProcessOnly, CStr const &_Enclave)
		: CDistributedAppActor
		(
			CDistributedAppActor_Settings(_Name)
			.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() / _Name)
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
				{
					"Names"_o= _o["--test-request"]
					, "Description"_o= "Return the sum the command line answers a request with."
					, "Options"_o=
					{
						"Integer?"_o=
						{
							"Names"_o= _o["--integer"]
							, "Default"_o= 5
							, "Description"_o= "Added to the value of the request."
						}
					}
				}
				, [](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					if (!_pCommandLine->f_SupportsRequest())
						co_return 1000;

					CEJsonSorted Response = co_await _pCommandLine->f_Request({"Value"_= 30});

					co_return Response["Sum"].f_Integer();
				}
				, EDistributedAppCommandFlag_None
				, [](CEJsonSorted _Params, CEJsonSorted _Request, NStorage::TCSharedPointer<CRunLoop> _pRunLoop) -> TCFuture<CEJsonSorted>
				{
					co_return {"Sum"_= _Request["Value"].f_Integer() + _Params["Integer"].f_Integer()};
				}
			)
		;

		Section.f_RegisterCommand
			(
				{"Names"_o= _o["--test-request-run-loop"], "Description"_o= "Return 1 when a request handler ran work on the run loop of the command line client."}
				, [](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					CEJsonSorted Response = co_await _pCommandLine->f_Request({});

					co_return Response["Ran"].f_Boolean() ? 1 : 0;
				}
				, EDistributedAppCommandFlag_None
				, [](CEJsonSorted _Params, CEJsonSorted _Request, NStorage::TCSharedPointer<CRunLoop> _pRunLoop) -> TCFuture<CEJsonSorted>
				{
					TCPromiseFuturePair<void> Ran;

					_pRunLoop->f_Dispatcher()
						(
							[Promise = Ran.m_Promise](CConcurrencyThreadLocal &_ThreadLocal)
							{
								g_RequestRunLoopThreadID = NSys::fg_Thread_GetCurrentUID();
								Promise.f_SetResult();
							}
						)
					;

					co_await fg_Move(Ran.m_Future);

					co_return {"Ran"_= true};
				}
			)
		;

		Section.f_RegisterCommand
			(
				{"Names"_o= _o["--test-request-unhandled"], "Description"_o= "Return 1 when a request of a command without a handler fails."}
				, [](NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
				{
					auto Response = co_await _pCommandLine->f_Request({"Value"_= 30}).f_Wrap();

					co_return !Response && Response.f_GetExceptionStr().f_Find("The command does not handle requests") >= 0 ? 1 : 0;
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
		// The client the tests get: the one of the app's own process, or one that connects to the app's listener the way a
		// command line in another process does
		enum class EClient
		{
			mc_InProcess
			, mc_Listener
		};

		void fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name, bool _bInProcessOnly = false, EClient _Client = EClient::mc_InProcess);
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

							{
								DMibTestPath("Request");
								aint Sum = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request", "--integer", "12"));
								DMibExpect(Sum, ==, 42);

								aint Unhandled = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request-unhandled"));
								DMibExpect(Unhandled, ==, 1);

								g_RequestRunLoopThreadID = 0;
								aint RanOnRunLoop = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request-run-loop"));
								DMibExpect(RanOnRunLoop, ==, 1);
								DMibExpect(g_RequestRunLoopThreadID, ==, NSys::fg_Thread_GetCurrentUID());
							}
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

							aint Sum = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request"));
							DMibExpect(Sum, ==, 35);
						}
						, "TestDistAppCommandLineInProcess"
					)
				;
			};

			DMibTestSuite("Through listener")
			{
				// The request goes over the connection to the app, with the handler on the app actor of the client's side
				fp_RunTests
					(
						[](CDistributedAppCommandLineClient &_Client)
						{
							aint Remote = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-remote-session"));
							DMibExpect(Remote, ==, 1);

							aint Sum = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request", "--integer", "12"));
							DMibExpect(Sum, ==, 42);

							aint Unhandled = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request-unhandled"));
							DMibExpect(Unhandled, ==, 1);

							g_RequestRunLoopThreadID = 0;
							aint RanOnRunLoop = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test-request-run-loop"));
							DMibExpect(RanOnRunLoop, ==, 1);
							DMibExpect(g_RequestRunLoopThreadID, ==, NSys::fg_Thread_GetCurrentUID());
						}
						, "TestDistAppCommandLineListener"
						, false
						, EClient::mc_Listener
					)
				;
			};
		};
	}

	void CDistributedApp_Tests::fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name, bool _bInProcessOnly, EClient _Client)
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
		NStorage::TCOptional<CDistributedAppCommandLineClient> ListenerClient;
		CDistributedAppActor::FMutateCommandLine fMutateCommandLine;

		// A client made without the app's actor has no in-process shortcut, so it connects to the listener
		if (_Client == EClient::mc_Listener)
		{
			fMutateCommandLine = [&ListenerClient, pRunLoop = RunLoopHelper.m_pRunLoop]
				(NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> _pSpecification, CDistributedAppActor_Settings _Settings) -> TCFuture<void>
				{
					ListenerClient = fg_Construct(_Settings, fg_Move(_pSpecification), NContainer::TCMap<CStr, CStr>{}, pRunLoop);
					return g_Void;
				}
			;
		}

		CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient, RunLoopHelper.m_pRunLoop, fg_Move(fMutateCommandLine))
			.f_CallSync(RunLoopHelper.m_pRunLoop)
		;

		if (ListenerClient)
		{
			_fTests(*ListenerClient);
			ListenerClient.f_Clear();
		}
		else
			_fTests(CommandLineClient);

		AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
	}

	DMibTestRegister(CDistributedApp_Tests, Malterlib::Concurrency);
}
