// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedDaemon.h"
#include <Mib/Daemon/Daemon>
#include <Mib/Encoding/EJson>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/File/ExeFS>

#ifdef DPlatformFamily_Windows
namespace NMib::NPlatform
{
	void *fg_GetWindowsDllInstance();
}
#endif

namespace NMib::NConcurrency
{
	using namespace NDaemon;
	struct CDistributedDaemonDaemon : public CDaemonImp
	{
		CDistributedDaemonDaemon(TCActor<CDistributedAppActor> const &_Actor, NEncoding::CEJsonSorted const &_Params, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop)
			: m_pRunLoop(_pRunLoop)
			, m_Actor(_Actor)
		{
			m_ApplyLoggingResults = fg_ApplyLoggingOption(_Params, m_Actor);

			m_Actor(&CDistributedAppActor::f_StartApp, _Params, m_ApplyLoggingResults.m_LogActor, EDistributedAppType_Daemon)
				.f_OnResultSet
				(
					[](TCAsyncResult<NStr::CStr> &&_Result)
					{
						if (_Result)
							return;

						DMibLogWithCategory(Malterlib/Concurrency/DistributedDaemon, Error, "Failed to start application: {}", _Result.f_GetExceptionStr());

						NStr::CStr Ticket = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerRequestTicket");
						if (Ticket.f_IsEmpty())
							return;

						NEncoding::CEJsonSorted Json;
						Json["Error"] = _Result.f_GetExceptionStr();
						DMibConErrOut2("{}:Error:{}\n", Ticket, Json.f_ToString(nullptr));
					}
				)
			;
		}
		~CDistributedDaemonDaemon()
		{
			if (m_Actor)
			{
				try
				{
					m_Actor(&CDistributedAppActor::f_StopApp).f_CallSync(m_pRunLoop);
					fg_Move(m_Actor).f_Destroy().f_CallSync(m_pRunLoop);;
				}
				catch (NException::CException const &_Exception)
				{
					(void)_Exception;
					DMibLogWithCategory(Malterlib/Concurrency/DistributedDaemon, Error, "Exception stopping app: {}", _Exception);
				}

				m_Actor = nullptr;
			}

			try
			{
				m_ApplyLoggingResults.f_Destroy(m_pRunLoop->f_ActorDestroyLoop());
			}
			catch (NException::CException const &_Exception)
			{
				DMibConErrOut("Error destroying logging: {}{\n}", _Exception.f_GetErrorStr());
			}

			m_ApplyLoggingResults = {};
		}

		TCActor<CDistributedAppActor> m_Actor;
		CApplyLoggingResults m_ApplyLoggingResults;
		NStorage::TCSharedPointer<CRunLoop> m_pRunLoop;
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
		void fg_SetDaemonOptions(CDaemonParams &o_DaemonParams, NEncoding::CEJsonSorted const &_Params)
		{
			EDaemonMode Mode = EDaemonMode_Global;
			if (_Params.f_GetMember("Daemon_Mode"))
			{
				NStr::CStr ModeStr = _Params["Daemon_Mode"].f_String();
				if (ModeStr == "global")
					Mode = EDaemonMode_Global;
				else if (ModeStr == "user")
					Mode = EDaemonMode_LocalUser;
				else if (ModeStr == "all-users")
					Mode = EDaemonMode_AllUsers;
				else
					DMibNeverGetHere;
				o_DaemonParams.f_SetDaemonMode(Mode);
			}
			if (Mode == EDaemonMode_Global)
			{
				if (_Params.f_GetMember("Daemon_RunAsUser"))
					o_DaemonParams.f_SetRunAsUser(_Params["Daemon_RunAsUser"].f_String());
				if (_Params.f_GetMember("Daemon_RunAsGroup"))
					o_DaemonParams.f_SetRunAsGroup(_Params["Daemon_RunAsGroup"].f_String());
			}
			if (_Params.f_GetMember("Daemon_MaxShutdownTime"))
				o_DaemonParams.f_SetMaxShutdownTime(_Params["Daemon_MaxShutdownTime"].f_Float());
			if (_Params.f_GetMember("Daemon_FailIfAdded"))
				o_DaemonParams.f_SetActionParam(!_Params["Daemon_FailIfAdded"].f_Boolean());
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
			if (_Params.f_GetMember("Daemon_DetachConsole"))
				o_DaemonParams.f_SetDetachConsole(_Params["Daemon_DetachConsole"].f_Boolean());
			o_DaemonParams.f_SetDisableWriteDaemon(true); // Handled locally
			o_DaemonParams.f_SetExecutablePath(NFile::CFile::fs_GetProgramPath());
		}

		aint fg_RunDaemon
			(
				CDistributedDaemon const &_DistributedDaemon
				, NEncoding::CEJsonSorted const &_Params
				, CDistributedAppActor_Settings const &_Settings
				, bool _bSaveSettings
				, EDaemonAction _Action
			)
		{
			void *pIconData = nullptr;
#		ifdef DPlatformFamily_Windows
			pIconData = NPlatform::fg_GetWindowsDllInstance();
#		elif defined(DPlatformFamily_macOS)
			NContainer::CByteVector IconData;
			if (_DistributedDaemon.m_Icon && NFile::fg_ReadExeFSFile(_DistributedDaemon.m_Icon, IconData))
				pIconData = &IconData;
#		endif

			CDaemonParams DaemonParams
				{
					_DistributedDaemon.m_DaemonName
					, _DistributedDaemon.m_DaemonDisplayName
					, _DistributedDaemon.m_DaemonDescription
					, pIconData
					, [&]() -> NStorage::TCUniquePointer<NDaemon::CDaemonImp>
					{
						return fg_Construct<CDistributedDaemonDaemon>(_DistributedDaemon.m_AppActor, _Params, _DistributedDaemon.m_pRunLoop);
					}
					, nullptr
					, [] (NStr::CStr const& _Errors)
					{
						DMibConErrOut("{}{\n}", _Errors);
					}
					, [bVerbose = _Params["Daemon_Verbose"].f_Boolean()] (NStr::CStr const& _Heading, NStr::CStr const& _Information)
					{
						if (bVerbose)
							DMibConErrOut("{}{\n}", _Information);
					}
					, [&] (NStr::CStr const& _Errors, NDaemon::EReportError _Default) -> NMib::NDaemon::EReportError
					{
						DMibConErrOut("{}{\n}", _Errors);
						return _Default;
					}
				}
			;
			DaemonParams.f_SetAlwaysRunStatusApp(_DistributedDaemon.m_bAlwaysRunStatusApp);
			DaemonParams.f_SetCanPause(false);
			DaemonParams.f_SetAction(_Action);
			DaemonParams.f_SetExecutionPriority(_DistributedDaemon.m_ExecutionPriority);
			NStr::CStr DaemonName = _Params["Daemon_Name"].f_String();
			DaemonParams.f_SetDaemonName(DaemonName, DaemonName != _DistributedDaemon.m_DaemonName);
			fg_SetDaemonOptions(DaemonParams, _Params);
			CDaemon Daemon(DaemonParams);
			aint Result = Daemon.f_ProcessCommand();

			if (_bSaveSettings && Result == 0 && _Params["Daemon_SaveSettings"].f_Boolean())
			{
				NStr::CStr SettingsFile = fg_Format("{}/{}DaemonSettings.json", _Settings.m_RootDirectory, _Settings.m_AppName);
				NEncoding::CEJsonSorted DaemonSettings;
				if (NFile::CFile::fs_FileExists(SettingsFile))
					DaemonSettings = NEncoding::CEJsonSorted::fs_FromString(NFile::CFile::fs_ReadStringFromFile(SettingsFile), SettingsFile);

				if (DaemonParams.f_GetDaemonMode() != EDaemonMode_AllUsers)
				{
					DaemonSettings["Name"] = _Params["Daemon_Name"];
					DaemonSettings["Mode"] = _Params["Daemon_Mode"];

					NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(SettingsFile));
					NFile::CFile::fs_WriteStringToFile(SettingsFile, DaemonSettings.f_ToString());
				}
			}
			return Result;
		}
	}

	void CDistributedDaemon::fp_AddDaemonCommands(CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
	{
		mp_Settings = _Settings;
		auto Section = o_CommandLine.f_AddSection("Daemon", "Commands for managing running the application as a system or user daemon.", "Distributed Computing");

		NStr::CStr SettingsFile = fg_Format("{}/{}DaemonSettings.json", _Settings.m_RootDirectory, _Settings.m_AppName);
		NEncoding::CEJsonSorted DaemonSettings;
		if (NFile::CFile::fs_FileExists(SettingsFile))
			DaemonSettings = NEncoding::CEJsonSorted::fs_FromString(NFile::CFile::fs_ReadStringFromFile(SettingsFile), SettingsFile);

		if (!DaemonSettings.f_GetMember("Name", NEncoding::EJsonType_String))
			DaemonSettings["Name"] = m_DaemonName;
		if (!DaemonSettings.f_GetMember("Mode", NEncoding::EJsonType_String))
		{
			switch (m_DefaultMode)
			{
			case EDaemonMode_Global:
				DaemonSettings["Mode"] = "global";
				break;
			case EDaemonMode_AllUsers:
				DaemonSettings["Mode"] = "all-users";
				break;
			case EDaemonMode_LocalUser:
				DaemonSettings["Mode"] = "user";
				break;
			}
		}

		Section.f_RegisterSectionOptions
			(
				{
					"Daemon_Mode?"_o=
					{
						"Names"_o= _o["--mode"]
						, "Type"_o= NCommandLine::COneOf{"global", "user", "all-users"}
						, "Default"_o= DaemonSettings["Mode"].f_String()
						, "Description"_o= "Specify the mode of the daemon.\n"
						"@Indent=16\r"
						"   global:      Install the daemon as a system daemon. This daemon will start when the computer starts.\r"
						"   user:        Install the daemon as a user daemon. This daemon will start when the current user logs in.\r"
						"   all-users:   Install the daemon as a user daemon for all users. This daemon will start when a user logs in.\r"
						, "DefaultEnabled"_o= false
					}
					, "Daemon_Verbose?"_o=
					{
						"Names"_o= _o["--verbose"]
						, "Default"_o= false
						, "Description"_o= "Output informational messages to command line.\n"
					}
				}
			)
		;
		auto DaemonNameParam = "Daemon_Name?"_o=
			{
				"Default"_o= DaemonSettings["Name"].f_String()
				, "Description"_o= fg_Format
				(
					"The unique name of the daemon.\n"
					"If you want to override the default name of the daemon you can specify the name here.\n"
					"The reason could for example be to be able to install the same daemon in different directories on the same computer.\n"
					"If you have previously added the daemon the default for the name is automatically read from the '{}' file."
					, NFile::CFile::fs_GetFile(SettingsFile)
				)
			}
		;
		auto DaemonWaitOption = "Daemon_Wait?"_o=
			{
				"Names"_o= _o["--wait"]
				, "Default"_o= true
				, "Description"_o= "Wait for daemon to stop."
			}
		;

		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-add"]
					, "Description"_o= fg_Format
					(
						"Add a daemon for the application.\n"
						"If the add was successful, the name of the daemon will be saved in '{}' unless you disable it with --no-daemon-save-settings.\n"
						, NFile::CFile::fs_GetFile(SettingsFile)
					)
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
					, "Options"_o=
					{
						"Daemon_SaveSettings?"_o=
						{
							"Names"_o= _o["--save-settings"]
							, "Default"_o= true
							, "Description"_o= "Enable or disable the saving of the mode and daemon name to file."
						}
						, "Daemon_RunAsUser?"_o=
						{
							"Names"_o= _o["--run-as-user", "-RunAsUser"]
							, "Default"_o= _Settings.m_RunAsUser
							, "Description"_o= "Specify the user that the daemon should run as when started."
						}
						, "Daemon_RunAsGroup?"_o=
						{
							"Names"_o= _o["--run-as-group", "-RunAsGroup"]
							, "Default"_o= _Settings.m_RunAsGroup
							, "Description"_o= "Specify the group that the daemon should run as when started."
						}
						, "Daemon_MaxShutdownTime?"_o=
						{
							"Names"_o= _o["--max-shutdown-time"]
							, "Default"_o= _Settings.m_MaxShutdownTime
							, "Description"_o= "Specify the max shutdown time after which the daemon is killed when stopping daemon."
						}
						, "Daemon_FailIfAdded?"_o=
						{
							"Names"_o= _o["--fail-if-added"]
							, "Default"_o= true
							, "Description"_o= "Fail if daemon is already installed."
						}
						, "Daemon_Start?"_o=
						{
							"Names"_o= _o["--start"]
							, "Default"_o= true
							, "Description"_o= "Start the daemon after it's added."
						}
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					uint32 Status = fg_RunDaemon(*this, _Params, mp_Settings, true, EDaemonAction_Add);
					if (Status)
						return Status;

					if (_Params["Daemon_Start"].f_Boolean())
						return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Start);
					else
						return Status;
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-remove"]
					, "Description"_o= "Remove the daemon for the application.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Remove);
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-start"]
					, "Description"_o= "Start the daemon.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Start);
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-restart"]
					, "Description"_o= "Restart the daemon.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Options"_o=
					{
						DaemonWaitOption
					}
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Restart);
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-stop"]
					, "Description"_o= "Stop the daemon.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Options"_o=
					{
						DaemonWaitOption
					}
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Stop);
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-run-debug"]
					, "Description"_o=
						"Run the daemon as a program.\n"
						"Mostly for debugging purposes. On supported operating systems a tray icon will be added that you can use to send commands to the daemon.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
					, "Options"_o=
					{
#ifdef DPlatformFamily_Windows
						"Daemon_DetachConsole?"_o=
						{
							"Names"_o= _o["--detach-console"]
							, "Default"_o= false
							, "Description"_o= "Detach from console so no window is displayed."
						}
#endif
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_RunAsProgram);
				}
				, EDistributedAppCommandFlag_DontApplyLogging
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-run-standalone"]
					, "Description"_o=
						"Run the daemon as a program without debugging helpers.\n"
						"Use this when you want to run the daemon manually without attempting to use system daemon facilities and without daemonizing.\n"
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_RunAsProgramNoDebug);
				}
				, EDistributedAppCommandFlag_DontApplyLogging
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-exists"]
					, "Description"_o= "Check if the daemon exists and has been installed.\n"
					, "Status"_o=
					{
						"0"_o= "The daemon exists."
						, "1"_o= "The daemon does not exist."
					}
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Exists);
				}
			)
		;
		Section.f_RegisterDirectCommand
			(
				{
					"Names"_o= _o["--daemon-run", "-Service"]
					, "Description"_o= "Runs the daemon. [INTERNAL]\n"
					, "Status"_o=
					{
						"0"_o= "The daemon exists."
						, "1"_o= "The daemon does not exist."
					}
					, "SectionOptions"_o= _o["Daemon_Mode", "Daemon_Verbose"]
					, "Parameters"_o=
					{
						DaemonNameParam
					}
					, "Options"_o=
					{
						"Daemon_Daemonize?"_o=
						{
							"Names"_o= _o["--daemonize", "-Daemonize"]
							, "Type"_o= true
							, "Description"_o= "Daemonizes the daemon on some platforms. [INTERNAL]"
						}
						, "Daemon_DoStart?"_o=
						{
							"Names"_o= _o["--do-start", "-DoStart"]
							, "Type"_o= true
							, "Description"_o= "Starts the daemon on some platform. [INTERNAL]"
						}
						, "Daemon_DoStop?"_o=
						{
							"Names"_o= _o["--do-stop", "-DoStop"]
							, "Type"_o= true
							, "Description"_o= "Stops the daemon on some platform. [INTERNAL]"
						}
						, "Daemon_DoStatus?"_o=
						{
							"Names"_o= _o["--do-status", "-DoStatus"]
							, "Type"_o= true
							, "Description"_o= "Checks daemon status on some platform. [INTERNAL]"
						}
					}
				}
				, [this](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					return fg_RunDaemon(*this, _Params, mp_Settings, false, EDaemonAction_Run);
				}
				, EDistributedAppCommandFlag_DontApplyLogging
			)
		;
	}

	aint CDistributedDaemon::f_Run()
	{
		if (!m_pRunLoop)
			m_pRunLoop = fg_Construct<CDefaultRunLoop>();

		return fg_RunApp
			(
				[this]
				{
					auto &SuggestedEnclave = fg_DistributedAppThreadLocal().m_DefaultSettings.m_Enclave;
					auto OldEnclave = SuggestedEnclave;
					SuggestedEnclave = NStr::CStr{};
					auto Cleanup = g_OnScopeExit / [&]
						{
							SuggestedEnclave = OldEnclave;
						}
					;
					m_AppActor = mp_fActorFactory();
					return m_AppActor;
				}
				, [this](CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
				{
					fp_AddDaemonCommands(o_CommandLine, _Settings);
				}
				, false
				, m_pRunLoop
			)
		;
	}
}
