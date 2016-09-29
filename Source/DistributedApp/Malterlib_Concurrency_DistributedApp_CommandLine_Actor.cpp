// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib
{
	using namespace NStr;
	using namespace NFile;
	using namespace NPtr;
	using namespace NEncoding;
	using namespace NContainer;
	
	namespace NConcurrency
	{
		CDistributedAppActor::CCommandLine::CCommandLine(TCWeakActor<CDistributedAppActor> const &_Actor)
			: mp_Actor(_Actor)
		{
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::CCommandLine::f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
		{
			return mp_Actor(&CDistributedAppActor::f_RunCommandLine, fg_GetCallingHostInfo(), _Command, _Params);
		}
		
		TCContinuation<void> CDistributedAppActor::fp_CreateCommandLineTrust()
		{
			CStr CommandLineTrustPath = fg_Format("{}/CommandLineTrustDatabase.{}", mp_Settings.m_ConfigDirectory, mp_Settings.m_AppName);
			
			TCContinuation<void> Continuation;
			
			fg_Dispatch
				(
					mp_FileOperationsActor
					, [this, Continuation, CommandLineTrustPath]
					{
						if (!CFile::fs_FileExists(CommandLineTrustPath))
							return false;
						return true;
					}
				)
				> Continuation / [this, Continuation, CommandLineTrustPath](bool _bAlreadySetUp)
				{
					if (_bAlreadySetUp && mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
					{
						// Already setup
						Continuation.f_SetResult();
						return;
					}

					// Remove trust database if anything went wrong during setup
					auto pCleanup = fg_OnScopeExitShared
						(
							[CommandLineTrustPath, FileOperationsActor = mp_FileOperationsActor]
							{
								fg_Dispatch
									(
										FileOperationsActor
										, [CommandLineTrustPath]
										{
											if (CFile::fs_FileExists(CommandLineTrustPath))
												CFile::fs_DeleteDirectoryRecursive(CommandLineTrustPath);
										}
									)
									> fg_DiscardResult()
								;
							}
						)
					;
					
					{
						struct CState
						{
							TCActor<ICDistributedActorTrustManagerDatabase> m_TrustManagerDatabase;
							TCActor<CDistributedActorTrustManager> m_TrustManager;
						};
						
						TCSharedPointer<CState> pState = fg_Construct();
						auto &State = *pState;
						State.m_TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(CommandLineTrustPath);
						State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>
							(
								State.m_TrustManagerDatabase
								, [](CActorDistributionManagerInitSettings const &_Settings)
								{
									return fg_ConstructActor<NConcurrency::CActorDistributionManager>(_Settings);
								}
								, mp_Settings.m_KeySetting
								, mp_Settings.m_ListenFlags
								, mp_Settings.f_GetCompositeFriendlyName()
								, CStr() 
								, fp_GetTranslateHostnames() 
							)
						;
						State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize) > Continuation / [this, Continuation, pState, pCleanup](CStr const &_HostID)
							{
								mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClient, _HostID) > Continuation / [this, Continuation, pState, _HostID, pCleanup](bool _bHasClient)
									{
										auto fContinue = [this, Continuation, pState, _HostID, pCleanup]
											{
												CDistributedActorTrustManager_Address LocalListenAddress;
												LocalListenAddress.m_URL = fp_GetLocalAddress();
										
												mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, LocalListenAddress) 
													> Continuation / [this, Continuation, pState, _HostID, pCleanup](CDistributedActorTrustManager::CTrustTicket &&_TrustTicket)
													{
														auto &State = *pState;
														State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, _TrustTicket, 60.0) 
															> Continuation / [this, Continuation, _HostID, pCleanup, pState]
															{
																pCleanup->f_Clear();
																auto &Setting = mp_State.m_StateDatabase.m_Data["CommandLineHostID"];
																if (!Setting.f_IsString() || Setting.f_String() != _HostID)
																{
																	mp_State.m_StateDatabase.m_Data["CommandLineHostID"] = _HostID;
																	mp_State.m_StateDatabase.f_Save() > Continuation % "Failed to save state database"; 
																}
																else
																	Continuation.f_SetResult();
															}
														;
													}
												;
											}
										;
										
										if (!_bHasClient)
											fContinue();
										else
										{
											mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID) > Continuation / [fContinue = fg_Move(fContinue)]()
												{
													fContinue();
												}
											;
										}
									}
								;
							}
						;
					}
				}
			;				
			return Continuation;
		}

		TCContinuation<void> CDistributedAppActor::fp_SetupCommandLineListen()
		{
			TCContinuation<void> Continuation;
			
			CDistributedActorTrustManager_Address LocalListenAddress;
			LocalListenAddress.m_URL = fp_GetLocalAddress();
			
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, LocalListenAddress) > Continuation / [this, Continuation, LocalListenAddress](bool _bHasListen)
				{
					if (_bHasListen)
					{
						Continuation.f_SetResult();
						return;
					}
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, LocalListenAddress) > Continuation / [this, Continuation]()
						{
							Continuation.f_SetResult();
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<void> CDistributedAppActor::fp_PublishCommandLine()
		{
			TCContinuation<void> Continuation;
			mp_CommandLine = fg_ConstructDistributedActor<CCommandLine>(fg_ThisActor(this));
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Publishing command line actor");
			
			mp_State.m_DistributionManager
				(
					&CActorDistributionManager::f_PublishActor
					, mp_CommandLine
					, "com.malterlib/Concurrency/Commandline"
					, CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy<ICCommandLine>()
				)
				> Continuation / [this, Continuation](CDistributedActorPublication &&_Publication)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Command line published");
					mp_CommandLinePublication = fg_Move(_Publication);
					Continuation.f_SetResult();
				}
			;
			
			return Continuation;
		}

		TCContinuation<void> CDistributedAppActor::fp_SetupCommandLineTrust()
		{
			TCContinuation<void> Continuation;
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Setting up command line trust");
			
			fg_ThisActor(this)(&CDistributedAppActor::fp_SetupCommandLineListen) > Continuation / [this, Continuation]()
				{
					fg_ThisActor(this)(&CDistributedAppActor::fp_CreateCommandLineTrust) > Continuation / [Continuation]
						{
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Finished setting up command line trust");
							Continuation.f_SetResult();
						}
					;
				}
			;
			
			return Continuation;
		}

		bool CDistributedAppActor::fp_HasCommandLineAccess(CStr const &_HostID)
		{
			if (auto pCommandLineHost = mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
				return pCommandLineHost->f_String() == _HostID;
			return false;
		}
	}		
}
