// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Concurrency/ActorSubscription>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NAtomic;

	CDistributedAppActor::CCommandLine::CCommandLine(TCWeakActor<CDistributedAppActor> const &_Actor)
		: mp_Actor(_Actor)
	{
	}

	TCFuture<uint32> CDistributedAppActor::CCommandLine::f_RunCommandLine
		(
			 CStr const &_Command
			 , CEJSON const &_Params
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

		auto ExpectedAddress = fp_GetLocalAddress();

		bool bAlreadySetup = co_await
			(
				g_Dispatch(mp_FileOperationsActor) / [CommandLineTrustPath, ExpectedAddress]()
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
		;

		if (bAlreadySetup && mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
			co_return {};

		TCSharedPointer<TCAtomic<bool>> pSuccessful = fg_Construct(false);

		// Remove trust database if anything went wrong during setup
		auto CleanupTrustDatabase = g_ActorSubscription(mp_FileOperationsActor) / [CommandLineTrustPath, pSuccessful]
			{
				if (pSuccessful->f_Load())
					return;

				if (CFile::fs_FileExists(CommandLineTrustPath))
					CFile::fs_DeleteDirectoryRecursive(CommandLineTrustPath);
			}
		;

		TCActor<ICDistributedActorTrustManagerDatabase> TrustManagerDatabase = fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>(CommandLineTrustPath);

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

		TCActor<CDistributedActorTrustManager> TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(fg_Move(TrustManagerDatabase), fg_Move(Options));
		TrustManagerDatabase.f_Clear();

		auto CleanupTrustManager = g_ActorSubscription / [TrustManager]() -> TCFuture<void>
			{
				co_await TrustManager->f_Destroy();
				co_return {};
			}
		;

		CStr HostID = co_await TrustManager(&CDistributedActorTrustManager::f_Initialize);

		if (co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClient, HostID))
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, HostID);

		CDistributedActorTrustManager_Address LocalListenAddress;
		LocalListenAddress.m_URL = fp_GetLocalAddress();

		auto TrustTicket = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, LocalListenAddress, nullptr, nullptr);

		co_await TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, 60.0, -1);
		*pSuccessful = true;

		auto &Setting = mp_State.m_StateDatabase.m_Data["CommandLineHostID"];
		if (!Setting.f_IsString() || Setting.f_String() != HostID)
		{
			mp_State.m_StateDatabase.m_Data["CommandLineHostID"] = HostID;
			co_await (mp_State.m_StateDatabase.f_Save() % "Failed to save state database");
		}

		co_await CleanupTrustManager->f_Destroy();

		co_return {};
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
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Publishing command line actor");

		mp_CommandLine->f_Publish<ICCommandLine>("com.malterlib/Concurrency/Commandline")
			> Promise / [this, Promise](CDistributedActorPublication &&_Publication)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Debug, "Command line published");
				mp_CommandLinePublication = fg_Move(_Publication);
				Promise.f_SetResult();
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_SetupCommandLineTrust()
	{
		TCPromise<void> Promise;
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Setting up command line trust");

		fg_ThisActor(this)(&CDistributedAppActor::fp_SetupCommandLineListen) > Promise / [this, Promise]()
			{
				fg_ThisActor(this)(&CDistributedAppActor::fp_CreateCommandLineTrust) > Promise / [this, Promise]
					{
						if (auto pCommandLineHost = mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
							mp_State.m_CommandLineHostID = pCommandLineHost->f_String();

						DMibLogWithCategory(Mib/Concurrency/App, Debug, "Finished setting up command line trust");
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
