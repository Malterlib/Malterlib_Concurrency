// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Concurrency/ActorSubscription>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

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
			 , CEJSONSorted const &_Params
			 , CCommandLineControl &&_CommandLine
		)
	{
		co_return co_await mp_Actor
			(
				&CDistributedAppActor::f_RunCommandLine
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

		bool bAlreadySetup = false;
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			bAlreadySetup = co_await
				(
					g_Dispatch(BlockingActorCheckout) / [CommandLineTrustPath, ExpectedAddress]()
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
							CEJSONSorted const ClientConnectionData = CEJSONSorted::fs_FromString(CFile::fs_ReadStringFromFile(ClientConnectionPath), ClientConnectionPath);

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
		}

		if (bAlreadySetup && mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
			co_return {};

		TCSharedPointer<TCAtomic<bool>> pSuccessful = fg_Construct(false);

		// Remove trust database if anything went wrong during setup
		auto CleanupTrustDatabase = g_BlockingActorSubscription / [CommandLineTrustPath, pSuccessful]
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
		Options.m_ReconnectDelay = mp_Settings.m_ReconnectDelay;

		TCActor<CDistributedActorTrustManager> TrustManager = fg_ConstructActor<CDistributedActorTrustManager>(fg_Move(TrustManagerDatabase), fg_Move(Options));
		TrustManagerDatabase.f_Clear();

		auto CleanupTrustManager = g_ActorSubscription / [TrustManager]() mutable -> TCFuture<void>
			{
				co_await fg_Move(TrustManager).f_Destroy();
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
		CDistributedActorTrustManager_Address LocalListenAddress;
		LocalListenAddress.m_URL = fp_GetLocalAddress();

		if (co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, LocalListenAddress))
			co_return {};

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, LocalListenAddress);

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_PublishCommandLine()
	{
		auto &Internal = *mp_pInternal;

		Internal.m_CommandLine = mp_State.m_DistributionManager->f_ConstructActor<CCommandLine>(fg_ThisActor(this));
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Publishing command line actor");

		Internal.m_CommandLinePublication = co_await Internal.m_CommandLine->f_Publish<ICCommandLine>("com.malterlib/Concurrency/Commandline", 0.0);
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Command line published");

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_SetupCommandLineTrust()
	{
		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Setting up command line trust");

		co_await self(&CDistributedAppActor::fp_SetupCommandLineListen);
		co_await self(&CDistributedAppActor::fp_CreateCommandLineTrust);

		if (auto pCommandLineHost = mp_State.m_StateDatabase.m_Data.f_GetMember("CommandLineHostID", EJSONType_String))
			mp_State.m_CommandLineHostID = pCommandLineHost->f_String();

		DMibLogWithCategory(Mib/Concurrency/App, Debug, "Finished setting up command line trust");

		co_return {};
	}

	bool CDistributedAppActor::fp_HasCommandLineAccess(CStr const &_HostID)
	{
		return _HostID == mp_State.m_CommandLineHostID;
	}
}
