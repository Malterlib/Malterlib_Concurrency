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
	using namespace NPtr;
	using namespace NNet;
	using namespace NDataProcessing;

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
		: mp_State(_Settings)
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
	}
	
	CDistributedAppActor::~CDistributedAppActor()
	{
	}

	void CDistributedAppActor::f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo)
	{
		CStr Category = _Category;
		if (Category.f_IsEmpty())
			Category = mp_Settings.m_AuditCategory;
		NMib::NLog::CSysLogCatScope Scope(NMib::fg_GetSys()->f_GetLogger(), Category);
		NMib::NLog::fg_SysLog(DLogLocTag, _Severity, "<{}> {}", _CallingHostInfo.f_GetHostInfo(), _Message);
	}

	CDistributedAppAuditor::CDistributedAppAuditor() = default;
	CDistributedAppAuditor::~CDistributedAppAuditor() = default;

	CDistributedAppAuditor::CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo)
		: mp_AppActor(_AppActor)
		, mp_CallingHostInfo(_CallingHostInfo)
	{
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
					, CurrentCallingHostInfo.f_GetUniqueHostID()
					, CurrentCallingHostInfo.f_GetRealHostID().f_IsEmpty()
					? CHostInfo(mp_State.m_HostID, FriendlyName) 
					: CHostInfo(CurrentCallingHostInfo.f_GetHostInfo().m_HostID, FriendlyName)
					, CurrentCallingHostInfo.f_LastExecutionID()
					, CurrentCallingHostInfo.f_GetProtocolVersion()
				}
			}
		;
	}
	
	CStr CDistributedAppAuditor::f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
 		mp_AppActor(&CDistributedAppActor::f_Audit, _Severity, _Message, _Category, mp_CallingHostInfo) > fg_DiscardResult();
		return _Message;
	}
	
	CStr CDistributedAppAuditor::f_Audit(NLog::ESeverity _Severity, NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		CStr FullMessage;
		CStr FirstMessage;
		for (CStr const &Message : _Message)
		{
			if (FirstMessage.f_IsEmpty())
				FirstMessage = Message;
			fg_AddStrSep(FullMessage, Message, " ");
		}
 		mp_AppActor(&CDistributedAppActor::f_Audit, _Severity, FullMessage, _Category, mp_CallingHostInfo) > fg_DiscardResult();
		return FirstMessage;
	}

	CStr CDistributedAppAuditor::f_Critical(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Critical, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Critical(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Critical, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Error(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Error, _Message, _Category);
	}

	CStr CDistributedAppAuditor::f_Error(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Error, _Message, _Category);
	}
	
	NException::CException CDistributedAppAuditor::f_Exception(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_Exception(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}
	
	CStr CDistributedAppAuditor::f_Warning(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Warning, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Warning(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Warning, _Message, _Category);
	}
	
	CStr CDistributedAppAuditor::f_Info(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Info, _Message, _Category);
	}

	CStr CDistributedAppAuditor::f_Info(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return f_Audit(NLog::ESeverity_Info, _Message, _Category);
	}
	
	CCallingHostInfo const &CDistributedAppAuditor::f_HostInfo() const
	{
		return mp_CallingHostInfo;
	}

	CDistributedAppAuditor CDistributedAppActor::f_Auditor(CCallingHostInfo const &_CallingHostInfo) const
	{
		return {fg_ThisActor(this), _CallingHostInfo};
	}
	
	CDistributedAppAuditor CDistributedAppState::f_Auditor(CCallingHostInfo const &_CallingHostInfo) const
	{
		return {m_AppActor, _CallingHostInfo};
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied(NStr::CStr const &_Message, NStr::CStr const &_Category) const
	{
		if (_Message.f_IsEmpty())
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, "Access denied", _Category));
		else
			return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, {"Access denied", _Message}, _Category));
	}
	
	NException::CException CDistributedAppAuditor::f_AccessDenied(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category) const
	{
		return DMibErrorInstance(f_Audit(NLog::ESeverity_Error, _Message, _Category));
	}
	
	NStr::CStr CDistributedAppActor::fp_GetLocalHostname(bool _bEnclaveSpecific) const
	{
		return mp_Settings.f_GetLocalSocketHostname(_bEnclaveSpecific);
	}
	
	NHTTP::CURL CDistributedAppActor::fp_GetLocalAddress() const
	{
		return NHTTP::CURL{fg_Format("wss://[{}]/", fp_GetLocalHostname(false))};
	}
	
	NContainer::TCMap<NStr::CStr, NStr::CStr> CDistributedAppActor::fp_GetTranslateHostnames() const
	{
		NContainer::TCMap<NStr::CStr, NStr::CStr> TranslateHostnames;
		if (!mp_Settings.m_Enclave.f_IsEmpty())
			TranslateHostnames[fp_GetLocalHostname(false)] = fp_GetLocalHostname(true);
		return TranslateHostnames;
	}
	
	TCContinuation<void> CDistributedAppActor::fp_SetupListen()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Setting up listen config");
		TCContinuation<void> Continuation;
		
		TCSet<CDistributedActorTrustManager_Address> WantedListens;
		
		auto const *pListen = mp_State.m_ConfigDatabase.m_Data.f_GetMember("Listen", EJSONType_Array);
		if (!pListen && mp_Settings.m_bRequireListen)
			return DMibErrorInstance(fg_Format("Missing 'Listen' array section in {}Config.json", mp_Settings.m_AppName));
		
		CDistributedActorTrustManager_Address LocalListen;
		LocalListen.m_URL = fp_GetLocalAddress(); 
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
			if (WantedListens.f_IsEmpty() && mp_Settings.m_bRequireListen)
				return DMibErrorInstance(fg_Format("At least one Listen entry needs to be in {}Config.json", mp_Settings.m_AppName));
		}
		WantedListens[LocalListen];
		
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens) 
			> Continuation % "Failed to enum current listen" / [this, Continuation, WantedListens](TCSet<CDistributedActorTrustManager_Address> &&_Listens)
			{
				TCActorResultVector<void> ChangesResults;
				bool bChanged = false;
				for (auto const &CurrentListen : _Listens)
				{
					if (WantedListens.f_FindEqual(CurrentListen))
						continue;
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, CurrentListen) > ChangesResults.f_AddResult();
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Removing listen config {}", CurrentListen.m_URL.f_Encode());
					bChanged = true;
				}
				for (auto const &WantedListen : WantedListens)
				{
					if (_Listens.f_FindEqual(WantedListen))
						continue;
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, WantedListen) > ChangesResults.f_AddResult();
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Adding listen config {}", WantedListen.m_URL.f_Encode());
					bChanged = true;
				}
				if (!bChanged)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "No listen config changes needed");
					Continuation.f_SetResult();
					return;
				}
				ChangesResults.f_GetResults() > [Continuation](TCAsyncResult<TCVector<TCAsyncResult<void>>> &&_Results)
					{
						if (!fg_CombineResults(Continuation, fg_Move(_Results)))
							return;
						
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Finished changing listen config");
						Continuation.f_SetResult();
					}
				;
			}
		;
		
		return Continuation;
	}

	void CDistributedAppActor::fp_CleanupEnclaveSockets()
	{
		if (mp_Settings.m_Enclave.f_IsEmpty())
			return;

		mp_CleanupSocketsActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Cleanup sockets"));

		g_Dispatch(mp_CleanupSocketsActor) > [WildcardPath = mp_Settings.f_GetLocalSocketWildcard(true)]
			{
				try
				{
					for (auto &File : CFile::fs_FindFiles(WildcardPath, EFileAttrib_File))
					{
						try
						{
							try
							{
								NNet::CNetAddress Address = NNet::CSocket::fs_ResolveAddress(fg_Format("UNIX:{}", File));
								NNet::CSocket Socket;
								Socket.f_Connect(Address);
							}
							catch (NException::CException const &)
							{
								CFile::fs_DeleteFile(File);
							}
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

	TCContinuation<void> CDistributedAppActor::fp_Initialize(NEncoding::CEJSON const &_Params)
	{
		TCContinuation<void> Continuation;
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Loading config file and state");
		
		fp_CleanupEnclaveSockets();
		
		mp_State.m_StateDatabase.f_Load()
			+ mp_State.m_ConfigDatabase.f_Load()
			> Continuation / [this, Continuation, _Params]()
			{
				if (mp_State.m_bStoppingApp)
					return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Initializing trust manager");
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
				
				CDistributedActorTrustManager::COptions Options;
				
				Options.m_fConstructManager = fg_Move(fManagerFactor);
				Options.m_KeySetting = mp_Settings.m_KeySetting;
				Options.m_ListenFlags = mp_Settings.m_ListenFlags;
				Options.m_FriendlyName = mp_Settings.f_GetCompositeFriendlyName();
				Options.m_Enclave = mp_Settings.m_Enclave;
				Options.m_TranslateHostnames = fp_GetTranslateHostnames();
				Options.m_InitialConnectionTimeout = InitialConnectionTimeout;
				Options.m_DefaultConnectionConcurrency = DefaultConcurrency;
				
				mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(mp_TrustManagerDatabase, fg_Move(Options));
				
				mp_State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize)
					> Continuation % "Failed to initialize trust manager" / [this, Continuation, _Params]()
					{
						if (mp_State.m_bStoppingApp)
							return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
						
						mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager) 
							+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID)
							> Continuation % "Failed to initialize trust manager"
							/ [this, Continuation, _Params](NConcurrency::TCActor<NConcurrency::CActorDistributionManager> &&_DistributionManager, CStr &&_HostID)
							{
								if (mp_State.m_bStoppingApp)
								{
									if (mp_Settings.m_bSeparateDistributionManager)
									{
										_DistributionManager->f_Destroy() > [Continuation](TCAsyncResult<void> &&)
											{
												Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
											}
										;
										return;
									}
									return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
								}
								mp_State.m_DistributionManager = fg_Move(_DistributionManager);
								mp_State.m_HostID = fg_Move(_HostID);
								fp_SetupListen()
									+ fp_SetupAppServerInterface(_Params)
									> Continuation % "Failed to setup listen config or app server interface" / [this, Continuation]()
									{
										if (mp_State.m_bStoppingApp)
											return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
										fp_SetupCommandLineTrust()
											> Continuation % "Failed to setup commmand line trust" / [Continuation]()
											{
												Continuation.f_SetResult();
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
		
		return Continuation;				
	}

	namespace
	{
		ch8 const *fg_GetAppTypeName(EDistributedAppType _AppType)
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

	TCContinuation<NStr::CStr> CDistributedAppActor::f_StartApp(NEncoding::CEJSON const &_Params, TCActor<CActor> const &_LogActor, EDistributedAppType _AppType)
	{
		f_SetAppType(_AppType);

		if (_AppType != EDistributedAppType_CommandLine)
			f_LogApplicationInfo();

		if (mp_State.m_bStoppingApp)
			return DMibErrorInstance("Startup aborted");
		
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
					, [this, _Params]() -> TCContinuation<void>
					{
						return fp_Initialize(_Params);
					}
					, false
				)
			;
		}				
			
		TCContinuation<NStr::CStr> Continuation;
		g_Dispatch > [this, _Params]()
			{
				TCContinuation<void> Continuation;
				(*mp_pInitOnce)() > Continuation % "Failed to initialize" / [this, Continuation, _Params]()
					{
						if (mp_State.m_bStoppingApp)
							return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Running specific application startup");
						fp_StartApp(_Params) > Continuation % "Failed to start app" / [this, Continuation]
							{
								if (mp_State.m_bStoppingApp)
									return Continuation.f_SetException(DMibErrorInstance("Startup aborted"));

								DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific application startup finished");
								fp_PublishCommandLine() > Continuation % "Failed to publish command line" / [Continuation] 
									{
										Continuation.f_SetResult();
									}
								;
							}
						;
					}
				 ;
				return Continuation;
			}
			> [this, Continuation](TCAsyncResult<void> &&_Result)
			{
				mp_AppStartupResult = _Result;
				
				for (auto &StartupResult : mp_DeferredAppStartupResults)
					StartupResult.f_SetResult(_Result);
				
				mp_DeferredAppStartupResults.f_Clear();
				
				if (!_Result)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Error, "{}", _Result.f_GetExceptionStr());
					Continuation.f_SetException(fg_Move(_Result));
					return;
				}
				DMibLogWithCategory(Mib/Concurrency/App, Info, "App startup finished");
				Continuation.f_SetResult(mp_State.m_HostID);
			}
		;
		
		return Continuation;
	}

	void CDistributedAppActor::fp_Construct()
	{
		mp_State.m_AppActor = fg_ThisActor(this);
		mp_pCommandLineSpec = fg_Construct();
		fp_BuildCommandLine(*mp_pCommandLineSpec);
	}

	TCContinuation<void> CDistributedAppActor::f_StopApp()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Info, "App shutting down");
		TCContinuation<void> Continuation;

		mp_State.m_bStoppingApp = true;
		
		g_Dispatch > [this]() -> TCContinuation<void>
			{
				if (mp_CommandLine)
				{
					TCContinuation<void> Continuation;
					mp_CommandLine->f_Destroy() > Continuation;
					mp_CommandLine = nullptr;
					return Continuation;
				}
				mp_pStdInCleanup.f_Clear();
				return fg_Explicit();
			}
			> Continuation % "Failed to stop command line interface" / [this, Continuation]
			{
				fp_StopApp() > Continuation % "Failed to stop app" / [Continuation]
					{
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific app successfully stopped");
						
						Continuation.f_SetResult();
					}
				;
			}
		;
		
		return Continuation;
	}
	
	TCContinuation<void> CDistributedAppActor::fp_Destroy()
	{
		TCActorResultVector<void> Destroys;
		if (mp_Settings.m_bSeparateDistributionManager && mp_State.m_DistributionManager)
			mp_State.m_DistributionManager->f_Destroy() > Destroys.f_AddResult();
		if (mp_State.m_TrustManager)
			mp_State.m_TrustManager->f_Destroy() > Destroys.f_AddResult();
	
		TCContinuation<void> Continuation;
		Destroys.f_GetResults() > Continuation.f_ReceiveAny(); 
		return Continuation;
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
				auto OldEnclave = SuggestedEnclave;
				SuggestedEnclave = NCryptography::fg_RandomID();
				auto Cleanup = g_OnScopeExit > [&]
					{
						SuggestedEnclave = OldEnclave; 
					}
				;
				AppActor = _fActorFactory();
			}
			bool bInstalledLogDispatcher = false;
			auto CleanupLogDispatcher = g_OnScopeExit > [&]
				{
					if (bInstalledLogDispatcher)
						fg_GetSys()->f_GetLogger().f_SetDispatcher(nullptr);
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
					[&](NEncoding::CEJSON const &_Params, bool _bForceStart)
					{
						EDistributedAppType AppType = EDistributedAppType_Unchanged;
						if (_bForceStart)
							AppType = EDistributedAppType_ForceLocal;
						else if (_bStartApp)
							AppType = EDistributedAppType_Local;
						else
							AppType = EDistributedAppType_CommandLine;

						TCActor<CActor> LogActor = fg_ApplyLoggingOption(_Params);
						if (LogActor)
							bInstalledLogDispatcher = true;
						if (_bStartApp || _bForceStart)
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
}
