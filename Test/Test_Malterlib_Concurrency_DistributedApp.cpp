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
#include <Windows.h>
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
		CInitReconnectDelay()
		{
			// Override reconnect delay for whole process
			CDistributedAppActor_Settings::fs_GetGlobalDefaultSettings().m_ReconnectDelay = 1_ms;
		}
	};

	assure_used CInitReconnectDelay g_InitReconnectDelay;

	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp(CStr const &_Name, bool _bInProcessOnly = false)
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings(_Name)
				.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() / _Name)
				.f_SeparateDistributionManager(true)
				.f_InProcessCommandLineOnly(_bInProcessOnly)
				.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
				.f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality_None)
			)
		{
		}

		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override
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
					{"Names"_o= _o["--test-input"], "Description"_o= "Exercise redirected input subscriptions."}
					, [](CEJsonSorted, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) -> TCFuture<uint32>
					{
						auto fTextInput = []
							{
								return g_ActorFunctor / [](NProcess::EStdInReaderOutputType, CStrIO) -> TCFuture<void>
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

						auto OnResize = g_ActorFunctor / [](ICCommandLineControl::CScreenChange) -> TCFuture<void>
							{
								co_return {};
							}
						;
						CActorSubscription Screen = (co_await _pCommandLine->f_RegisterForScreenChange(fg_Move(OnResize))).f_GetSubscription();
						DMibExpectTrue(Screen);
						auto DestroyScreen = co_await fg_AsyncDestroy(fg_Move(Screen));

						co_await Input->f_Destroy();
						Input.f_Clear();

						for (bool bBinary : {false, true})
						{
							DMibTestPath(bBinary ? "Binary" : "Text");
							if (bBinary)
							{
								auto OnInput = g_ActorFunctor / [](NProcess::EStdInReaderOutputType, CIOByteVector, CStr) -> TCFuture<void>
									{
										co_return {};
									}
								;
								Input = (co_await _pCommandLine->f_RegisterForStdInBinary(fg_Move(OnInput), NProcess::EStdInReaderFlag_None)).f_GetSubscription();
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

		TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) override
		{
			co_return {};
		}

		TCFuture<void> fp_StopApp() override
		{
			co_return {};
		}
	};

	class CDistributedApp_Tests : public NMib::NTest::CTest
	{
	public:
		void fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name, bool _bInProcessOnly = false)
		{
			CStr ConfigDir = NFile::CFile::fs_GetProgramDirectory() / _Name;
			fg_TestAddCleanupPath(ConfigDir);

			for (umint i = 0; i < 5; ++i)
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
			CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient, RunLoopHelper.m_pRunLoop).f_CallSync(RunLoopHelper.m_pRunLoop);

			_fTests(CommandLineClient);

			AppActor->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
		}

		void f_DoTests()
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
								, Output, Error, ExitCode
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
			};
		}
	};

	DMibTestRegister(CDistributedApp_Tests, Malterlib::Concurrency);
}
