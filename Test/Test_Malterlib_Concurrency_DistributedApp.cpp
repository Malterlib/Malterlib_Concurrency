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

namespace
{
	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings
				(
					"TestDistApp"
					, false
					, NFile::CFile::fs_GetProgramDirectory() + "/TestDistApp"
					, true
					, NConcurrency::CDistributedActorTestKeySettings{}
				)
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
					, [](NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCContinuation<uint32>
					{
						return fg_Explicit(_Params["Integer"].f_Integer());
					}
				)
			;
		}
		
		TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) override 
		{
			return fg_Explicit();
		}

		TCContinuation<void> fp_StopApp() override 
		{
			return fg_Explicit();
		}
	};
	
	class CDistributedApp_Tests : public NMib::NTest::CTest
	{
		bool mp_bFirstRun = true;
	public:
		void fp_RunTests(TCFunction<void (CDistributedAppCommandLineClient &_Client)> &&_fTests)
		{
			if (mp_bFirstRun)
			{
				mp_bFirstRun = false;
				CStr ConfigDir = NFile::CFile::fs_GetProgramDirectory() + "/TestDistApp";
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
			auto AppActor = fg_ConstructActor<CTestDistributedApp>();
			AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJSON{}, TCActor<>{}, EDistributedAppType_InProcess).f_CallSync();
			CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync();
			
			_fTests(CommandLineClient);

			AppActor->f_BlockDestroy();
		}
		
		void f_DoTests()
		{
			DMibTestCategory("Command line")
			{
				DMibTestSuite("General")
				{
					fp_RunTests
						(
							[](CDistributedAppCommandLineClient &_Client)
							{
								CEJSON RunParams;
								
								_Client.f_MutateCommandLineSpecification
									(
										[&](CDistributedAppCommandLineSpecification &o_Specification, CDistributedAppActor_Settings const &_Settings)
										{
											auto Section = o_Specification.f_AddSection("Test1", "Testing test 2");
											
											[[maybe_unused]] auto Command = Section.f_RegisterDirectCommand
												(
													{
														"Names"_= {"--test", "-t"}
														, "Description"_= "Test 3."
														, "Parameters"_=
														{
															"ArgString?"_= 
															{
																"Default"_= ""
																, "Description"_= "Test 5"
															}
														}
														, "Options"_=
														{
															"Boolean?"_=
															{
																"Names"_= {"--boolean"}
																, "Default"_= false
																, "Description"_= "Test 1" 
															}
															, "Integer?"_=
															{
																"Names"_= {"--integer"}
																, "Default"_= 5
																, "Description"_= "Test 2" 
															}
															, "Float?"_=
															{
																"Names"_= {"--float"}
																, "Default"_= 5.5
																, "Description"_= "Test 3" 
															}
															, "String?"_=
															{
																"Names"_= {"--string"}
																, "Default"_= "TestStr"
																, "Description"_= "Test 4" 
															}
															, "Binary?"_=
															{
																"Names"_= {"--binary"}
																, "Default"_= fg_CreateVector<uint8>(5, 6, 7)
																, "Description"_= "Test 4" 
															}
															, "StringChoice?"_=
															{
																"Names"_= {"--string-choice"}
																, "Type"_= COneOf{"TestStr", "TestStr2"}
																, "Default"_= "TestStr"
																, "Description"_= "Test 4" 
															}
															, "Array?"_=
															{
																"Names"_= {"--array"}
																, "Type"_= _[_]
																, "Default"_= {"Test1", "Test2", {"Test"_= 5}}
																, "Description"_= "Test 5"
															}
															, "ArrayChoice?"_=
															{
																"Names"_= {"--array-choice"}
																, "Type"_= {COneOf{"", "Test1", "Test2"}}
																, "Default"_= {"Test1", "Test2"}
																, "Description"_= "Test 5"
															}
															, "ComplexObject?"_=
															{
																"Names"_= {"--complex-object"}
																, "Type"_= 
																{
																	"Key1?"_= ""
																	, "Key2?"_= {""}
																	, "Key3?"_= {COneOf{"Test1", "Test2"}}
																}
																, "Default"_= 
																{
																	"Key2"_= {"Test2"}
																}
																, "Description"_= "Test 4" 
															}
														}
													}
													, [&](CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
													{
														RunParams = _Params;
														return 66;
													}
												)
											;
										}
									)
								;
								DMibTestPath("General");
								aint Ret = _Client.f_RunCommandLine(fg_CreateVector<CStr>("App", "--test"));
								DMibExpect(Ret, ==, 66);
								DMibExpect
									(
										RunParams
										, ==
										, CEJSON
										(
											{
												"Boolean"_= false
												, "Integer"_= 5
												, "Float"_= 5.5
												, "String"_= "TestStr"
												, "Binary"_= fg_CreateVector<uint8>(5, 6, 7)
												, "StringChoice"_= "TestStr"
												, "Array"_= {"Test1", "Test2", {"Test"_= 5}}
												, "ArrayChoice"_= {"Test1", "Test2"}
												, "ComplexObject"_= {"Key2"_= {"Test2"}}
												, "ArgString"_= ""
												, "Command"_= "--test"
											}
										)
									)
								;
							}
						)
					;
				};
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
						)
					;
				};
			};
		}
		
		/* TODO
		Optional
			Parameters
			Options
		Defaults
			Type from defaults
			Type when no default
			Different types and defaults
		Global options
		Local options
		Type conversions
		Register command
			Direct
			Actor
			
		Validate Command Params
		Errors
			DMibError("An optional option needs to specify the 'Default' value");
			DMibError("An option needs to specify at least 'Default' or 'Type'");
			DMibError
				(
					"An option or command name cannot start with --no- as this is reserved for negating a boolean option."
					" If this is what you are trying to achieve, rather use then non-negative form and set the default to false."
				)
			;
			DMibError("An option or command name cannot contain '=' as this can be used to specify the value for an option.");
			Register options
				DMibError(fg_Format("Option with same identifier '{}' already exists in global options", Identifier));
				DMibError(fg_Format("Option with identifier '{}' already exists on command", Identifier));
				DMibError("You need to specify at least one name for option");
				DMibError(fg_Format("Name is already used as a command '{}'", Name));
				DMibError(fg_Format("Option with same name '{}' already exists in global options", Name));
				DMibError(fg_Format("Option with name '{}' already exists on command", Name));
			Register global options
				DMibError(fg_Format("A global option with identifier '{}' already exists", Identifier));
				DMibError(fg_Format("Option with same identifier '{}' already exists in command(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				DMibError("You need to specify at least one name for option");
				DMibError(fg_Format("Name is already used as a command '{}'", Name));
				DMibError(fg_Format("A global option with same name '{}' already exists", Name));
				DMibError(fg_Format("Option with name '{}' already exists in command(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
			Register command
				DMibError("You need to specify at least one name for command");
				DMibError(fg_Format("A global option with same name '{}' already exists", Name));
				DMibError(fg_Format("Option with name '{}' already exists in command(s) so cannot be used as a command name: {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
				DMibError(fg_Format("Name is already used for another command '{}'", Name));
				DMibError("Previous parameter was optional, but this one is not. This does not make sense.");
				DMibError(fg_Format("Duplicate parameter identifier '{}'", Identifier));
		 RunCommand
			DMibError(fg_Format("You cannot specify a command parameter with '=': {}", OptionValue));
			DMibError(fg_Format("Command '{}' already specified. You cannot specify additional command '{}'.", FoundCommandName, ParsedParameter));
			DMibError(fg_Format("You cannot negate a non-boolean option ({}) with --no-", _Value.m_Names.f_GetFirst()));
			DMibError(fg_Format("No such option '{}'", ParsedParameter));
			DMibError(fg_Format("Unexpected parameter: {}", Parameter));
			DMibError(fg_Format("Missing parameter for option: {}", pCurrentOption->m_Names.f_GetFirst()));
			DMibError(fg_Format("Missing required command parameter: {}", iCommandParameter->m_Identifier));
			DMibError("No command specified")		 
			DMibError(fg_Format("Missing required option: {}", _Option.m_Names.f_GetFirst()));
		
		*/ 
		
	};

	DMibTestRegister(CDistributedApp_Tests, Malterlib::Concurrency);
}
