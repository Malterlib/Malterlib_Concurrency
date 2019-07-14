// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Process/Platform>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Network/Socket>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_Private.h"
#include <Mib/Process/Platform>

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NNetwork;
	using namespace NCryptography;

	namespace NPrivate
	{
		CSubSystem_Concurrency_DistributedApp::CSubSystem_Concurrency_DistributedApp()
		{
			fg_DistributedActorSubSystem();
		}

		CSubSystem_Concurrency_DistributedApp::~CSubSystem_Concurrency_DistributedApp()
		{
		}

		TCSubSystem<CSubSystem_Concurrency_DistributedApp, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_DistributedApp = {DAggregateInit};

		CSubSystem_Concurrency_DistributedApp &fg_DistributedAppSubSystem()
		{
			return *g_MalterlibSubSystem_Concurrency_DistributedApp;
		}
	}

	CDistributedAppThreadLocal &fg_DistributedAppThreadLocal()
	{
		return *(*NPrivate::g_MalterlibSubSystem_Concurrency_DistributedApp).m_ThreadLocal;
	}

	CDistributedAppState::~CDistributedAppState() = default;

	CDistributedAppState::CDistributedAppState(CDistributedAppActor_Settings const &_Settings)
		: m_StateDatabase(fg_Format("{}/{}State.json", _Settings.m_RootDirectory, _Settings.m_AppName))
		, m_ConfigDatabase(fg_Format("{}/{}Config.json", _Settings.m_RootDirectory, _Settings.m_AppName))
		, m_RootDirectory(_Settings.m_RootDirectory)
	{
	}

	CDistributedAppState::CDistributedAppState(CDistributedAppState const &) = default;
	CDistributedAppState::CDistributedAppState(CDistributedAppState &&) = default;

	CDistributedAppState &CDistributedAppState::operator = (CDistributedAppState const &) = default;
	CDistributedAppState &CDistributedAppState::operator = (CDistributedAppState &&) = default;

	CDistributedAppActor::CDistributedAppActor(CDistributedAppActor_Settings const &_Settings)
		: mp_State(_Settings, *this)
		, mp_Settings(_Settings)
		, mp_AppType(fg_DistributedAppThreadLocal().m_DefaultAppType)
	{
		mp_State.m_LocalAddress = fp_GetLocalAddress();

		if (mp_AppType != EDistributedAppType_InProcess)
		{
			fg_GetSys()->f_SetDefaultLogFileName(fg_Format("{}.log", _Settings.m_AppName));
			mp_CurrentLogDirectory = _Settings.m_RootDirectory + "/Log";
			fg_GetSys()->f_SetDefaultLogFileDirectory(mp_CurrentLogDirectory);
		}

		fp_MakeActive();
	}

	CDistributedAppActor::~CDistributedAppActor()
	{
	}

	void CDistributedAppActor::f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo)
	{
		CStr Category = _Category;
		if (Category.f_IsEmpty())
			Category = mp_Settings.m_AuditCategory;
#if (DMibSysLogSeverities) != 0
		NMib::NLog::CSysLogCatScope Scope(NMib::fg_GetSys()->f_GetLogger(), Category);
		CStr UserID = _CallingHostInfo.f_GetClaimedUserID();
		CStr UserName = _CallingHostInfo.f_GetClaimedUserName();
		if (UserName || UserID)
			NMib::NLog::fg_SysLog(DLogLocTag, _Severity, "<{}, {} [{}]> {}", _CallingHostInfo.f_GetHostInfo(), UserID, UserName, _Message);
		else
			NMib::NLog::fg_SysLog(DLogLocTag, _Severity, "<{}> {}", _CallingHostInfo.f_GetHostInfo(), _Message);
#endif
	}

	CCallingHostInfoScope CDistributedAppActor::fp_PopulateCurrentHostInfoIfMissing(CStr const &_Description)
	{
		auto CurrentCallingHostInfo = fg_GetCallingHostInfo();
		CStr FriendlyName;

		if (CurrentCallingHostInfo.f_GetRealHostID().f_IsEmpty())
		{
			CStr LocalName = fg_Format("{}@{}/{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName(), mp_Settings.m_AppName);
			if (_Description.f_IsEmpty())
				FriendlyName = fg_Format("{} (Local)", LocalName);
			else
				FriendlyName = fg_Format("{} (Local {})", LocalName, _Description);
		}
		else
		{
			if (!_Description.f_IsEmpty())
				FriendlyName = fg_Format("{} ({})", CurrentCallingHostInfo.f_GetHostInfo().m_FriendlyName, _Description);
			else
				FriendlyName = CurrentCallingHostInfo.f_GetHostInfo().m_FriendlyName;
		}

		return CCallingHostInfoScope
			{
				CCallingHostInfo
				{
					CurrentCallingHostInfo.f_GetDistributionManager() ? CurrentCallingHostInfo.f_GetDistributionManager() : mp_State.m_DistributionManager
					, CurrentCallingHostInfo.f_GetAuthenticationHandler()
					, CurrentCallingHostInfo.f_GetUniqueHostID()
					, CurrentCallingHostInfo.f_GetRealHostID().f_IsEmpty()
					? CHostInfo(mp_State.m_HostID, FriendlyName)
					: CHostInfo(CurrentCallingHostInfo.f_GetHostInfo().m_HostID, FriendlyName)
					, CurrentCallingHostInfo.f_LastExecutionID()
					, CurrentCallingHostInfo.f_GetProtocolVersion()
					, CurrentCallingHostInfo.f_GetClaimedUserID()
					, CurrentCallingHostInfo.f_GetClaimedUserName()
					, CurrentCallingHostInfo.f_GetHost()
				}
			}
		;
	}

	NStr::CStr CDistributedAppActor::fp_GetLocalHostname(bool _bEnclaveSpecific) const
	{
		return mp_Settings.f_GetLocalSocketHostname(_bEnclaveSpecific);
	}

	NWeb::NHTTP::CURL CDistributedAppActor::fp_GetLocalAddress() const
	{
		return NWeb::NHTTP::CURL{fg_Format("wss://[{}]/", fp_GetLocalHostname(false))};
	}

	NContainer::TCMap<NStr::CStr, NStr::CStr> CDistributedAppActor::fp_GetTranslateHostnames() const
	{
		NContainer::TCMap<NStr::CStr, NStr::CStr> TranslateHostnames;
		if (!mp_Settings.m_Enclave.f_IsEmpty())
			TranslateHostnames[fp_GetLocalHostname(false)] = fp_GetLocalHostname(true);
		return TranslateHostnames;
	}

	TCFuture<void> CDistributedAppActor::fp_SetupListen()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Setting up listen config");
		TCPromise<void> Promise;

		TCSet<CDistributedActorTrustManager_Address> WantedListens;

		CDistributedActorTrustManager_Address LocalListen;
		LocalListen.m_URL = fp_GetLocalAddress();

		auto const *pListen = mp_State.m_ConfigDatabase.m_Data.f_GetMember("Listen", EJSONType_Array);
		if (pListen)
		{
			bool bFirst = true;
			for (auto &Object : pListen->f_Array())
			{
				if (!Object.f_IsObject())
					return DMibErrorInstance("Invalid listen entry. Entry needs to be an object with 'Address' specified");

				CDistributedActorTrustManager_Address Address;
				if (auto pAddress = Object.f_GetMember("Address"))
				{
					if (pAddress->f_Type() != EJSONType_String)
						return DMibErrorInstance("Listen 'Address' has wrong type, only string is supported");
					if (!Address.m_URL.f_Decode(pAddress->f_String()))
						return DMibErrorInstance(fg_Format("Invalid listen URL '{}'", pAddress->f_String()));
					if (Address.m_URL.f_GetScheme() != "wss")
						return DMibErrorInstance(fg_Format("Invalid scheme in URL '{}': Only wss is currently supported", pAddress->f_String()));
				}
				else
					return DMibErrorInstance("Missing 'Address' for listen entry");

				if (bFirst)
					mp_PrimaryListen = Address;
				bFirst = false;

				if (Address == LocalListen)
					return DMibErrorInstance(fg_Format("'{}' cannot be used as listen address, it is reserved for local command line communication", fp_GetLocalAddress().f_Encode()));
				WantedListens[Address];
			}
		}
		WantedListens[LocalListen];

		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens)
			> Promise % "Failed to enum current listen" / [this, Promise, WantedListens](TCSet<CDistributedActorTrustManager_Address> &&_Listens)
			{
				TCActorResultVector<void> ChangesResults;
				bool bChanged = false;
				for (auto const &CurrentListen : _Listens)
				{
					if (WantedListens.f_FindEqual(CurrentListen))
						continue;
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, CurrentListen) > ChangesResults.f_AddResult();
					DMibLogWithCategory(Mib/Concurrency/App, Debug, "Removing listen config {}", CurrentListen.m_URL.f_Encode());
					bChanged = true;
				}
				for (auto const &WantedListen : WantedListens)
				{
					if (_Listens.f_FindEqual(WantedListen))
						continue;
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, WantedListen) > ChangesResults.f_AddResult();
					DMibLogWithCategory(Mib/Concurrency/App, Debug, "Adding listen config {}", WantedListen.m_URL.f_Encode());
					bChanged = true;
				}
				if (!bChanged)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Debug, "No listen config changes needed");
					Promise.f_SetResult();
					return;
				}
				ChangesResults.f_GetResults() > [Promise](TCAsyncResult<TCVector<TCAsyncResult<void>>> &&_Results)
					{
						if (!fg_CombineResults(Promise, fg_Move(_Results)))
							return;

						DMibLogWithCategory(Mib/Concurrency/App, Debug, "Finished changing listen config");
						Promise.f_SetResult();
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	void CDistributedAppActor::fp_CleanupEnclaveSockets()
	{
		if (mp_Settings.m_Enclave.f_IsEmpty())
			return;

		if (!mp_CleanupFilesActor)
			mp_CleanupFilesActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Cleanup files"));

		g_Dispatch(mp_CleanupFilesActor) / [WildcardPath = mp_Settings.f_GetLocalSocketWildcard(true)]
			{
				try
				{
					for (auto &File : CFile::fs_FindFiles(WildcardPath, EFileAttrib_File))
					{
						try
						{
							try
							{
								NNetwork::CNetAddress Address = NNetwork::CSocket::fs_ResolveAddress(fg_Format("UNIX:{}", File));
								NNetwork::CSocket Socket;
								Socket.f_Connect(Address, nullptr, 5.0);
							}
							catch (NException::CException const &_Exception)
							{
								(void)_Exception;
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Cleaned up stale enclave socket '{}': {}", File, _Exception);
								CFile::fs_DeleteFile(File);
							}
						}
						catch (NException::CException const &_Exception)
						{
							(void)_Exception;
							DMibLogWithCategory(Mib/Concurrency/App, Error, "Failed to clean up enclave socket '{}': {}", File, _Exception);
						}
					}
				}
				catch (NException::CException const &_Exception)
				{
					(void)_Exception;
					DMibLogWithCategory(Mib/Concurrency/App, Error, "Failed to clean up enclave sockets: {}", _Exception);
				}
			}
			> fg_DiscardResult()
		;
	}

#ifdef DPlatformFamily_Windows
	void CDistributedAppActor::fp_CleanupOldExecutables()
	{
		if (!mp_CleanupFilesActor)
			mp_CleanupFilesActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Cleanup files"));

		g_Dispatch(mp_CleanupFilesActor) / []
			{
				try
				{
					for (auto &File : CFile::fs_FindFiles(CFile::fs_GetProgramPath() + "~*.TMP", EFileAttrib_File))
					{
						try
						{
							CFile::fs_DeleteFile(File);
						}
						catch (NException::CException const &)
						{
						}
					}
				}
				catch (NException::CException const &)
				{
				}
			}
			> fg_DiscardResult()
		;
	}
#endif

	TCFuture<void> CDistributedAppActor::fp_Initialize(NEncoding::CEJSON const &_Params)
	{
		TCPromise<void> Promise;
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Loading config file and state");

		fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		fp_CleanupOldExecutables();
#endif

		mp_State.m_StateDatabase.f_Load()
			+ mp_State.m_ConfigDatabase.f_Load()
			> Promise / [this, Promise, _Params]()
			{
				if (mp_State.m_bStoppingApp)
					return Promise.f_SetException(DMibErrorInstance("Startup aborted"));
				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Initializing trust manager");
				NFunction::TCFunctionMovable<NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)>
					fManagerFactor
				;

				if (mp_Settings.m_bSeparateDistributionManager)
				{
					fManagerFactor = [](CActorDistributionManagerInitSettings const &_Settings)
						{
							return fg_ConstructActor<CActorDistributionManager>(_Settings);
						}
					;
				}

				int32 DefaultConcurrency = 1;
				if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("DefaultConnectionConcurrency", EJSONType_Integer))
					DefaultConcurrency = fg_Clamp(pValue->f_Integer(), 1, 128);

				fp64 InitialConnectionTimeout = 5.0;
				if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("InitialConnectionTimeout", EJSONType_Float))
					InitialConnectionTimeout = fg_Clamp(pValue->f_Float(), 0.1, 3600.0);

				bool bSupportAuthentication = mp_Settings.m_bSupportUserAuthentication;
				if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("SupportAuthentication", EJSONType_Boolean))
					bSupportAuthentication = pValue->f_Boolean();

				CDistributedActorTrustManager::COptions Options;

				Options.m_fConstructManager = fg_Move(fManagerFactor);
				Options.m_KeySetting = mp_Settings.m_KeySetting;
				Options.m_ListenFlags = mp_Settings.m_ListenFlags;
				Options.m_FriendlyName = mp_Settings.f_GetCompositeFriendlyName();
				Options.m_Enclave = mp_Settings.m_Enclave;
				Options.m_TranslateHostnames = fp_GetTranslateHostnames();
				Options.m_InitialConnectionTimeout = InitialConnectionTimeout;
				Options.m_bWaitForConnectionsDuringInit = mp_Settings.m_bWaitForRemotes;
				Options.m_DefaultConnectionConcurrency = DefaultConcurrency;
				Options.m_bSupportAuthentication = bSupportAuthentication;

				mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(mp_TrustManagerDatabase, fg_Move(Options));

				mp_State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize)
					> Promise % "Failed to initialize trust manager" / [this, Promise, _Params]()
					{
						if (mp_State.m_bStoppingApp)
							return Promise.f_SetException(DMibErrorInstance("Startup aborted"));

						mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager)
							+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID)
							> Promise % "Failed to initialize trust manager"
							/ [this, Promise, _Params](NConcurrency::TCActor<NConcurrency::CActorDistributionManager> &&_DistributionManager, CStr &&_HostID)
							{
								if (mp_State.m_bStoppingApp)
								{
									if (mp_Settings.m_bSeparateDistributionManager)
									{
										_DistributionManager->f_Destroy() > [Promise](TCAsyncResult<void> &&)
											{
												Promise.f_SetException(DMibErrorInstance("Startup aborted"));
											}
										;
										return;
									}
									return Promise.f_SetException(DMibErrorInstance("Startup aborted"));
								}
								mp_State.m_DistributionManager = fg_Move(_DistributionManager);
								mp_State.m_HostID = fg_Move(_HostID);
								fp_SetupListen()
									+ fp_SetupAppServerInterface(_Params)
									> Promise % "Failed to setup listen config or app server interface" / [this, Promise]()
									{
										if (mp_State.m_bStoppingApp)
											return Promise.f_SetException(DMibErrorInstance("Startup aborted"));
										fp_SetupCommandLineTrust()
											> Promise % "Failed to setup commmand line trust" / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
									}
								;
							}
						;
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	namespace
	{
		[[maybe_unused]] ch8 const *fg_GetAppTypeName(EDistributedAppType _AppType)
		{
			switch (_AppType)
			{
			case EDistributedAppType_InProcess: return "in process";
			case EDistributedAppType_Daemon: return "as daemon";
			case EDistributedAppType_Local: return "as local";
			case EDistributedAppType_ForceLocal: return "as forced local";
			case EDistributedAppType_CommandLine:  return "as command line";
			case EDistributedAppType_DirectCommandLine:  return "as direct command line";
			}
			return "invalid app type";
		}
	}

	void CDistributedAppActor::f_LogApplicationInfo()
	{
		CStr ProgramPath = CFile::fs_GetProgramPath();

		NProcess::CVersionInfo VersionInfo;
		NProcess::NPlatform::fg_Process_GetVersionInfo(ProgramPath, VersionInfo);

		DMibLogWithCategory
			(
			 	Mib/Concurrency/App
			 	, Info
			 	, "{} running {} in {} {}.{}.{}.{} with pid {} built from {} [{}] at {tc5}"
			 	, mp_Settings.m_AppName
			 	, fg_GetAppTypeName(mp_AppType)
			 	, CFile::fs_GetFileNoExt(ProgramPath)
				, VersionInfo.m_Major
				, VersionInfo.m_Minor
				, VersionInfo.m_Revision
				, VersionInfo.m_MinorRevision
				, NProcess::NPlatform::fg_Process_GetCurrentUID()
				, VersionInfo.m_GitCommit
				, VersionInfo.m_GitBranch
				, VersionInfo.m_BuildTime
			)
		;
	}

	void CDistributedAppActor::f_SetAppType(EDistributedAppType _AppType)
	{
		DMibRequire(_AppType != EDistributedAppType_Unknown);

		if (mp_AppType == _AppType || mp_AppType == EDistributedAppType_Unchanged)
			return;

		mp_AppType = _AppType;

		CStr NewLogDirectory;
		switch (_AppType)
		{
		case EDistributedAppType_InProcess:
			break;
		case EDistributedAppType_Daemon:
		case EDistributedAppType_Local:
		case EDistributedAppType_ForceLocal:
			NewLogDirectory = mp_Settings.m_RootDirectory + "/Log";
			break;
		case EDistributedAppType_CommandLine:
		case EDistributedAppType_DirectCommandLine:
			NewLogDirectory = mp_Settings.m_RootDirectory + "/Log/CommandLine";
			break;
		}

		if (!NewLogDirectory.f_IsEmpty() && NewLogDirectory != mp_CurrentLogDirectory)
		{
			mp_CurrentLogDirectory = NewLogDirectory;
			fg_GetSys()->f_SetDefaultLogFileDirectory(NewLogDirectory);
		}

		if (_AppType == EDistributedAppType_CommandLine)
			f_LogApplicationInfo();
	}

	TCFuture<NStr::CStr> CDistributedAppActor::f_StartApp(NEncoding::CEJSON const &_Params, TCActor<CActor> const &_LogActor, EDistributedAppType _AppType)
	{
		f_SetAppType(_AppType);

		if (_AppType != EDistributedAppType_CommandLine)
			f_LogApplicationInfo();

		if (mp_State.m_bStoppingApp)
			co_return DMibErrorInstance("Startup aborted");

		mp_State.m_LogActor = _LogActor;

		if (!mp_pInitOnce)
		{
			mp_FileOperationsActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Distributed app file access"));
			mp_TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>
				(
					mp_Settings.m_RootDirectory + fg_Format("/TrustDatabase.{}", mp_Settings.m_AppName)
				)
			;

			mp_pInitOnce = fg_Construct
				(
					self
					, [this, _Params]() -> TCFuture<void>
					{
						return fp_Initialize(_Params);
					}
					, false
				)
			;
		}

		auto InitializeCall = self / [this, _Params]() -> TCFuture<void>
			{
				co_await ((*mp_pInitOnce)() % "Failed to initialize");

				if (mp_State.m_bStoppingApp)
					co_return DMibErrorInstance("Startup aborted");

				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Running specific application startup");

				co_await (self(&CDistributedAppActor::fp_StartApp, _Params) % "Failed to start app");

				if (mp_State.m_bStoppingApp)
					co_return DMibErrorInstance("Startup aborted");

				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Specific application startup finished");

				co_await (fp_PublishCommandLine() % "Failed to publish command line");

				co_return {};
			}
		;

		mp_AppStartupResult = co_await fg_Move(InitializeCall).f_Wrap();

		for (auto &StartupResult : mp_DeferredAppStartupResults)
			StartupResult.f_SetResult(mp_AppStartupResult);

		mp_DeferredAppStartupResults.f_Clear();

		if (!mp_AppStartupResult)
		{
			DMibLogWithCategory(Mib/Concurrency/App, Error, "{}", mp_AppStartupResult.f_GetExceptionStr());
			co_return mp_AppStartupResult.f_GetException();
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "App startup finished: {}", mp_Settings.m_AppName);
		co_return mp_State.m_HostID;
	}

	void CDistributedAppActor::fp_Construct()
	{
		mp_State.m_AppActor = fg_ThisActor(this);
		mp_pCommandLineSpec = fg_Construct();
		fp_BuildDefaultCommandLine(*mp_pCommandLineSpec, mp_Settings.m_DefaultCommandLineFunctionality);
		fp_BuildCommandLine(*mp_pCommandLineSpec);
	}

	TCFuture<void> CDistributedAppActor::f_StopApp()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Info, "App shutting down");
		TCPromise<void> Promise;

		mp_State.m_bStoppingApp = true;

		self / [this]() -> TCFuture<void>
			{
				if (mp_CommandLine)
				{
					TCPromise<void> Promise;
					mp_CommandLine->f_Destroy() > Promise;
					mp_CommandLine = nullptr;
					return Promise.f_MoveFuture();
				}
				mp_pStdInCleanup.f_Clear();
				return fg_Explicit();
			}
			> Promise % "Failed to stop command line interface" / [this, Promise]
			{
				self(&CDistributedAppActor::fp_StopApp) > Promise % "Failed to stop app" / [this, Promise]
					{
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific app successfully stopped, destroying app interface");

						self / [this]() -> TCFuture<void>
							{
								TCActorResultVector<void> Destroys;

								if (mp_AppInterfaceClientTrustProxy)
									mp_AppInterfaceClientTrustProxy->f_Destroy() > Destroys.f_AddResult();

								if (mp_AppInterfaceClientRegistrationSubscription)
									mp_AppInterfaceClientRegistrationSubscription->f_Destroy() > Destroys.f_AddResult();

								mp_AppInteraceServerSubscription.f_Destroy() > Destroys.f_AddResult();;
								mp_AppInterfaceClientImplementation.f_Destroy() > Destroys.f_AddResult();;

								co_await Destroys.f_GetResults().f_Timeout(10.0, "Timed out waiting for destroy of app interface");

								co_return {};
							}
							> Promise / [Promise]
							{
								DMibLogWithCategory(Mib/Concurrency/App, Info, "App interface destroyed");
								Promise.f_SetResult();
							}
						;
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_Destroy()
	{
		TCActorResultVector<void> Destroys;
		if (mp_Settings.m_bSeparateDistributionManager && mp_State.m_DistributionManager)
			mp_State.m_DistributionManager->f_Destroy() > Destroys.f_AddResult();
		if (mp_State.m_TrustManager)
			mp_State.m_TrustManager->f_Destroy() > Destroys.f_AddResult();

		if (mp_FileOperationsActor)
			mp_FileOperationsActor->f_Destroy() > Destroys.f_AddResult();
		if (mp_CleanupFilesActor)
			mp_CleanupFilesActor->f_Destroy() > Destroys.f_AddResult();

		co_await Destroys.f_GetResults();
		co_return {};
	}

	TCActor<CActor> fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params)
	{
		TCActor<CActor> LogActor;

#if DMibEnableTrace > 0
		if (auto *pParam = _Params.f_GetMember("TraceLogger", EJSONType_Boolean))
		{
			if (!pParam->f_Boolean())
				fg_GetSys()->f_RemoveTraceLogger();
		}
#endif
		if (auto *pParam = _Params.f_GetMember("StdErrLogger", EJSONType_Boolean))
		{
			if (pParam->f_Boolean())
				fg_GetSys()->f_AddStdErrLogger();
		}
#if (DMibSysLogSeverities) != 0
		if (auto *pParam = _Params.f_GetMember("ConcurrentLogging", EJSONType_Boolean))
		{
			if (pParam->f_Boolean())
			{
				LogActor = NMib::NConcurrency::fg_ConstructActor<NMib::NConcurrency::CSeparateThreadActor>(fg_Construct("Log Dispatcher"));
				fg_GetSys()->f_GetLogger().f_SetDispatcher
					(
						[LogActor](NFunction::TCFunctionMovable<void ()> &&_fToDispatch)
						{
							fg_Dispatch
								(
									LogActor
									, fg_Move(_fToDispatch)
								)
								> fg_DiscardResult()
							;
						}
					)
				;
			}
		}
#endif

		return LogActor;
	}

	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine
			, bool _bStartApp
		)
	{
		bool bStartedApp = false;
		TCActor<CDistributedAppActor> AppActor;
		aint Ret = 1;
		try
		{
			{
				auto &SuggestedEnclave = fg_DistributedAppThreadLocal().m_DefaultSettings.m_Enclave;
				auto &bSuggestedWaitForRemotes = fg_DistributedAppThreadLocal().m_DefaultSettings.m_bWaitForRemotes;

				auto OldEnclave = SuggestedEnclave;
				SuggestedEnclave = NCryptography::fg_RandomID();

				auto bOldWaitForRemotes = bSuggestedWaitForRemotes;
				bSuggestedWaitForRemotes = false;

				auto Cleanup = g_OnScopeExit > [&]
					{
						SuggestedEnclave = OldEnclave;
						bSuggestedWaitForRemotes = bOldWaitForRemotes;
					}
				;
				AppActor = _fActorFactory();
			}
			bool bInstalledLogDispatcher = false;
			auto CleanupLogDispatcher = g_OnScopeExit > [&]
				{
#if (DMibSysLogSeverities) != 0
					if (bInstalledLogDispatcher)
						fg_GetSys()->f_GetLogger().f_SetDispatcher(nullptr);
#endif
				}
			;
			CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync();

			if (_fMutateCommandLine)
				CommandLineClient.f_MutateCommandLineSpecification(_fMutateCommandLine);

			CommandLineClient.f_SetLazyPreRunDirectCommand
				(
					[&](NEncoding::CEJSON const &_Params)
					{
						AppActor(&CDistributedAppActor::f_SetAppType, EDistributedAppType_DirectCommandLine).f_CallSync();
					}
				)
			;

			CommandLineClient.f_SetLazyStartApp
				(
					[&](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineSpecification::ECommandFlag _Flags)
					{
						EDistributedAppType AppType = EDistributedAppType_Unchanged;
						if (_Flags & CDistributedAppCommandLineSpecification::ECommandFlag_RunLocalApp)
							AppType = EDistributedAppType_ForceLocal;
						else if (_bStartApp)
							AppType = EDistributedAppType_Local;
						else
							AppType = EDistributedAppType_CommandLine;

						TCActor<CActor> LogActor = fg_ApplyLoggingOption(_Params);
						if (LogActor)
							bInstalledLogDispatcher = true;
						if (_bStartApp || (_Flags & CDistributedAppCommandLineSpecification::ECommandFlag_RunLocalApp))
						{
							AppActor(&CDistributedAppActor::f_StartApp, _Params, LogActor, AppType).f_CallSync();
							bStartedApp = true;
						}
						else if (AppType != EDistributedAppType_Unchanged)
							AppActor(&CDistributedAppActor::f_SetAppType, AppType).f_CallSync();
					}
				)
			;
			Ret = CommandLineClient.f_RunCommandLine(fg_GetSys()->f_GetCommandLineArgs());
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("{}{\n}", _Exception.f_GetErrorStr());

			Ret = 1;
		}

		try
		{
			if (bStartedApp)
				AppActor(&CDistributedAppActor::f_StopApp).f_CallSync();

			if (AppActor)
				AppActor->f_BlockDestroy();
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("Error stopping app: {}{\n}", _Exception.f_GetErrorStr());
		}

		return Ret;
	}

	extern void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorPassword_MakeActive();
	extern void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorU2F_MakeActive();

	void CDistributedAppActor::fp_MakeActive()
	{
		fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorPassword_MakeActive();
		fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorU2F_MakeActive();
	}

#if DMibConfig_Tests_Enable
	TCFuture<CEJSON> CDistributedAppActor::f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		return self(&CDistributedAppActor::fp_Test_Command, _Command, _Params);
	}

	TCFuture<CEJSON> CDistributedAppActor::fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		return fg_Explicit();
	}
#endif
}
