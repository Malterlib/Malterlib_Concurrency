// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedDaemon.h"
#include <Mib/Daemon/Daemon>
#include <Mib/Encoding/EJSON>
#include <Mib/Encoding/JSONShortcuts>

namespace NMib
{
	namespace NConcurrency
	{
		using namespace NService;
		struct CDistributedDaemonDaemon : public CServiceImp
		{
			CDistributedDaemonDaemon(TCActor<CDistributedAppActor> const &_Actor, NEncoding::CEJSON const &_Params)
			{
				fg_ApplyLoggingOption(_Params);
				
				m_Actor = _Actor;
				m_Actor(&CDistributedAppActor::f_StartApp, _Params) > fg_ConcurrentActor() / [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
						{
							DMibLogWithCategory(Malterlib/Concurrency/DistributedDaemon, Error, "Failed to start application: {}", _Result.f_GetExceptionStr());
						}
					}
				;
			}
			~CDistributedDaemonDaemon()
			{
				if (m_Actor)
				{
					m_Actor(&CDistributedAppActor::f_StopApp).f_CallSync();
					m_Actor = nullptr;
				}
			}
		
			TCActor<CDistributedAppActor> m_Actor;
		};

		CDistributedDaemon::CDistributedDaemon
			(
				NStr::CStr const &_DaemonName
				, NStr::CStr const &_DaemonDisplayName
				, NStr::CStr const &_DaemonDescription 
				, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			)
			: mp_fActorFactory(_fActorFactory)
			, m_DaemonName(_DaemonName)
			, m_DaemonDisplayName(_DaemonDisplayName)
			, m_DaemonDescription(_DaemonDescription)
		{
		}
		
		CDistributedDaemon::~CDistributedDaemon()
		{
		}
		
		namespace
		{
			void fg_SetServiceOptions(CServiceParams &o_DaemonParams, NEncoding::CEJSON const &_Params)
			{
				if (_Params.f_GetMember("Daemon_Mode"))
				{
					NStr::CStr ModeStr = _Params["Daemon_Mode"].f_String();
					EServiceMode Mode = EServiceMode_Global;
					if (ModeStr == "global")
						Mode = EServiceMode_Global;  
					else if (ModeStr == "user")
						Mode = EServiceMode_LocalUser;  
					else if (ModeStr == "all-users")
						Mode = EServiceMode_AllUsers;
					else
						DMibNeverGetHere;
					o_DaemonParams.f_SetServiceMode(Mode);
				}
				if (_Params.f_GetMember("Daemon_RunAsUser"))
					o_DaemonParams.f_SetRunAsUser(_Params["Daemon_RunAsUser"].f_String());
				if (_Params.f_GetMember("Daemon_RunAsGroup"))
					o_DaemonParams.f_SetRunAsGroup(_Params["Daemon_RunAsGroup"].f_String());
				if (_Params.f_GetMember("Daemon_FailIfAdded"))
					o_DaemonParams.f_SetActionParam(_Params["Daemon_FailIfAdded"].f_Boolean());
				else if (_Params.f_GetMember("Daemon_Wait"))
					o_DaemonParams.f_SetActionParam(_Params["Daemon_Wait"].f_Boolean());
				if (_Params.f_GetMember("Daemon_Daemonize"))
					o_DaemonParams.f_SetKey("-Daemonize", _Params["Daemon_Daemonize"].f_Boolean());
				if (_Params.f_GetMember("Daemon_DoStart"))
					o_DaemonParams.f_SetKey("-DoStart", _Params["Daemon_DoStart"].f_Boolean());
				if (_Params.f_GetMember("Daemon_DoStop"))
					o_DaemonParams.f_SetKey("-DoStop", _Params["Daemon_DoStop"].f_Boolean());
				if (_Params.f_GetMember("Daemon_DoStatus"))
					o_DaemonParams.f_SetKey("-DoStatus", _Params["Daemon_DoStatus"].f_Boolean());
				o_DaemonParams.f_SetDisableWriteService(true); // Handled locally
				o_DaemonParams.f_SetExecutablePath(NFile::CFile::fs_GetProgramPath());
			}
			
			aint fg_RunDaemon
				(
					CDistributedDaemon const &_DistributedDaemon
					, NEncoding::CEJSON const &_Params
					, CDistributedAppActor_Settings const &_Settings
					, bool _bSaveSettings
					, EServiceAction _Action
				)
			{
				CServiceParams DaemonParams
					{
						_DistributedDaemon.m_DaemonName
						, _DistributedDaemon.m_DaemonDisplayName
						, _DistributedDaemon.m_DaemonDescription
						, nullptr
						, [&]() -> NPtr::TCUniquePointer<NService::CServiceImp>
						{
							return fg_Construct<CDistributedDaemonDaemon>(_DistributedDaemon.m_AppActor, _Params);
						}
						, nullptr
						, [] (NStr::CStr const& _Errors)
						{
							DMibConErrOut("{}{\n}", _Errors);
						}
						, [] (NStr::CStr const& _Heading, NStr::CStr const& _Information)
						{
							DMibConErrOut("{}{\n}", _Information);
						}
						, [&] (NStr::CStr const& _Errors, NService::EReportError _Default) -> NMib::NService::EReportError
						{
							DMibConErrOut("{}{\n}", _Errors);
							return _Default;
						}
					}
				;
				DaemonParams.f_SetAction(_Action);
				NStr::CStr DaemonName = _Params["Daemon_Name"].f_String();
				DaemonParams.f_SetServiceName(DaemonName, DaemonName != _DistributedDaemon.m_DaemonName);
				fg_SetServiceOptions(DaemonParams, _Params);
				CService Daemon(DaemonParams);
				aint Result = Daemon.f_ProcessCommand();
								
				if (_bSaveSettings && Result == 0 && _Params["Daemon_SaveSettings"].f_Boolean())
				{
					NStr::CStr SettingsFile = fg_Format("{}/{}DaemonSettings.json", _Settings.m_ConfigDirectory, _Settings.m_AppName);
					NEncoding::CEJSON DaemonSettings;
					if (NFile::CFile::fs_FileExists(SettingsFile))
						DaemonSettings = NEncoding::CEJSON::fs_FromString(NFile::CFile::fs_ReadStringFromFile(SettingsFile), SettingsFile);
					
					DaemonSettings["Name"] = _Params["Daemon_Name"];
					DaemonSettings["Mode"] = _Params["Daemon_Mode"];
					
					NFile::CFile::fs_WriteStringToFile(SettingsFile, DaemonSettings.f_ToString());
				}
				return Result;
			}
		}

		void CDistributedDaemon::fp_AddDaemonCommands(CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
		{
			mp_Settings = _Settings;
			auto Section = o_CommandLine.f_AddSection("Daemon", "Commands for managing running the application as a system or user daemon.");
			
			NStr::CStr SettingsFile = fg_Format("{}/{}DaemonSettings.json", _Settings.m_ConfigDirectory, _Settings.m_AppName);
			NEncoding::CEJSON DaemonSettings;
			if (NFile::CFile::fs_FileExists(SettingsFile))
				DaemonSettings = NEncoding::CEJSON::fs_FromString(NFile::CFile::fs_ReadStringFromFile(SettingsFile), SettingsFile);
			
			if (!DaemonSettings.f_GetMember("Name", NEncoding::EJSONType_String))
				DaemonSettings["Name"] = m_DaemonName;
			if (!DaemonSettings.f_GetMember("Mode", NEncoding::EJSONType_String))
				DaemonSettings["Mode"] = "global";				
			
			Section.f_RegisterSectionOptions
				(
					{
						"Daemon_Mode?"_= 
						{
							"Names"_= {"--daemon-mode"}
							, "Type"_= COneOf{"global", "user", "all-users"}
							, "Default"_= DaemonSettings["Mode"].f_String()
							, "Description"_= "Specify the mode of the daemon\n"
							"@Indent=15\r"
							"global:      Install the daemon as a system daemon. This daemon will start when the compure starts.\r"
							"user:        Install the daemon as a user daemon. This daemon will start when the current user logs in.\r"
							"all-users:   Install the daemon as a user daemon for all users. This daemon will start when a user logs in.\r"
							, "DefaultEnabled"_= false
						}
					}
				)
			;
			auto DaemonNameParam = "Daemon_Name?"_= 
				{
					"Default"_= DaemonSettings["Name"].f_String()
					, "Description"_= fg_Format
					(
						"The unique name of the daemon.\n"
						"If you want to override the default name of the daemon you can specify the name here.\n" 
						"The reason could for example be to be able to install the same daemon in different directories on the same computer.\n"
						"If you have previously added the daemon the default for the name is automatically read from the '{}' file."
						, NFile::CFile::fs_GetFile(SettingsFile)
					)
				}
			;
			auto DaemonWaitOption = "Daemon_Wait?"_= 
				{
					"Names"_= {"--daemon-wait"}
					, "Default"_= true
					, "Description"_= "Wait for service to stop"
				}
			;
			
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-add"}
						, "Description"_= fg_Format
						(
							"Add a daemon for the application.\n"
							"If the add was successful, the name of the daemon will be saved in '{}' unless you disable it with --no-daemon-save-settings.\n"
							, NFile::CFile::fs_GetFile(SettingsFile)
						)
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
						, "Options"_= 
						{
							"Daemon_SaveSettings?"_= 
							{
								"Names"_= {"--daemon-save-settings"}
								, "Default"_= true
								, "Description"_= "Enable or disable the saving of the mode and daemon name to file."
							}
							, "Daemon_RunAsUser?"_= 
							{
								"Names"_= {"--daemon-run-as-user", "-RunAsUser"}
								, "Default"_= ""
								, "Description"_= "Specify the user that the daemon should run as when started."
							}
							, "Daemon_RunAsGroup?"_= 
							{
								"Names"_= {"--daemon-run-as-group", "-RunAsGroup"}
								, "Default"_= ""
								, "Description"_= "Specify the group that the daemon should run as when started."
							}
							, "Daemon_FailIfAdded?"_= 
							{
								"Names"_= {"--daemon-fail-if-added"}
								, "Default"_= true
								, "Description"_= "Fail if daemon is already installed."
							}
						}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, true, EServiceAction_Add);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-remove"}
						, "Description"_= "Remove the daemon for the application.\n"
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Remove);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-start"}
						, "Description"_= "Start the daemon.\n"
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Start);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-restart"}
						, "Description"_= "Restart the daemon.\n"
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Options"_=
						{
							DaemonWaitOption
						}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Restart);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-stop"}
						, "Description"_= "Stop the daemon.\n"
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Options"_=
						{
							DaemonWaitOption
						}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Stop);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-run-debug"}
						, "Description"_= 
							"Run the daemon as a program.\n"
							"Mostly for debugging purposes. On supported operating systems a tray icon will be added that you can use to send commands to the daemon.\n"
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_RunAsProgram);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-exists"}
						, "Description"_= "Check if the daemon exists and has been installed.\n"
						, "Status"_= 
						{
							"0"_= "The service exists"
							, "1"_= "The service does not exist"
						}
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Exists);
					}
				)
			;
			Section.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--daemon-run", "-Service"}
						, "Description"_= "Runs the daemon. [INTERNAL]\n"
						, "Status"_= 
						{
							"0"_= "The service exists"
							, "1"_= "The service does not exist"
						}
						, "SectionOptions"_= {"Daemon_Mode"}
						, "Parameters"_= {DaemonNameParam}
						, "Options"_= 
						{
							"Daemon_Daemonize?"_= 
							{
								"Names"_= {"--daemon-daemonize", "-Daemonize"}
								, "Type"_= true
								, "Description"_= "Daemonizes the daemon on some platforms. [INTERNAL]"
							}
							, "Daemon_DoStart?"_= 
							{
								"Names"_= {"--daemon-do-start", "-DoStart"}
								, "Type"_= true
								, "Description"_= "Starts the daemon on some platform. [INTERNAL]"
							}
							, "Daemon_DoStop?"_= 
							{
								"Names"_= {"--daemon-do-stop", "-DoStop"}
								, "Type"_= true
								, "Description"_= "Stops the daemon on some platform. [INTERNAL]"
							}
							, "Daemon_DoStatus?"_= 
							{
								"Names"_= {"--daemon-do-status", "-DoStatus"}
								, "Type"_= true
								, "Description"_= "Checks daemon status on some platform. [INTERNAL]"
							}
						}
					}
					, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
					{
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EServiceAction_Run);
					}
				)
			;
		}

		aint CDistributedDaemon::f_Run()
		{
			return fg_RunApp
				(
					[this]
					{
						m_AppActor = mp_fActorFactory();
						return m_AppActor;
					}
					, [this](CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
					{
						fp_AddDaemonCommands(o_CommandLine, _Settings);
					}
					, false
				)
			;
		}

	}
}
