// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Process/Platform>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JsonDirectory>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Concurrency/LogError>
#include <Mib/Network/Socket>
#include <Mib/Log/AnsiLogger>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_Private.h"
#include "Malterlib_Concurrency_DistributedApp_LogDestination.h"
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

	CFutureCoroutineContext::COnResumeScopeAwaiter CDistributedAppActor::fp_CheckStoppedOrDestroyedOnResume() const
	{
		return fg_OnResume
			(
				[this]() -> NException::CExceptionPointer
				{
					if (f_IsDestroyed() || mp_State.m_bStoppingApp)
						return DMibImpExceptionInstance(CExceptionActorIsBeingDestroyed, "Shutting down");

					return {};
				}
			)
		;
	}

	TCFuture<void> CDistributedAppActor::fp_AuditToDistributedLogger(CDistributedAppAuditParams _AuditParams)
	{
		auto &Internal = *mp_pInternal;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> NException::CExceptionPointer
				{
					if (Internal.m_bAuditLogsDestroyed)
						return DMibErrorInstance("Shutting down");
					return {};
				}
			)
		;

		auto pCanDestroy = Internal.m_pCanDestroyAuditLogs;

		if (!Internal.m_AuditLogReporter)
		{
			auto InitSequenceSubscription = co_await Internal.m_AuditLogReporterInitSequencer.f_Sequence();

			if (!Internal.m_AuditLogReporter)
				Internal.m_AuditLogReporter = co_await f_OpenDefaultLogReporter();
		}

		DMibCheck(Internal.m_AuditLogReporter);
		DMibCheck(Internal.m_AuditLogReporter->m_fReportEntries);

		NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> LogEntries;

		auto &LogEntry = LogEntries.f_Insert();
		auto &Data = LogEntry.m_Data;
		Data.f_SetFromLogSeverity(_AuditParams.m_Severity);
		Data.m_Flags |= CDistributedAppLogReporter::ELogEntryFlag_Audit;
		Data.m_Message = fg_Move(_AuditParams.m_Message);

		if (_AuditParams.m_Category)
			Data.m_Categories.f_Insert(fg_Move(_AuditParams.m_Category));

		Data.m_Metadata["ClaimedUserID"] = _AuditParams.m_CallingHostInfo.f_GetClaimedUserID();
		Data.m_Metadata["ClaimedUserName"] = _AuditParams.m_CallingHostInfo.f_GetClaimedUserName();
		Data.m_Metadata["RealHostID"] = _AuditParams.m_CallingHostInfo.f_GetRealHostID();
		Data.m_Metadata["FriendlyHostName"] = _AuditParams.m_CallingHostInfo.f_GetHostInfo().m_FriendlyName;
		Data.m_Metadata["UniqueHostID"] = _AuditParams.m_CallingHostInfo.f_GetUniqueHostID();

		if (auto pAccessDenied = _AuditParams.m_ExtraData.f_TryGet<EDistributedAppAuditType_AccessDenied>())
		{
			Data.m_Metadata["Type"] = "AccessDenied";
			auto &AccessDenied = Data.m_Metadata["AccessDenied"].f_Array();
			for (auto &PermissionSet : pAccessDenied->m_Permissions)
			{
				TCVector<CEJsonSorted> Array;
				for (auto &Permission : PermissionSet)
					Array.f_Insert(Permission);

				AccessDenied.f_Insert(CEJsonSorted(fg_Move(Array)));
			}
		}
		else
			Data.m_Metadata["Type"] = "Normal";

		if (!Internal.m_AuditLogReporter->m_fReportEntries)
			co_return DMibErrorInstance("Invalid report functor");

		co_await Internal.m_AuditLogReporter->m_fReportEntries(fg_Move(LogEntries));

		co_return {};
	}

	void CDistributedAppActor::fs_LogAudit(CDistributedAppAuditParams const &_AuditParams, CStr const &_DefaultCategory)
	{
#if (DMibSysLogSeverities) != 0
		CStr Category = _AuditParams.m_Category;
		if (Category.f_IsEmpty())
			Category = _DefaultCategory;
		
		NMib::NLog::CSysLogCatScope Scope(NMib::fg_GetSys()->f_GetLogger(), Category);
		CStr UserID = _AuditParams.m_CallingHostInfo.f_GetClaimedUserID();
		CStr UserName = _AuditParams.m_CallingHostInfo.f_GetClaimedUserName();
		CStr Message = _AuditParams.m_Message;

		if (auto *pAccessDenied = _AuditParams.m_ExtraData.f_TryGet<EDistributedAppAuditType_AccessDenied>(); pAccessDenied && !pAccessDenied->m_Permissions.f_IsEmpty())
		{
			TCVector<CStr> PermissionSets;
			for (auto &Set : pAccessDenied->m_Permissions)
				PermissionSets.f_Insert("({})"_f << CStr::fs_Join(Set, " or "));

			if (PermissionSets.f_GetLen() == 1)
				Message += " {}"_f << PermissionSets[0];
			else
				Message += " ({})"_f << CStr::fs_Join(PermissionSets, " and ");
		}

		auto &HostInfo = _AuditParams.m_CallingHostInfo.f_GetHostInfo();

		if (UserName || UserID)
			NMib::NLog::fg_SysLog(DLogLocTag, _AuditParams.m_Severity, "<{}, {} [{}]> {}", HostInfo, UserID, UserName, Message);
		else if (!HostInfo.f_IsEmpty())
			NMib::NLog::fg_SysLog(DLogLocTag, _AuditParams.m_Severity, "<{}> {}", HostInfo, Message);
		else
			NMib::NLog::fg_SysLog(DLogLocTag, _AuditParams.m_Severity, "{}", Message);
#endif
	}

	void CDistributedAppActor::f_Audit(CDistributedAppAuditParams &&_AuditParams)
	{
		auto &Internal = *mp_pInternal;

		{
			DMibLogOperation(DisableDistributedLogReporter); // We already report audit logs separately
			CDistributedAppActor::fs_LogAudit(_AuditParams, mp_Settings.m_AuditCategory);
		}

		switch (Internal.m_AppType)
		{
		case EDistributedAppType_InProcess:
		case EDistributedAppType_Daemon:
		case EDistributedAppType_Local:
		case EDistributedAppType_ForceLocal:
			{
				fp_AuditToDistributedLogger(fg_Move(_AuditParams)) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
						{
							DMibLogOperation(DisableDistributedLogReporter); // Prevent generating another error
							DMibLogWithCategory(Mib/Concurrency/App, Error, "Failed to log audit message: {}", _Result.f_GetExceptionStr());
						}
					}
				;
			}
			break;
		case EDistributedAppType_CommandLine:
		case EDistributedAppType_DirectCommandLine:
			break;
		case EDistributedAppType_Unknown:
		case EDistributedAppType_Unchanged:
			DMibNeverGetHere;
			break;
		}
	}

	CCallingHostInfoScope CDistributedAppActor::fp_PopulateCurrentHostInfoIfMissing(CStr _Description)
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

		TCSet<CDistributedActorTrustManager_Address> WantedListens;

		// Deduce primary listen from old config and remove the config
		auto const *pListen = mp_State.m_ConfigDatabase.m_Data.f_GetMember("Listen", EJsonType_Array);
		if (!pListen)
		{
			if (mp_Settings.m_bCanUseLocalListenAsPrimary)
			{
				auto PrimaryListen = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetPrimaryListen);
				if (!PrimaryListen)
				{
					CDistributedActorTrustManager_Address LocalListen;
					LocalListen.m_URL = fp_GetLocalAddress();
					(void)co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetPrimaryListen, LocalListen).f_Wrap();
				}
			}

			co_return {};
		}

		auto PrimaryListen = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetPrimaryListen);
		if (!PrimaryListen)
		{
			CDistributedActorTrustManager_Address LocalListen;
			LocalListen.m_URL = fp_GetLocalAddress();

			for (auto &Object : pListen->f_Array())
			{
				if (!Object.f_IsObject())
					continue;

				CDistributedActorTrustManager_Address Address;
				if (auto pAddress = Object.f_GetMember("Address"))
				{
					if (pAddress->f_Type() != EJsonType_String)
						continue;
					if (!Address.m_URL.f_Decode(pAddress->f_String()))
						continue;
					if (Address.m_URL.f_GetScheme() != "wss")
						continue;
				}
				else
					continue;

				if (Address == LocalListen && !mp_Settings.m_bCanUseLocalListenAsPrimary)
					continue;

				co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetPrimaryListen, Address);
				break;
			}
		}

		mp_State.m_ConfigDatabase.m_Data.f_RemoveMember("Listen");
		co_await mp_State.m_ConfigDatabase.f_Save();

		co_return {};
	}

	void CDistributedAppActor::fp_CleanupEnclaveSockets()
	{
		if (mp_Settings.m_Enclave.f_IsEmpty())
			return;

		auto BlockingActorCheckout = fg_BlockingActor();
		auto BlockingActor = BlockingActorCheckout.f_Actor();

		(
			g_Dispatch(BlockingActor) / [WildcardPath = mp_Settings.f_GetLocalSocketWildcard(true)]
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
		)
		.f_OnResultSet(BlockingActorCheckout.f_MoveResultHandler("Mib/Concurrency/App", "Error cleaning up enclave sockets"));
	}

#ifdef DPlatformFamily_Windows
	void CDistributedAppActor::fp_CleanupOldExecutables()
	{
		auto BlockingActorCheckout = fg_BlockingActor();
		auto BlockingActor = BlockingActorCheckout.f_Actor();

		(
			g_Dispatch(BlockingActor) / []
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
		)
		.f_OnResultSet(BlockingActorCheckout.f_MoveResultHandler("Mib/Concurrency/App", "Error cleaning up old executables"));
	}
#endif

	TCFuture<void> CDistributedAppActor::fp_InitializeDistributedTrust()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Loading config file and state");

		fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		fp_CleanupOldExecutables();
#endif
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> NException::CExceptionPointer
				{
					if (mp_State.m_bStoppingApp)
					{
						if (mp_Settings.m_bSeparateDistributionManager)
							fg_Move(mp_State.m_DistributionManager).f_Destroy().f_DiscardResult();

						return DMibErrorInstance("Startup aborted");
					}

					return {};
				}
			)
		;

		co_await (mp_State.m_StateDatabase.f_Load() + mp_State.m_ConfigDatabase.f_Load());

		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Initializing trust manager");
		NFunction::TCFunctionMovable<NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)> fManagerFactory;

		if (mp_Settings.m_bSeparateDistributionManager)
		{
			fManagerFactory = [](CActorDistributionManagerInitSettings const &_Settings)
				{
					return fg_ConstructActor<CActorDistributionManager>(_Settings);
				}
			;
		}

		int32 DefaultConcurrency = 1;
		if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("DefaultConnectionConcurrency", EJsonType_Integer))
			DefaultConcurrency = fg_Clamp(pValue->f_Integer(), 1, 128);

		fp64 InitialConnectionTimeout = 5.0;
		if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("InitialConnectionTimeout", EJsonType_Float))
			InitialConnectionTimeout = fg_Clamp(pValue->f_Float(), 0.1, 3600.0);

		bool bSupportAuthentication = mp_Settings.m_bSupportUserAuthentication;
		if (auto *pValue = mp_State.m_ConfigDatabase.m_Data.f_GetMember("SupportAuthentication", EJsonType_Boolean))
			bSupportAuthentication = pValue->f_Boolean();

		CDistributedActorTrustManager::COptions Options;

		Options.m_fConstructManager = fg_Move(fManagerFactory);
		Options.m_KeySetting = mp_Settings.m_KeySetting;
		Options.m_ListenFlags = mp_Settings.m_ListenFlags;
		Options.m_FriendlyName = mp_Settings.f_GetCompositeFriendlyName();
		Options.m_Enclave = mp_Settings.m_Enclave;
		Options.m_TranslateHostnames = fp_GetTranslateHostnames();
		Options.m_InitialConnectionTimeout = InitialConnectionTimeout;
		Options.m_bWaitForConnectionsDuringInit = mp_Settings.m_bWaitForRemotes;
		Options.m_DefaultConnectionConcurrency = DefaultConcurrency;
		Options.m_bSupportAuthentication = bSupportAuthentication;
		Options.m_bTimeoutForUnixSockets = mp_Settings.m_bTimeoutForUnixSockets;
		Options.m_ReconnectDelay = mp_Settings.m_ReconnectDelay;

		auto &Internal = *mp_pInternal;

		mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(Internal.m_TrustManagerDatabase, fg_Move(Options));

		co_await (mp_State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize) % "Failed to initialize trust manager");

		auto [DistributionManager, HostID] = co_await
			(
				(
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager)
					+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID)
				)
				% "Failed to get distribution manager"
			)
		;

		mp_State.m_DistributionManager = fg_Move(DistributionManager);
		mp_State.m_HostID = fg_Move(HostID);

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_Initialize(NEncoding::CEJsonSorted const _Params)
	{
		auto &Internal = *mp_pInternal;

		auto Result = co_await fp_InitializeDistributedTrust().f_Wrap();

		if (!Result)
			Internal.m_DistributedTrustInitResult.f_SetException(DMibErrorInstance("Failed to initialize distributed trust"_f << Result.f_GetExceptionStr()));
		else
			Internal.m_DistributedTrustInitResult.f_SetResult();

		for (auto &StartupResult : Internal.m_DeferredDistributedTrustInit)
			StartupResult.f_SetResult(Internal.m_DistributedTrustInitResult);
		Internal.m_DeferredDistributedTrustInit.f_Clear();

		if (!Result)
			co_return Result.f_GetException();

		co_await ((fp_SetupListen() + fp_SetupAppServerInterface(_Params)) % "Failed to setup listen config or app server interface");

		co_await (fp_SetupCommandLineTrust() % "Failed to setup commmand line trust");

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_WaitForDistributedTrustInitialization()
	{
		auto &Internal = *mp_pInternal;

		if (mp_State.m_TrustManager && mp_State.m_DistributionManager)
			co_return {};

		if (Internal.m_DistributedTrustInitResult.f_IsSet())
		{
			if (!Internal.m_DistributedTrustInitResult)
				co_return Internal.m_DistributedTrustInitResult.f_GetException();
			else
				co_return DMibErrorInstance("Missing trust or distribution manager");
		}

		co_await Internal.m_DeferredDistributedTrustInit.f_Insert().f_Future();

		co_return {};
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
		try
		{
			NProcess::NPlatform::fg_Process_GetVersionInfo(CFile::fs_GetProgramPathForExecutabelContents(), VersionInfo);

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
		catch ([[maybe_unused]] NException::CException const &_Exception)
		{
			DMibLogWithCategory
				(
					Mib/Concurrency/App
					, Error
					, "{} running {} in {} with pid {}. Failed to get version info: {}"
					, mp_Settings.m_AppName
					, fg_GetAppTypeName(mp_pInternal->m_AppType)
					, CFile::fs_GetFileNoExt(ProgramPath)
					, NProcess::NPlatform::fg_Process_GetCurrentUID()
					, _Exception
				)
			;
		}
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

	TCFuture<NStr::CStr> CDistributedAppActor::f_StartApp(NEncoding::CEJsonSorted const _Params, TCActor<CDistiributedAppLogActor> _LogActor, EDistributedAppType _AppType)
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
			Internal.m_TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JsonDirectory>
				(
					fg_Format("{}/TrustDatabase.{}", mp_Settings.m_RootDirectory, mp_Settings.m_AppName)
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

				if (mp_Settings.m_bCommandLineBeforeAppStart)
					co_await (fp_PublishCommandLine() % "Failed to publish command line");

				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Running specific application startup");

				co_await (fp_StartApp(_Params) % "Failed to start app");

				if (mp_State.m_bStoppingApp)
					co_return DMibErrorInstance("Startup aborted");

				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Specific application startup finished");

				if (!mp_Settings.m_bCommandLineBeforeAppStart)
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

	TCFuture<CDistributedAppLogReporter::CLogReporter> CDistributedAppActor::f_OpenDefaultLogReporter()
	{
		CDistributedAppLogReporter::CLogInfo LogInfo;

		LogInfo.m_Identifier = "org.malterlib.log.distributedapp";
		LogInfo.m_IdentifierScope = "{} ({})"_f << mp_Settings.m_AppName << mp_Settings.m_RootDirectory;
		LogInfo.m_Name = "Malterlib Distributed App";

		co_return co_await fp_OpenLogReporter(LogInfo);
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

		CLogError LogError("Mib/Concurrency/App");

		mp_State.m_bStoppingApp = true;

		if (mp_State.m_DistributionManager)
			co_await mp_State.m_DistributionManager(&CActorDistributionManager::f_PrepareShutdown, mp_Settings.m_HostTimeoutOnShutdown, mp_Settings.m_KillHostsTimeoutOnShutdown);

		co_await fp_StopApp().f_Wrap() > LogError("Failed to stop app");

		auto &Internal = *mp_pInternal;

		if (Internal.m_CommandLinePublication.f_IsValid())
			co_await Internal.m_CommandLinePublication.f_Destroy().f_Wrap() > LogError("Failed to destroy command line publication");

		if (Internal.m_CommandLine)
			co_await fg_Move(Internal.m_CommandLine).f_Destroy().f_Wrap() > LogError("Failed to stop command line interface");

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific app successfully stopped, destroying app interface");

		{
			TCFutureVector<void> Destroys;

			if (Internal.m_AppInterfaceClientTrustProxy)
				fg_Move(Internal.m_AppInterfaceClientTrustProxy).f_Destroy() > Destroys;

			if (Internal.m_AppInterfaceClientRegistrationSubscription)
				Internal.m_AppInterfaceClientRegistrationSubscription->f_Destroy() > Destroys;

			if (Internal.m_AppInterfaceClientRegistrationConfigSubscription)
				Internal.m_AppInterfaceClientRegistrationConfigSubscription->f_Destroy() > Destroys;

			Internal.m_AppInteraceServerSubscription.f_Destroy() > Destroys;
			Internal.m_AppInterfaceClientImplementation.f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Timeout(mp_Settings.m_KillHostsTimeoutOnShutdown + 10.0, "Timed out waiting for destroy of app interface").f_Wrap()
				> LogError("Failed to destroy app interface")
			;
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
		CLogError LogError("Mib/Concurrency/App");

		{
			auto Future = fg_Exchange(Internal.m_pCanDestroyAuditLogs, fg_Construct())->f_Future();
			co_await fg_Move(Future).f_Timeout(10.0, "Timeout").f_Wrap() > LogError.f_Warning("Failed to wait for destroy adit logs");
		}

		if (mp_State.m_LogActor)
			co_await mp_State.m_LogActor(&CDistiributedAppLogActor::f_StopDeferring);

		{
			TCFutureVector<void> Destroys;
			if (Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription)
				fg_Exchange(Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription, nullptr)->f_Destroy() > Destroys;

			if (Internal.m_AppSensorStoreLocalAppServerChangeSubscription)
				fg_Exchange(Internal.m_AppSensorStoreLocalAppServerChangeSubscription, nullptr)->f_Destroy() > Destroys;

			if (Internal.m_AppLogStoreLocalExtraLogInterfaceSubscription)
				fg_Exchange(Internal.m_AppLogStoreLocalExtraLogInterfaceSubscription, nullptr)->f_Destroy() > Destroys;

			if (Internal.m_AppLogStoreLocalAppServerChangeSubscription)
				fg_Exchange(Internal.m_AppLogStoreLocalAppServerChangeSubscription, nullptr)->f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to destroy sensor and log store subscriptions");
		}

		{
			TCFutureVector<void> Destroys;

			fg_Move(Internal.m_AppSensorStoreLocalAppServerChangeSequencer).f_Destroy() > Destroys;
			fg_Move(Internal.m_AppLogStoreLocalAppServerChangeSequencer).f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to abort app server change sequencers");
		}

		{
			TCFutureVector<void> Destroys;

			for (auto &Subscription : Internal.m_AppInterfaceServerChangeSubscriptions)
				fg_Move(Subscription).f_Destroy() > Destroys;
			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to destroy app interface server change subscriptions");
		}

		{
			TCFutureVector<void> Destroys;

			if (Internal.m_AppSensorStoreLocal)
				Internal.m_AppSensorStoreLocal(&CDistributedAppSensorStoreLocal::f_DestroyRemote) > Destroys;
			if (Internal.m_AppLogStoreLocal)
				Internal.m_AppLogStoreLocal(&CDistributedAppLogStoreLocal::f_DestroyRemote) > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to destroy remote for local log and sensor store");
		}

		if (mp_State.m_TrustManager)
			co_await fg_TempCopy(mp_State.m_TrustManager).f_Destroy().f_Wrap() > LogError("Failed to destroy trust manager");
		else if (mp_Settings.m_bSeparateDistributionManager && mp_State.m_DistributionManager)
			co_await fg_TempCopy(mp_State.m_DistributionManager).f_Destroy().f_Wrap() > LogError("Failed to destroy distribution manager");

		{
			auto Future = fg_Exchange(Internal.m_pCanDestroyAuditLogs, nullptr)->f_Future();
			co_await fg_Move(Future).f_Timeout(10.0, "Timeout").f_Wrap() > LogError.f_Warning("Failed to wait for destroy adit logs (after trust manager)");
		}

		if (mp_State.m_LogActor)
			co_await mp_State.m_LogActor(&CDistiributedAppLogActor::f_StopDeferring);

		Internal.m_bAuditLogsDestroyed = true;

		if (Internal.m_AuditLogReporter)
		{
			if (Internal.m_AuditLogReporter->m_fReportEntries)
				co_await fg_Move(Internal.m_AuditLogReporter->m_fReportEntries).f_Destroy().f_Wrap() > LogError("Failed to destroy audit log reporter");
			Internal.m_AuditLogReporter.f_Clear();
		}

		{
			TCFutureVector<void> Destroys;

			fg_Move(Internal.m_AuditLogReporterInitSequencer).f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to abort audit log init sequencers");
		}

		{
			TCFutureVector<void> Destroys;

			fg_Move(Internal.m_AppSensorStoreLocalInitSequencer).f_Destroy() > Destroys;
			fg_Move(Internal.m_AppLogStoreLocalInitSequencer).f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to abort audit sensor and log store sequencers");
		}

		{
			TCFutureVector<void> Destroys;

			if (Internal.m_AppSensorStoreLocal)
				fg_Move(Internal.m_AppSensorStoreLocal).f_Destroy() > Destroys;
			if (Internal.m_AppLogStoreLocal)
				fg_Move(Internal.m_AppLogStoreLocal).f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to destroy for local log and sensor store");
		}

		{
			TCFutureVector<void> Destroys;

			fg_Move(mp_State.m_ConfigDatabase).f_Destroy() > Destroys;
			fg_Move(mp_State.m_StateDatabase).f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError("Failed to destroy Json databases");
		}

		if (Internal.m_TrustManagerDatabase)
			co_await fg_Move(Internal.m_TrustManagerDatabase).f_Destroy().f_Wrap() > LogError("Failed to destroy trust manager database");

		if (Internal.m_pInitOnce)
			co_await Internal.m_pInitOnce->f_Destroy().f_Wrap() > LogError("Failed to destroy init once");

		co_return {};
	}

#if DMibEnableSafeCheck > 0
	static constinit NAtomic::TCAtomic<bool> g_bAppliedLogging{false};
#endif

	CApplyLoggingResults fg_ApplyLoggingOption(NEncoding::CEJsonSorted const &_Params, TCActor<CDistributedAppActor> const &_DistributedLoggingApp)
	{
		DMibFastCheck(!g_bAppliedLogging.f_Exchange(true));

		TCActor<CDistiributedAppLogActor> LogActor;
		CApplyLoggingResults Results;

#if (DMibEnableTrace > 0) || ((DMibSysLogSeverities) != 0)
		NLog::ESeverity LogSeverities = NLog::ESeverity_All;

		if (auto *pParam = _Params.f_GetMember("LogSeverities", EJsonType_Array))
		{
			LogSeverities = NLog::ESeverity_None;
			for (auto &SeverityName : pParam->f_Array())
				LogSeverities |= NLog::fg_LookupSeverity(SeverityName.f_String());
		}
#endif

#if DMibEnableTrace > 0 && ((DMibSysLogSeverities) != 0)
		if (auto *pParam = _Params.f_GetMember("TraceLogger", EJsonType_Boolean))
		{
			if (!pParam->f_Boolean())
				fg_GetSys()->f_RemoveTraceLogger();
			else if (fg_GetSys()->f_HasTraceLogger())
			{
				fg_GetSys()->f_RemoveTraceLogger();
				fg_GetSys()->f_GetLogger().f_PushGlobalDestination(NLog::CLogToStdErrAnsi(NCommandLine::EAnsiEncodingFlag_None, LogSeverities, true));
			}
		}
#endif
#if (DMibSysLogSeverities) != 0
		if (auto *pParam = _Params.f_GetMember("StdErrLogger", EJsonType_Boolean))
		{
			if (pParam->f_Boolean())
				fg_GetSys()->f_GetLogger().f_PushGlobalDestination(NLog::CLogToStdErrAnsi(NCommandLine::CCommandLineDefaults::fs_ParseAnsiEncodingParams(_Params), LogSeverities, false));
		}

		bool bInstalledLogDispatcher = false;
		if (auto *pParam = _Params.f_GetMember("ConcurrentLogging", EJsonType_Boolean))
		{
			if (pParam->f_Boolean())
			{
				LogActor = fg_Construct(fg_Construct(), "Log Dispatcher");
				bInstalledLogDispatcher = true;
				fg_GetSys()->f_GetLogger().f_SetDispatcher
					(
						[LogActor](NFunction::TCFunctionMovable<void ()> &&_fToDispatch)
						{
							fg_Dispatch
								(
									LogActor
									, fg_Move(_fToDispatch)
								)
								.f_DiscardResult()
							;
						}
					)
				;
			}
		}

		if (_DistributedLoggingApp)
		{
			if (!LogActor)
				LogActor = fg_Construct(fg_Construct(), "Log Actor");

			auto DestinationID = fg_GetSys()->f_GetLogger().f_PushGlobalDestination(CDistributedAppLogDestination(_DistributedLoggingApp, LogActor), false);

			Results.m_CleanupLogging = g_OnScopeExitShared / [DestinationID, bInstalledLogDispatcher]() mutable
				{
					if (bInstalledLogDispatcher)
						fg_GetSys()->f_GetLogger().f_SetDispatcher(nullptr);
					fg_GetSys()->f_GetLogger().f_RemoveGlobalDestination(DestinationID);
				}
			;
		}

		if (bInstalledLogDispatcher && !Results.m_CleanupLogging)
		{
			Results.m_CleanupLogging = g_OnScopeExitShared / []() mutable
				{
					fg_GetSys()->f_GetLogger().f_SetDispatcher(nullptr);
				}
			;
		}
#endif
		Results.m_LogActor = fg_Move(LogActor);

		return Results;
	}

	void CApplyLoggingResults::f_Destroy(CActorDestroyEventLoop const &_DestroyLoop)
	{
		if (m_CleanupLogging)
		{
			bool bDeleted = m_CleanupLogging.f_Clear();
			DMibFastCheck(bDeleted);
			if (bDeleted)
			{
#if DMibEnableSafeCheck > 0
				g_bAppliedLogging.f_Exchange(false);
#endif
				if (m_LogActor)
					m_LogActor->f_BlockDestroy(_DestroyLoop);
			}
		}
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

				auto Cleanup = g_OnScopeExit / [&]
					{
						SuggestedEnclave = OldEnclave;
						bSuggestedWaitForRemotes = bOldWaitForRemotes;
					}
				;
				m_AppActor = _fActorFactory();
			}
			CDistributedAppCommandLineClient CommandLineClient = m_AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync(m_pRunLoop);

			if (_fMutateCommandLine)
				CommandLineClient.f_MutateCommandLineSpecification(_fMutateCommandLine);

			CommandLineClient.f_SetLazyPreRunDirectCommand
				(
					[this](NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)
					{
						if (!(_Flags & EDistributedAppCommandFlag_DontApplyLogging))
							m_ApplyLoggingResults = fg_ApplyLoggingOption(_Params, nullptr);

						if (_Flags & EDistributedAppCommandFlag_RunLocalApp)
						{
							EDistributedAppType AppType = EDistributedAppType_ForceLocal;

							m_AppActor(&CDistributedAppActor::f_StartApp, _Params, m_ApplyLoggingResults.m_LogActor, AppType).f_CallSync(m_pRunLoop);
							m_bStartedApp = true;
						}

						m_AppActor(&CDistributedAppActor::f_SetAppType, EDistributedAppType_DirectCommandLine).f_CallSync(m_pRunLoop);
					}
				)
			;

			CommandLineClient.f_SetLazyStartApp
				(
					[&](NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags) -> CDistributedAppCommandLineClient::FStopApp
					{
						EDistributedAppType AppType = EDistributedAppType_Unchanged;
						if (_Flags & EDistributedAppCommandFlag_RunLocalApp)
							AppType = EDistributedAppType_ForceLocal;
						else if (_bStartApp)
							AppType = EDistributedAppType_Local;
						else
							AppType = EDistributedAppType_CommandLine;

						if (!(_Flags & EDistributedAppCommandFlag_DontApplyLogging))
							m_ApplyLoggingResults = fg_ApplyLoggingOption(_Params, nullptr);

						if (_bStartApp || (_Flags & EDistributedAppCommandFlag_RunLocalApp))
						{
							m_AppActor(&CDistributedAppActor::f_StartApp, _Params, m_ApplyLoggingResults.m_LogActor, AppType).f_CallSync(m_pRunLoop);
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
									fg_Move(m_AppActor).f_Destroy().f_CallSync(m_pRunLoop);

									return true;
								}
								return false;
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
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("Error stopping app: {}{\n}", _Exception.f_GetErrorStr());
		}

		try
		{
			if (m_AppActor)
				m_AppActor->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
		}
		catch (CExceptionActorDeleted const &)
		{
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("Error destroying app: {}{\n}", _Exception.f_GetErrorStr());
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
	TCFuture<CEJsonSorted> CDistributedAppActor::f_Test_Command(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params)
	{
		co_return co_await fp_Test_Command(fg_Move(_Command), fg_Move(fg_RemoveQualifiers(_Params)));
	}

	TCFuture<CEJsonSorted> CDistributedAppActor::fp_Test_Command(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params)
	{
		co_return {};
	}
#endif
}
