// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Process/Platform>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NPtr;
	using namespace NNet;
	
	namespace NConcurrency
	{
		CDistributedAppActor_Settings::CDistributedAppActor_Settings
			(
				NStr::CStr const &_AppName
				, bool _bRequireListen
				, NStr::CStr const &_ConfigDirectory
				, bool _bSeparateConcurrencyManager
				, uint32 _KeySize
				, NStr::CStr const &_FriendlyName
			)
			: m_AppName(_AppName)
			, m_bRequireListen(_bRequireListen)
			, m_ConfigDirectory(_ConfigDirectory)
			, m_bSeparateConcurrencyManager(_bSeparateConcurrencyManager)
			, m_KeySize(_KeySize)
			, m_FriendlyName(_FriendlyName)
		{
		}
		
		NStr::CStr CDistributedAppActor_Settings::f_GetCompositeFriendlyName() const
		{
			CStr FriendlyName = m_FriendlyName;
			if (FriendlyName.f_IsEmpty())
				FriendlyName = fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName()); 
			return fg_Format("{}/{}", FriendlyName, m_AppName);
		}

		CDistributedAppState::CDistributedAppState(CDistributedAppActor_Settings const &_Settings)
			: m_StateDatabase(fg_Format("{}/{}State.json", _Settings.m_ConfigDirectory, _Settings.m_AppName))
			, m_ConfigDatabase(fg_Format("{}/{}Config.json", _Settings.m_ConfigDirectory, _Settings.m_AppName))
		{
		}
		
		CDistributedAppActor::CDistributedAppActor(CDistributedAppActor_Settings const &_Settings)
			: mp_State(_Settings)
			, mp_Settings(_Settings)
		{
			fg_GetSys()->f_SetDefaultLogFileName(fg_Format("{}.log", _Settings.m_AppName));
			fg_GetSys()->f_SetDefaultLogFileDirectory(_Settings.m_ConfigDirectory + "/Log");
		}
		
		CDistributedAppActor::~CDistributedAppActor()
		{
		}

		NHTTP::CURL CDistributedAppActor::fp_GetLocalAddress()
		{
#ifdef DPlatformFamily_Windows
#error "Implement unix socket emulation support on Windows before attempting this"
#else
			return NHTTP::CURL{fg_Format("wss://[UNIX:{}/{}.socket]/", mp_Settings.m_ConfigDirectory, mp_Settings.m_AppName)};
#endif
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

		TCContinuation<void> CDistributedAppActor::fp_Initialize()
		{
			TCContinuation<void> Continuation;
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Loading config file and state");
			
			mp_State.m_StateDatabase.f_Load()
				+ mp_State.m_ConfigDatabase.f_Load()
				> Continuation / [this, Continuation]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Initializing trust manager");
					NFunction::TCFunction<NConcurrency::TCActor<NConcurrency::CActorDistributionManager> 
						(NFunction::CThisTag &, NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)> 
						fManagerFactor
					;
					
					if (mp_Settings.m_bSeparateConcurrencyManager)
					{
						fManagerFactor = [](NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
							{
								return fg_ConstructActor<CActorDistributionManager>(_HostID, _FriendlyName);
							}
						;
					}
					
					mp_State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>
						(
							mp_TrustManagerDatabase
							, fManagerFactor
							, mp_Settings.m_KeySize
							, mp_Settings.m_ListenFlags
							, mp_Settings.f_GetCompositeFriendlyName()
						)
					;
					
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize)
						> Continuation % "Failed to initialize trust manager" / [this, Continuation]()
						{
							mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager) 
								> Continuation % "Failed to initialize trust manager"
								/ [this, Continuation](NConcurrency::TCActor<NConcurrency::CActorDistributionManager> &&_DistributionManager)
								{
									mp_State.m_DistributionManager = fg_Move(_DistributionManager);
									fg_ThisActor(this)(&CDistributedAppActor::fp_SetupListen) 
										> Continuation % "Failed to setup listen config" / [this, Continuation]()
										{
											fg_ThisActor(this)(&CDistributedAppActor::fp_SetupCommandLineTrust) 
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
						fg_ThisActor(this)
						, [this]() -> TCContinuation<void>
						{
							return fp_Initialize();
						}
					)
				;
			}				
				
			TCContinuation<void> Continuation;
			fg_Dispatch
				(
					[this, _Params]()
					{
						TCContinuation<void> Continuation;
						(*mp_pInitOnce)() > Continuation % "Failed to initialize" / [this, Continuation, _Params]()
							{
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Running specific application startup");
								fg_ThisActor(this)(&CDistributedAppActor::fp_StartApp, _Params) > Continuation % "Failed to start app" / [this, Continuation]
									{
										DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific application startup finished");
										fg_ThisActor(this)(&CDistributedAppActor::fp_PublishCommandLine) > Continuation % "Failed to publish command line" / [Continuation] 
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
				)
				> [this, Continuation](TCAsyncResult<void> &&_Result)
				{
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

		void CDistributedAppActor::f_Construct()
		{
			mp_pCommandLineSpec = fg_Construct();
			fp_BuildCommandLine(*mp_pCommandLineSpec);
		}

		TCContinuation<void> CDistributedAppActor::f_StopApp()
		{
			DMibLogWithCategory(Mib/Concurrency/App, Info, "App shutting down");
			TCContinuation<void> Continuation;

			fg_Dispatch
				(
					[this]() -> TCContinuation<void>
					{
						if (mp_CommandLine)
						{
							TCContinuation<void> Continuation;
							mp_CommandLine->f_Destroy
								(
									[this, Continuation](TCAsyncResult<void> &&_Result)
									{
										Continuation.f_SetResult(fg_Move(_Result));
									}
								)
							;
							mp_CommandLine = nullptr;
							return Continuation;
						}
						return fg_Explicit();
					}
				)
				> Continuation % "Failed to stop command line interface" / [this, Continuation]
				{
					fg_ThisActor(this)(&CDistributedAppActor::fp_StopApp) > Continuation % "Failed to stop app" / [Continuation]
						{
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Specific app successfully stopped");
							
							Continuation.f_SetResult();
						}
					;
				}
			;
			
			return Continuation;
		}
		
		TCContinuation<void> CDistributedAppActor::f_Destroy()
		{
			TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();
			
			if (mp_State.m_TrustManager)
			{
				mp_State.m_TrustManager->f_Destroy
					(
						[this, pCanDestroy](TCAsyncResult<void> &&)
						{
						}
					)
				;
			}
			
			return pCanDestroy->m_Continuation;
		}
		
		void fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params)
		{
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
			if (auto *pParam = _Params.f_GetMember("ConcurrentLoging", EJSONType_Boolean))
			{
				if (pParam->f_Boolean())
				{
					auto LoggerActor = NMib::NConcurrency::fg_ConstructActor<NMib::NConcurrency::CSeparateThreadActor>(fg_Construct("Log Dispatcher"));
					
					fg_GetSys()->f_GetLogger().f_SetDispatcher
						(
							[LoggerActor](NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fToDispatch)
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
				auto AppActor = _fActorFactory();
				CDistributedAppCommandLineClient CommandLineClient = AppActor(&CDistributedAppActor::f_GetCommandLineClient).f_CallSync();
				
				if (_fMutateCommandLine)
					CommandLineClient.f_MutateCommandLineSpecification(_fMutateCommandLine);
				
				CommandLineClient.f_SetLazyStartApp
					(
						[&](NEncoding::CEJSON const &_Params)
						{
							fg_ApplyLoggingOption(_Params);
							if (_bStartApp)
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
}
