// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Process/Platform>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Cryptography/UUID>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Concurrency/DistributedAppInterface>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NPtr;
	using namespace NNet;
	using namespace NDataProcessing;

	namespace
	{
		CUniversallyUniqueIdentifier g_HostnameRootUUID("8ED61926-AC8A-4793-92C5-DE05547999E7", EUniversallyUniqueIdentifierFormat_Bare);
	}

	CDistributedAppActor_Settings::CDistributedAppActor_Settings
		(
			NStr::CStr const &_AppName
			, bool _bRequireListen
			, NStr::CStr const &_ConfigDirectory
			, bool _bSeparateConcurrencyManager
			, NNet::CSSLKeySetting _KeySetting
			, NStr::CStr const &_FriendlyName
			, NStr::CStr const &_Enclave
			, EDistributedAppUpdateType _UpdateType
			, NStr::CStr const &_AuditCategory
		)
		: m_AppName(_AppName)
		, m_bRequireListen(_bRequireListen)
		, m_ConfigDirectory(_ConfigDirectory)
		, m_bSeparateConcurrencyManager(_bSeparateConcurrencyManager)
		, m_KeySetting(_KeySetting)
		, m_FriendlyName(_FriendlyName)
		, m_Enclave(_Enclave)
		, m_UpdateType(_UpdateType)
		, m_AuditCategory(_AuditCategory)
	{
		if (m_AuditCategory.f_IsEmpty())
			m_AuditCategory = _AppName;
#ifndef DPlatformFamily_Windows
		// A longer app name and enclave results in a unix socket name becoming too long 
		DMibRequire
			(
				fp_GetLocalSocketPath(fg_Format("/tmp/{}", g_HostnameRootUUID.f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum)), true).f_GetLen() 
				<= aint(NSys::NNet::fg_GetMaxUnixSocketNameLength())
			)
		;
#endif
	}

	NStr::CStr CDistributedAppActor_Settings::fp_GetLocalSocketPath(CStr const &_Prefix, bool _bEnclaveSpecific) const
	{
		CStr SocketName;
		if (!m_Enclave.f_IsEmpty() && _bEnclaveSpecific)
			SocketName = fg_Format("{}.{}.socket", m_AppName, m_Enclave);
		else
			SocketName = fg_Format("{}.socket", m_AppName);
		return fg_Format("{}/{}", _Prefix, SocketName);
	}

	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketHostname(bool _bEnclaveSpecific) const
	{
#ifdef DPlatformFamily_Windows
		NDataProcessing::CHash_SHA256 Hash;
		CStr Path = fp_GetLocalSocketPath(m_ConfigDirectory, _bEnclaveSpecific);
		Hash.f_AddData(Path.f_GetStr(), Path.f_GetLen());
		uint16 Port = 0;
		NMem::fg_MemCopy(&Port, Hash.f_GetDigest().f_GetData(), sizeof(Port));
		if (Port == 0)
			++Port;

		return fg_Format("localhost:{}", Port);
#else
		mint MaxLength = NSys::NNet::fg_GetMaxUnixSocketNameLength();
		if (fp_GetLocalSocketPath(m_ConfigDirectory, true).f_GetLen() <= aint(MaxLength))
			return fg_Format("UNIX(777):{}", fp_GetLocalSocketPath(m_ConfigDirectory, _bEnclaveSpecific));
		
		CStr ConfigHash = fg_GetHashedUuidString(m_ConfigDirectory, g_HostnameRootUUID, EUniversallyUniqueIdentifierFormat_AlphaNum);
		CStr TempDir = CFile::fs_GetTemporaryDirectory();
		CStr Prefix = fg_Format("{}/{}", TempDir, ConfigHash);
		if (fp_GetLocalSocketPath(Prefix, true).f_GetLen() <= aint(MaxLength))
			return fg_Format("UNIX(777):{}", fp_GetLocalSocketPath(Prefix, _bEnclaveSpecific));
		
		Prefix = fg_Format("/tmp/{}", ConfigHash);
		DMibCheck(fp_GetLocalSocketPath(Prefix, true).f_GetLen() <= aint(MaxLength));
		return fg_Format("UNIX(777):{}", fp_GetLocalSocketPath(Prefix, _bEnclaveSpecific));
#endif
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_ConfigDirectory(CStr const &_ConfigDirectory) &&
	{
		m_ConfigDirectory = _ConfigDirectory;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_SeparateConcurrencyManager(bool _bSeparateConcurrencyManager) &&
	{
		m_bSeparateConcurrencyManager = _bSeparateConcurrencyManager;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_KeySetting(NNet::CSSLKeySetting _KeySetting) &&
	{
		m_KeySetting = _KeySetting;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_FriendlyName(CStr const &_FriendlyName) &&
	{
		m_FriendlyName = _FriendlyName;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_Enclave(CStr const &_Enclave) &&
	{
		m_Enclave = _Enclave;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_AuditCategory(NStr::CStr const &_Category) &&
	{
		m_AuditCategory = _Category;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_UpdateType(EDistributedAppUpdateType _UpdateType) &&
	{
		m_UpdateType = _UpdateType;
		return fg_Move(*this);
	}
	
	NStr::CStr CDistributedAppActor_Settings::f_GetCompositeFriendlyName() const
	{
		CStr FriendlyName = m_FriendlyName;
		if (FriendlyName.f_IsEmpty())
			FriendlyName = fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName()); 
		return fg_Format("{}/{}", FriendlyName, m_AppName);
	}

	CDistributedAppState::~CDistributedAppState() = default;
	
	CDistributedAppState::CDistributedAppState(CDistributedAppActor_Settings const &_Settings)
		: m_StateDatabase(fg_Format("{}/{}State.json", _Settings.m_ConfigDirectory, _Settings.m_AppName))
		, m_ConfigDatabase(fg_Format("{}/{}Config.json", _Settings.m_ConfigDirectory, _Settings.m_AppName))
	{
	}
	
	CDistributedAppState::CDistributedAppState(CDistributedAppState const &) = default;
	CDistributedAppState::CDistributedAppState(CDistributedAppState &&) = default;
	
	CDistributedAppState &CDistributedAppState::operator = (CDistributedAppState const &) = default;
	CDistributedAppState &CDistributedAppState::operator = (CDistributedAppState &&) = default;
	
	CDistributedAppActor::CDistributedAppActor(CDistributedAppActor_Settings const &_Settings)
		: mp_State(_Settings)
		, mp_Settings(_Settings)
	{
		mp_State.m_LocalAddress = fp_GetLocalAddress();
		fg_GetSys()->f_SetDefaultLogFileName(fg_Format("{}.log", _Settings.m_AppName));
		fg_GetSys()->f_SetDefaultLogFileDirectory(_Settings.m_ConfigDirectory + "/Log");
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
#ifdef DPlatformFamily_Windows
		return NHTTP::CURL{fg_Format("wss://{}/", fp_GetLocalHostname(false))};
#else
		return NHTTP::CURL{fg_Format("wss://[{}]/", fp_GetLocalHostname(false))};
#endif
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
				ChangesResults.f_GetResults() > [this, Continuation](TCAsyncResult<TCVector<TCAsyncResult<void>>> &&_Results)
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

	TCContinuation<void> CDistributedAppActor::fp_Initialize(NEncoding::CEJSON const &_Params)
	{
		TCContinuation<void> Continuation;
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Loading config file and state");
		
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
				
				if (mp_Settings.m_bSeparateConcurrencyManager)
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
				
				mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>
					(
						mp_TrustManagerDatabase
						, fg_Move(fManagerFactor)
						, mp_Settings.m_KeySetting
						, mp_Settings.m_ListenFlags
						, mp_Settings.f_GetCompositeFriendlyName()
						, mp_Settings.m_Enclave
						, fp_GetTranslateHostnames()
						, DefaultConcurrency
					)
				;
				
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
									if (mp_Settings.m_bSeparateConcurrencyManager)
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
											> Continuation % "Failed to setup commmand line trust" / [this, Continuation]()
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

	TCContinuation<void> CDistributedAppActor::f_StartApp(NEncoding::CEJSON const &_Params)
	{
		if (mp_State.m_bStoppingApp)
			return DMibErrorInstance("Startup aborted");
		if (!mp_pInitOnce)
		{
			mp_FileOperationsActor = fg_ConstructActor<CSeparateThreadActor>(fg_Construct("Distributed app file access"));
			mp_TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>
				(
					mp_Settings.m_ConfigDirectory + fg_Format("/TrustDatabase.{}", mp_Settings.m_AppName)
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
			
		TCContinuation<void> Continuation;
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
				Continuation.f_SetResult();
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
		if (mp_Settings.m_bSeparateConcurrencyManager && mp_State.m_DistributionManager)
			mp_State.m_DistributionManager->f_Destroy() > Destroys.f_AddResult();
		if (mp_State.m_TrustManager)
			mp_State.m_TrustManager->f_Destroy() > Destroys.f_AddResult();
	
		TCContinuation<void> Continuation;
		Destroys.f_GetResults() > Continuation.f_ReceiveAny(); 
		return Continuation;
	}
	
	bool fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params)
	{
		bool bInstalledLogDispatcher = false;
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
				auto LoggerActor = NMib::NConcurrency::fg_ConstructActor<NMib::NConcurrency::CSeparateThreadActor>(fg_Construct("Log Dispatcher"));
				bInstalledLogDispatcher = true;		
				fg_GetSys()->f_GetLogger().f_SetDispatcher
					(
						[LoggerActor](NFunction::TCFunctionMovable<void ()> &&_fToDispatch)
						{
							fg_Dispatch
								(
									LoggerActor
									, fg_Move(_fToDispatch)
								)
								> fg_DiscardResult() 
							;
						}
					)
				;
			}
		}
		
		return bInstalledLogDispatcher;
	}

	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine
			, bool _bStartApp
		)
	{
		try
		{
			TCActor<CDistributedAppActor> AppActor;
			{
				auto &SuggestedEnclave = fg_DistributedActorSuggestedEnclave();
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
			
			CommandLineClient.f_SetLazyStartApp
				(
					[&](NEncoding::CEJSON const &_Params, bool _bForceStart)
					{
						if (fg_ApplyLoggingOption(_Params))
							bInstalledLogDispatcher = true;
						if (_bStartApp || _bForceStart)
							AppActor(&CDistributedAppActor::f_StartApp, _Params).f_CallSync();
					}
				)
			;
			aint Ret = CommandLineClient.f_RunCommandLine(fg_GetSys()->f_GetCommandLineArgs());
			
			AppActor->f_BlockDestroy();
			
			return Ret;
		}
		catch (NException::CException const &_Exception)
		{
			DMibConErrOut("{}{\n}", _Exception.f_GetErrorStr());
			return 1;
		}
		
		return 0;
	}
}
