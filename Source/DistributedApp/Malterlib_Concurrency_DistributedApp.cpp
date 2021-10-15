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

		constinit TCSubSystem<CSubSystem_Concurrency_DistributedApp, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_DistributedApp = {DAggregateInit};

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

	CDistributedAppActor::CInternal::CInternal() = default;
	CDistributedAppActor::CInternal::~CInternal() = default;

	CDistributedAppActor::CDistributedAppActor(CDistributedAppActor_Settings const &_Settings)
		: mp_State(_Settings, *this)
		, mp_Settings(_Settings)
		, mp_pInternal(fg_Construct())
	{
		mp_State.m_LocalAddress = fp_GetLocalAddress();

		auto &Internal = *mp_pInternal;

		if (Internal.m_AppType != EDistributedAppType_InProcess)
		{
			fg_GetSys()->f_SetDefaultLogFileName(fg_Format("{}.log", _Settings.m_AppName));
			Internal.m_CurrentLogDirectory = _Settings.m_RootDirectory + "/Log";
			fg_GetSys()->f_SetDefaultLogFileDirectory(Internal.m_CurrentLogDirectory);
		}

		fp_MakeActive();
	}

	CDistributedAppActor::~CDistributedAppActor() = default;

#if DMibEnableSafeCheck > 0
	void CDistributedAppActor::fp_CheckDestroy()
	{
		auto &Internal = *mp_pInternal;

		// Did you forget to call CDistributedAppActor::fp_Destroy in your derived class?:
		// co_await CDistributedAppActor::fp_Destroy();
		DMibFastCheck(Internal.m_bDestroyCalled);
	}
#endif

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
		TCPromise<void> Promise;

		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Setting up listen config");

		TCSet<CDistributedActorTrustManager_Address> WantedListens;

		CDistributedActorTrustManager_Address LocalListen;
		LocalListen.m_URL = fp_GetLocalAddress();

		auto &Internal = *mp_pInternal;

		bool bSetPrimary = false;

		auto const *pListen = mp_State.m_ConfigDatabase.m_Data.f_GetMember("Listen", EJSONType_Array);
		if (pListen)
		{
			bool bFirst = true;
			for (auto &Object : pListen->f_Array())
			{
				if (!Object.f_IsObject())
					return Promise <<= DMibErrorInstance("Invalid listen entry. Entry needs to be an object with 'Address' specified");

				CDistributedActorTrustManager_Address Address;
				if (auto pAddress = Object.f_GetMember("Address"))
				{
					if (pAddress->f_Type() != EJSONType_String)
						return Promise <<= DMibErrorInstance("Listen 'Address' has wrong type, only string is supported");
					if (!Address.m_URL.f_Decode(pAddress->f_String()))
						return Promise <<= DMibErrorInstance(fg_Format("Invalid listen URL '{}'", pAddress->f_String()));
					if (Address.m_URL.f_GetScheme() != "wss")
						return Promise <<= DMibErrorInstance(fg_Format("Invalid scheme in URL '{}': Only wss is currently supported", pAddress->f_String()));
				}
				else
					return Promise <<= DMibErrorInstance("Missing 'Address' for listen entry");

				if (bFirst)
				{
					Internal.m_PrimaryListen = Address;
					bSetPrimary = true;
				}
				bFirst = false;

				if (Address == LocalListen)
				{
					return Promise <<= DMibErrorInstance
						(
							fg_Format("'{}' cannot be used as listen address, it is reserved for local command line communication", fp_GetLocalAddress().f_Encode())
						)
					;
				}
				WantedListens[Address];
			}
		}
		WantedListens[LocalListen];

		if (!bSetPrimary && mp_Settings.m_bCanUserLocalListenAsPrimary)
			Internal.m_PrimaryListen = LocalListen;

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
		auto &Internal = *mp_pInternal;

		if (mp_Settings.m_Enclave.f_IsEmpty())
			return;

		if (!Internal.m_CleanupFilesActor)
			Internal.m_CleanupFilesActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Cleanup files"));

		g_Dispatch(Internal.m_CleanupFilesActor) / [WildcardPath = mp_Settings.f_GetLocalSocketWildcard(true)]
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
					[[maybe_unused]] auto &Exception = _Exception;
					DMibLogWithCategory(Mib/Concurrency/App, Error, "Failed to clean up enclave sockets: {}", Exception);
				}
			}
			> fg_DiscardResult()
		;
	}

#ifdef DPlatformFamily_Windows
	void CDistributedAppActor::fp_CleanupOldExecutables()
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_CleanupFilesActor)
			Internal.m_CleanupFilesActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Cleanup files"));

		g_Dispatch(Internal.m_CleanupFilesActor) / []
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

				auto &Internal = *mp_pInternal;

				mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(Internal.m_TrustManagerDatabase, fg_Move(Options));

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
										fg_Move(_DistributionManager).f_Destroy() > [Promise](TCAsyncResult<void> &&)
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
								fp_SetupListen() + fp_SetupAppServerInterface(_Params)
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
			case EDistributedAppType_Unknown: return "unknown";
			case EDistributedAppType_InProcess: return "in process";
			case EDistributedAppType_Daemon: return "as daemon";
			case EDistributedAppType_Local: return "as local";
			case EDistributedAppType_ForceLocal: return "as forced local";
			case EDistributedAppType_CommandLine:  return "as command line";
			case EDistributedAppType_DirectCommandLine:  return "as direct command line";
			case EDistributedAppType_Unchanged:
				DMibNeverGetHere;
				break;
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
			 	, fg_GetAppTypeName(mp_pInternal->m_AppType)
			 	, CFile::fs_GetFileNoExt(ProgramPath)
				, VersionInfo.m_Major
				, VersionInfo.m_Minor
				, VersionInfo.m_Revision
				, VersionInfo.m_MinorRevision
				, NProcess::NPlatform::fg_Process_GetCurrentUID()
				, VersionInfo.m_GitCommit
				, VersionInfo.m_GitBranch
				, VersionInfo.m_BuildTime.f_ToLocal()
			)
		;
	}

	void CDistributedAppActor::f_SetAppType(EDistributedAppType _AppType)
	{
		DMibRequire(_AppType != EDistributedAppType_Unknown);

		auto &Internal = *mp_pInternal;

		if (Internal.m_AppType == _AppType || _AppType == EDistributedAppType_Unchanged)
			return;

		Internal.m_AppType = _AppType;

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
		case EDistributedAppType_Unknown:
		case EDistributedAppType_Unchanged:
			DMibNeverGetHere;
			break;
		}

		if (!NewLogDirectory.f_IsEmpty() && NewLogDirectory != Internal.m_CurrentLogDirectory)
		{
			Internal.m_CurrentLogDirectory = NewLogDirectory;
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

		auto &Internal = *mp_pInternal;

		mp_State.m_LogActor = _LogActor;

		if (!Internal.m_pInitOnce)
		{
			Internal.m_FileOperationsActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Distributed app file access"));
			Internal.m_TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>
				(
					mp_Settings.m_RootDirectory + fg_Format("/TrustDatabase.{}", mp_Settings.m_AppName)
				)
			;

			Internal.m_pInitOnce = fg_Construct
				(
					g_ActorFunctor / [this, _Params]() -> TCFuture<void>
					{
						return fp_Initialize(_Params);
					}
					, false
				)
			;
		}

		auto InitializeCall = self / [this, _Params]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				co_await ((*Internal.m_pInitOnce)() % "Failed to initialize");

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

		Internal.m_AppStartupResult = co_await fg_Move(InitializeCall).f_Wrap();

		for (auto &StartupResult : Internal.m_DeferredAppStartupResults)
			StartupResult.f_SetResult(Internal.m_AppStartupResult);

		Internal.m_DeferredAppStartupResults.f_Clear();

		if (!Internal.m_AppStartupResult)
		{
			DMibLogWithCategory(Mib/Concurrency/App, Error, "{}", Internal.m_AppStartupResult.f_GetExceptionStr());
			co_return Internal.m_AppStartupResult.f_GetException();
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "App startup finished: {}", mp_Settings.m_AppName);
		co_return mp_State.m_HostID;
	}

	void CDistributedAppActor::fp_Construct()
	{
		mp_State.m_AppActor = fg_ThisActor(this);

		auto &Internal = *mp_pInternal;

		Internal.m_pCommandLineSpec = fg_Construct();
		fp_BuildDefaultCommandLine(*Internal.m_pCommandLineSpec, mp_Settings.m_DefaultCommandLineFunctionality);
		fp_BuildCommandLine(*Internal.m_pCommandLineSpec);
	}

	TCFuture<void> CDistributedAppActor::f_StopApp()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Info, "App shutting down");

		mp_State.m_bStoppingApp = true;

		co_await (self(&CDistributedAppActor::fp_StopApp) % "Failed to stop app");

		auto &Internal = *mp_pInternal;

		if (Internal.m_CommandLine)
			co_await (fg_Move(Internal.m_CommandLine).f_Destroy() % "Failed to stop command line interface");

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific app successfully stopped, destroying app interface");

		{
			TCActorResultVector<void> Destroys;

			if (Internal.m_AppInterfaceClientTrustProxy)
				Internal.m_AppInterfaceClientTrustProxy.f_Destroy() > Destroys.f_AddResult();

			if (Internal.m_AppInterfaceClientRegistrationSubscription)
				Internal.m_AppInterfaceClientRegistrationSubscription->f_Destroy() > Destroys.f_AddResult();

			Internal.m_AppInteraceServerSubscription.f_Destroy() > Destroys.f_AddResult();;
			Internal.m_AppInterfaceClientImplementation.f_Destroy() > Destroys.f_AddResult();;

			co_await Destroys.f_GetResults().f_Timeout(10.0, "Timed out waiting for destroy of app interface");
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "App interface destroyed");

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

#if DMibEnableSafeCheck > 0
		Internal.m_bDestroyCalled = true;
#endif
		TCActorResultVector<void> Destroys;
		if (mp_Settings.m_bSeparateDistributionManager && mp_State.m_DistributionManager)
			mp_State.m_DistributionManager.f_Destroy() > Destroys.f_AddResult();
		if (mp_State.m_TrustManager)
			mp_State.m_TrustManager.f_Destroy() > Destroys.f_AddResult();

		if (Internal.m_FileOperationsActor)
			Internal.m_FileOperationsActor.f_Destroy() > Destroys.f_AddResult();
		if (Internal.m_CleanupFilesActor)
			Internal.m_CleanupFilesActor.f_Destroy() > Destroys.f_AddResult();

		Internal.m_AppSensorStoreLocalInitSequencer.f_Abort() > Destroys.f_AddResult();
		Internal.m_AppSensorStoreLocalAppServerChangeSequencer.f_Abort() > Destroys.f_AddResult();

		if (Internal.m_AppSensorStoreLocal)
			fg_Move(Internal.m_AppSensorStoreLocal).f_Destroy() > Destroys.f_AddResult();

		if (Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription)
			fg_Exchange(Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription, nullptr)->f_Destroy() > Destroys.f_AddResult();
		
		if (Internal.m_AppSensorStoreLocalAppServerChangeSubscription)
			fg_Exchange(Internal.m_AppSensorStoreLocalAppServerChangeSubscription, nullptr)->f_Destroy() > Destroys.f_AddResult();

		for (auto &Subscription : Internal.m_AppInterfaceServerChangeSubscriptions)
			fg_Move(Subscription).f_Destroy() > Destroys.f_AddResult();

		co_await Destroys.f_GetResults();

		if (Internal.m_TrustManagerDatabase)
			co_await Internal.m_TrustManagerDatabase.f_Destroy();

		if (Internal.m_pInitOnce)
			co_await Internal.m_pInitOnce->f_Destroy();

		co_return {};
	}

#if (DMibSysLogSeverities) != 0
	struct CLogToStdErrAnsi
	{
		CLogToStdErrAnsi(NCommandLine::EAnsiEncodingFlag _AnsiFlags, bool _bTrace)
			: mp_AnsiEncoding(_AnsiFlags)
			, mp_bTrace(_bTrace)
		{
			mp_TimeColor = mp_AnsiEncoding.f_ForegroundRGB(128, 128, 128);
			mp_DebugColor = mp_AnsiEncoding.f_ForegroundRGB(100, 100, 100);
			mp_CategoryColor = mp_AnsiEncoding.f_ForegroundRGB(51, 182, 255);
			mp_Indent = "                                                                     ";
		}

		void operator()
			(
				mint _ThreadID
				, NTime::CTime const &_Time
				, NLog::ESeverity _Sev
				, NLog::CLogStr const &_Message
				, NContainer::TCVector<NStr::CStr> const &_Categories
				, NContainer::TCVector<NStr::CStr> const &_Operations
				, NLog::CLogLocationTag const& _Loc
			)
		{
			NTime::CTimeConvert::CDateTime DateTime;
			NTime::CTimeConvert(_Time.f_ToLocal()).f_ExtractDateTime(DateTime);

			auto SeverityString = [&]() -> NStr::CStr
				{
					if (mp_AnsiEncoding.f_Color())
					{
						if (!_Operations.f_IsEmpty())
							return _Operations.f_GetFirst();
						else
							return NLog::fg_GetSeverityName(_Sev);
					}
					else
					{
						if (!_Operations.f_IsEmpty())
							return NStr::fg_Format("[{}]", _Operations.f_GetFirst());
						else
							return NStr::fg_Format("[{}]", NLog::fg_GetSeverityName(_Sev));
					}
				}
				()
			;

			mint SeverityOffset = fg_Max((10 - SeverityString.f_GetLen()) / 2, 0);

			auto CategoryString = [&]() -> NStr::CStr
				{
					if (_Categories.f_IsEmpty())
						return {};

					if (mp_AnsiEncoding.f_Color())
						return _Categories.f_GetFirst();

					return NStr::fg_Format("<{}>", _Categories.f_GetFirst());
				}
				()
			;

			NStr::CStrNonTracked OutputString = NStr::CStrNonTracked::CFormat
				(
					"{}{}-{sj2,sf0}-{sj2,sf0} {sj2,sf0}:{sj2,sf0}:{sj2,sf0}.{fr1,fe3}{}  {}{sj32}{} {}{sj10,a-*}{} {}{\n}"
				)
				<< mp_TimeColor
				<< DateTime.m_Year
				<< DateTime.m_Month
				<< DateTime.m_DayOfMonth
				<< DateTime.m_Hour
				<< DateTime.m_Minute
				<< DateTime.m_Second
				<< DateTime.m_Fraction
				<< mp_AnsiEncoding.f_Default()
				<< mp_CategoryColor
				<< CategoryString
				<< mp_AnsiEncoding.f_Default()
				<< [&]() -> CStr const &
				{
					switch(_Sev)
					{
					case NLog::ESeverity_None:
					case NLog::ESeverity_Info:
						return mp_AnsiEncoding.f_StatusNormal();
					case NLog::ESeverity_Debug:
					case NLog::ESeverity_DebugVerbose1:
					case NLog::ESeverity_DebugVerbose2:
					case NLog::ESeverity_Perf_Info:
						return mp_DebugColor;
					case NLog::ESeverity_Warning:
					case NLog::ESeverity_Perf_Warning:
						return mp_AnsiEncoding.f_StatusWarning();
					case NLog::ESeverity_Critical:
					case NLog::ESeverity_Error:
					case NLog::ESeverity_Perf_Error:
						return mp_AnsiEncoding.f_StatusError();
					}

					return mp_AnsiEncoding.f_Default();
				}
				()
				<< SeverityString
				<< SeverityOffset
				<< mp_AnsiEncoding.f_Default()
				<< _Message.f_Indent(mp_Indent, false)
			;

			if (mp_bTrace)
				DMibTraceRaw(OutputString.f_GetStr());
			else
				DMibConErrOutRaw(OutputString.f_GetStr());
		}

	private:
		NCommandLine::CAnsiEncoding mp_AnsiEncoding;
		CStr mp_EmptyColor;
		CStr mp_TimeColor;
		CStr mp_CategoryColor;
		CStr mp_DebugColor;
		CStr mp_Indent;
		bool mp_bTrace = false;
	};
#endif

	TCActor<CActor> fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params)
	{
		TCActor<CActor> LogActor;

#if DMibEnableTrace > 0
		if (auto *pParam = _Params.f_GetMember("TraceLogger", EJSONType_Boolean))
		{
			if (!pParam->f_Boolean())
				fg_GetSys()->f_RemoveTraceLogger();
			else if (fg_GetSys()->f_HasTraceLogger())
			{
				fg_GetSys()->f_RemoveTraceLogger();
				fg_GetSys()->f_GetLogger().f_PushGlobalDestination(CLogToStdErrAnsi(NCommandLine::EAnsiEncodingFlag_None, true));
			}
		}
#endif
#if (DMibSysLogSeverities) != 0
		if (auto *pParam = _Params.f_GetMember("StdErrLogger", EJSONType_Boolean))
		{
			if (pParam->f_Boolean())
				fg_GetSys()->f_GetLogger().f_PushGlobalDestination(CLogToStdErrAnsi(NCommandLine::CCommandLineDefaults::fs_ParseAnsiEncodingParams(_Params), false));
		}
#endif
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

	aint CRunDistributedAppHelper::f_RunCommandLine
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine
			, bool _bStartApp
			, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
		)
	{
		m_pRunLoop = _pRunLoop;

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
				m_AppActor = _fActorFactory();
			}
			auto CleanupLogDispatcher = g_OnScopeExit > [&]
				{
#if (DMibSysLogSeverities) != 0
					if (m_bInstalledLogDispatcher)
					{
						fg_GetSys()->f_GetLogger().f_SetDispatcher(nullptr);
						if (m_LogActor)
							m_LogActor->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
					}
#endif
				}
			;
			CDistributedAppCommandLineClient CommandLineClient = m_AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync(m_pRunLoop);

			if (_fMutateCommandLine)
				CommandLineClient.f_MutateCommandLineSpecification(_fMutateCommandLine);

			CommandLineClient.f_SetLazyPreRunDirectCommand
				(
					[this](NEncoding::CEJSON const &_Params, EDistributedAppCommandFlag _Flags)
					{
						if (_Flags & EDistributedAppCommandFlag_RunLocalApp)
						{
							EDistributedAppType AppType = EDistributedAppType_ForceLocal;

							m_LogActor = fg_ApplyLoggingOption(_Params);
							if (m_LogActor)
								m_bInstalledLogDispatcher = true;

							m_AppActor(&CDistributedAppActor::f_StartApp, _Params, m_LogActor, AppType).f_CallSync(m_pRunLoop);
							m_bStartedApp = true;
						}

						m_AppActor(&CDistributedAppActor::f_SetAppType, EDistributedAppType_DirectCommandLine).f_CallSync(m_pRunLoop);
					}
				)
			;

			CommandLineClient.f_SetLazyStartApp
				(
					[&](NEncoding::CEJSON const &_Params, EDistributedAppCommandFlag _Flags) -> CDistributedAppCommandLineClient::FStopApp
					{
						EDistributedAppType AppType = EDistributedAppType_Unchanged;
						if (_Flags & EDistributedAppCommandFlag_RunLocalApp)
							AppType = EDistributedAppType_ForceLocal;
						else if (_bStartApp)
							AppType = EDistributedAppType_Local;
						else
							AppType = EDistributedAppType_CommandLine;

						m_LogActor = fg_ApplyLoggingOption(_Params);
						if (m_LogActor)
							m_bInstalledLogDispatcher = true;
						if (_bStartApp || (_Flags & EDistributedAppCommandFlag_RunLocalApp))
						{
							m_AppActor(&CDistributedAppActor::f_StartApp, _Params, m_LogActor, AppType).f_CallSync(m_pRunLoop);
							m_bStartedApp = true;
						}
						else if (AppType != EDistributedAppType_Unchanged)
							m_AppActor(&CDistributedAppActor::f_SetAppType, AppType).f_CallSync(m_pRunLoop);

						return [this]
							{
								if (m_bStartedApp)
								{
									m_bStartedApp = false;
									m_AppActor(&CDistributedAppActor::f_StopApp).f_CallSync(m_pRunLoop);
								}
							}
						;
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

		return Ret;
	}

	void CRunDistributedAppHelper::f_Stop()
	{
		try
		{
			if (m_bStartedApp)
				m_AppActor(&CDistributedAppActor::f_StopApp).f_CallSync(m_pRunLoop);

			if (m_AppActor)
				m_AppActor->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("Error stopping app: {}{\n}", _Exception.f_GetErrorStr());
		}

		m_LogActor.f_Clear();
		m_AppActor.f_Clear();
		m_pRunLoop.f_Clear();
	}

	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine
			, bool _bStartApp
			, TCSharedPointer<CRunLoop> const &_pRunLoop
		)
	{
		TCSharedPointer<CRunLoop> pRunLoop = _pRunLoop;
		if (!pRunLoop)
			pRunLoop = fg_Construct<CDefaultRunLoop>();
		
		CRunDistributedAppHelper Helper;
		auto Ret = Helper.f_RunCommandLine(_fActorFactory, _fMutateCommandLine, _bStartApp, pRunLoop);
		Helper.f_Stop();

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
		return g_Future <<= self(&CDistributedAppActor::fp_Test_Command, _Command, _Params);
	}

	TCFuture<CEJSON> CDistributedAppActor::fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		co_return {};
	}
#endif
}
