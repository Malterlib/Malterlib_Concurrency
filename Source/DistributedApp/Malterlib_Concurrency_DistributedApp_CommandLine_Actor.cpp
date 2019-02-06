// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;

	CDistributedAppActor::CCommandLine::CCommandLine(TCWeakActor<CDistributedAppActor> const &_Actor)
		: mp_Actor(_Actor)
	{
	}

	TCFuture<uint32> CDistributedAppActor::CCommandLine::f_RunCommandLine
		(
			 NStr::CStr const &_Command
			 , NEncoding::CEJSON const &_Params
			 , CCommandLineControl &&_CommandLine
		)
	{
		return mp_Actor
			(
				&CDistributedAppActor::f_RunCommandLine
				, fg_GetCallingHostInfo()
				, _Command
				, _Params
				, NStorage::TCSharedPointer<CCommandLineControl>{fg_Construct(fg_Move(_CommandLine))}
			)
		;
	}

	TCFuture<void> CDistributedAppActor::fp_CreateCommandLineTrust()
	{
		CStr CommandLineTrustPath = fg_Format("{}/CommandLineTrustDatabase.{}", mp_Settings.m_RootDirectory, mp_Settings.m_AppName);

		TCPromise<void> Promise;

		auto ExpectedAddress = fp_GetLocalAddress();

		fg_Dispatch
			(
				mp_FileOperationsActor
				, [Promise, CommandLineTrustPath, ExpectedAddress]()
				{
					if (!CFile::fs_FileExists(CommandLineTrustPath))
						return false;

					auto ClientConnections = CFile::fs_FindFiles(CommandLineTrustPath + "/ClientConnections/*.json");
					if (ClientConnections.f_IsEmpty())
						return false;

					if (ClientConnections.f_GetLen() > 1)
					{
						for (auto &File : ClientConnections)
							CFile::fs_DeleteFile(File);
						return false;
					}

					CStr ClientConnectionPath = ClientConnections.f_GetFirst();

					try
					{
						CEJSON const ClientConnectionData = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(ClientConnectionPath), ClientConnectionPath);

						CStr CurrentAddress = ClientConnectionData["Address"].f_String();

						if (CurrentAddress == ExpectedAddress.f_Encode())
							return true;
					}
					catch (NException::CException const &)
					{
					}

					CFile::fs_DeleteFile(ClientConnectionPath);

					return false;
				}
			)
			> Promise / [this, Promise, CommandLineTrustPath](bool _bAlreadySetup)
			{
				if (_bAlreadySetup && mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
				{
					// Already setup
					Promise.f_SetResult();
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

					CDistributedActorTrustManager::COptions Options;

					Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings)
						{
							return fg_ConstructActor<NConcurrency::CActorDistributionManager>(_Settings);
						}
					;
					Options.m_KeySetting = mp_Settings.m_KeySetting;
					Options.m_ListenFlags = mp_Settings.m_ListenFlags;
					Options.m_FriendlyName = mp_Settings.f_GetCompositeFriendlyName();
					Options.m_Enclave = CStr();
					Options.m_TranslateHostnames = fp_GetTranslateHostnames();
					Options.m_DefaultConnectionConcurrency = 1;

					State.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(State.m_TrustManagerDatabase, fg_Move(Options));

					State.m_TrustManager(&CDistributedActorTrustManager::f_Initialize) > Promise / [this, Promise, pState, pCleanup](CStr const &_HostID)
						{
							mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClient, _HostID) > Promise / [this, Promise, pState, _HostID, pCleanup](bool _bHasClient)
								{
									auto fContinue = [this, Promise, pState, _HostID, pCleanup]
										{
											CDistributedActorTrustManager_Address LocalListenAddress;
											LocalListenAddress.m_URL = fp_GetLocalAddress();

											mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, LocalListenAddress, nullptr, nullptr)
												> Promise / [this, Promise, pState, _HostID, pCleanup]
												(CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult &&_TrustTicket)
												{
													auto &State = *pState;
													State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, _TrustTicket.m_Ticket, 60.0, -1)
														> Promise / [this, Promise, _HostID, pCleanup, pState]
														{
															pCleanup->f_Clear();
															auto &Setting = mp_State.m_StateDatabase.m_Data["CommandLineHostID"];
															if (!Setting.f_IsString() || Setting.f_String() != _HostID)
															{
																mp_State.m_StateDatabase.m_Data["CommandLineHostID"] = _HostID;
																mp_State.m_StateDatabase.f_Save() > Promise % "Failed to save state database";
															}
															else
																Promise.f_SetResult();
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
										mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID) > Promise / [fContinue = fg_Move(fContinue)]()
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
		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_SetupCommandLineListen()
	{
		TCPromise<void> Promise;

		CDistributedActorTrustManager_Address LocalListenAddress;
		LocalListenAddress.m_URL = fp_GetLocalAddress();

		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, LocalListenAddress) > Promise / [this, Promise, LocalListenAddress](bool _bHasListen)
			{
				if (_bHasListen)
				{
					Promise.f_SetResult();
					return;
				}
				mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, LocalListenAddress) > Promise / [Promise]()
					{
						Promise.f_SetResult();
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_PublishCommandLine()
	{
		TCPromise<void> Promise;
		mp_CommandLine = mp_State.m_DistributionManager->f_ConstructActor<CCommandLine>(fg_ThisActor(this));
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Publishing command line actor");

		mp_CommandLine->f_Publish<ICCommandLine>("com.malterlib/Concurrency/Commandline")
			> Promise / [this, Promise](CDistributedActorPublication &&_Publication)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Command line published");
				mp_CommandLinePublication = fg_Move(_Publication);
				Promise.f_SetResult();
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_SetupCommandLineTrust()
	{
		TCPromise<void> Promise;
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Setting up command line trust");

		fg_ThisActor(this)(&CDistributedAppActor::fp_SetupCommandLineListen) > Promise / [this, Promise]()
			{
				fg_ThisActor(this)(&CDistributedAppActor::fp_CreateCommandLineTrust) > Promise / [this, Promise]
					{
						if (auto pCommandLineHost = mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
							mp_State.m_CommandLineHostID = pCommandLineHost->f_String();

						DMibLogWithCategory(Mib/Concurrency/App, Info, "Finished setting up command line trust");
						Promise.f_SetResult();
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	bool CDistributedAppActor::fp_HasCommandLineAccess(CStr const &_HostID)
	{
		return _HostID == mp_State.m_CommandLineHostID;
	}
}
