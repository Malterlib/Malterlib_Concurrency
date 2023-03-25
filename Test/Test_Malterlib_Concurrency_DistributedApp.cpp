// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Test/Exception>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTestHelpers>

using namespace NMib;
using namespace NMib::NConcurrency;
using namespace NMib::NContainer;
using namespace NMib::NEncoding;
using namespace NMib::NStr;
using namespace NMib::NFunction;
using namespace NMib::NCommandLine;

namespace
{
	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp(CStr const &_Name)
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings(_Name)
				.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() / _Name)
				.f_SeparateDistributionManager(true)
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
						"Names"_= {"--test-actor"}
						, "Description"_= "Test 3."
						, "Options"_=
						{
							"Integer?"_=
							{
								"Names"_= {"--integer"}
								, "Default"_= 5
								, "Description"_= "Test 1" 
							}
						}
					}
					, [](NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
					{
						co_return _Params["Integer"].f_Integer();
					}
				)
			;
		}
		
		TCFuture<void> fp_StartApp(NEncoding::CEJSON const &_Params) override
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
		bool mp_bFirstRun = true;
	public:
		void fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests, CStr const &_Name)
		{
			if (mp_bFirstRun)
			{
				mp_bFirstRun = false;
				CStr ConfigDir = NFile::CFile::fs_GetProgramDirectory() / _Name;
				fg_TestAddCleanupPath(ConfigDir);

				for (mint i = 0; i < 5; ++i)
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
			}
			auto AppActor = fg_ConstructActor<CTestDistributedApp>(_Name);
			AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJSON{}, TCActor<>{}, EDistributedAppType_InProcess).f_CallSync();
			CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync();
			
			_fTests(CommandLineClient);

			AppActor->f_BlockDestroy();
		}
		
		void f_DoTests()
		{
			DMibTestCategory("Command line")
			{
				DMibTestSuite("Actor")
				{
					fp_RunTests
						(
							[](CDistributedAppCommandLineClient &_Client)
							{
								CEJSON RunParams;
								
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
