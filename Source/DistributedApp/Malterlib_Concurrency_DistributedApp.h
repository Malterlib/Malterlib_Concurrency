// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJSONDatabase>
#include <Mib/Concurrency/ActorCallOnce>
#include <Mib/Concurrency/DistributedAppInterface>

#include "Malterlib_Concurrency_DistributedApp_SettingsProperties.h"
#include "Malterlib_Concurrency_DistributedApp_ThreadLocal.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLineClient.h"
#include "Malterlib_Concurrency_DistributedApp_Settings.h"
#include "Malterlib_Concurrency_DistributedApp_Auditor.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppActor;
	struct CDistributedAppInterfaceClient;

	struct CDistributedAppState
	{
		NEncoding::CSimpleJSONDatabase m_StateDatabase;
		NEncoding::CSimpleJSONDatabase m_ConfigDatabase;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCDistributedActor<CDistributedAppInterfaceServer> m_AppInterfaceServer;
		TCActor<CActor> m_LogActor;
		NHTTP::CURL m_LocalAddress;
		TCWeakActor<CDistributedAppActor> m_AppActor;
		NStr::CStr m_HostID;
		NStr::CStr m_CommandLineHostID;
		NStr::CStr m_RootDirectory;
		bool m_bStoppingApp = false;

		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

		virtual TCContinuation<void> f_SaveStateDatabase() = 0;
		virtual TCContinuation<void> f_SaveConfigDatabase() = 0;

		CDistributedAppState() = delete;
		~CDistributedAppState();
		
		CDistributedAppState(CDistributedAppState const &);
		CDistributedAppState(CDistributedAppState &&);
		
		CDistributedAppState &operator = (CDistributedAppState const &);
		CDistributedAppState &operator = (CDistributedAppState &&);
		
	private:
		friend struct CDistributedAppActor;
		
		CDistributedAppState(CDistributedAppActor_Settings const &_Settings);
	};

	struct CDistributedAppActor : public CActor
	{
		struct CCommandLine : public ICCommandLine
		{
			CCommandLine(TCWeakActor<CDistributedAppActor> const &_Actor);
			
			TCContinuation<uint32> f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params, CCommandLineControl &&_CommandLine) override;

		private:
			TCWeakActor<CDistributedAppActor> mp_Actor; 
		};
		
		CDistributedAppActor(CDistributedAppActor_Settings const &_Settings);
		~CDistributedAppActor();

		TCContinuation<NStr::CStr> f_StartApp(NEncoding::CEJSON const &_Params, TCActor<CActor> const &_LogActor, EDistributedAppType _AppType);
		TCContinuation<void> f_StopApp();

		void f_SetAppType(EDistributedAppType _AppType);
		void f_LogApplicationInfo();

		TCContinuation<CDistributedAppCommandLineClient> f_GetCommandLineClient(); 

		TCContinuation<uint32> f_RunCommandLine
			(
				 CCallingHostInfo const &_CallingHost
				 , NStr::CStr const &_Command
				 , NEncoding::CEJSON const &_Params
				 , NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			)
		;

		uint32 f_CommandLine_RemoveAllTrust(bool _bConfirm);
		
		TCContinuation<uint32> f_CommandLine_AddConnection
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_Ticket
				 , bool _bIncludeFriendlyHostName
				 , int32 _ConnectionConcurrency
			)
		;
		TCContinuation<uint32> f_CommandLine_GenerateTrustTicket(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_ForListen);

		TCContinuation<uint32> f_CommandLine_ListTrustedHosts(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName);
		TCContinuation<uint32> f_CommandLine_RemoveTrustedHost(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID);
		
		TCContinuation<uint32> f_CommandLine_GetHostID(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		 
		TCContinuation<uint32> f_CommandLine_GetConnetionStatus(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		 
		TCContinuation<uint32> f_CommandLine_ListConnections(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName);
		TCContinuation<uint32> f_CommandLine_RemoveConnection(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_AddAdditionalConnection
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_URL
				 , bool _bIncludeFriendlyHostName
				 , int32 _ConnectionConcurrency
			)
		;
		TCContinuation<uint32> f_CommandLine_SetConnectionConcurrency(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL, int32 _ConnectionConcurrency);
		 
		TCContinuation<uint32> f_CommandLine_AddListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_RemoveListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_SetPrimaryListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_ListListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCContinuation<uint32> f_CommandLine_ListNamespaces(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTrustedHosts);
		TCContinuation<uint32> f_CommandLine_TrustHostForNamespace(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_Namespace, NStr::CStr const &_Host);
		TCContinuation<uint32> f_CommandLine_UntrustHostForNamespace(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_Namespace, NStr::CStr const &_Host);

		TCContinuation<uint32> f_CommandLine_ListHostPermissions(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeHosts);
		TCContinuation<uint32> f_CommandLine_AddHostPermission(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID, NStr::CStr const &_Permission);
		TCContinuation<uint32> f_CommandLine_RemoveHostPermission(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID, NStr::CStr const &_Permission);

		TCContinuation<uint32> f_CommandLine_ListUsers(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		TCContinuation<uint32> f_CommandLine_AddUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCContinuation<uint32> f_CommandLine_RemoveUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);
		TCContinuation<uint32> f_CommandLine_SetUserInfo(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCContinuation<uint32> f_CommandLine_RemoveMetadata(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, NStr::CStr const &_Key);

		void f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo);
		
		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

#if DMibConfig_Tests_Enable
		TCContinuation<NEncoding::CEJSON> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif

	protected:
		virtual TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) = 0;
		virtual TCContinuation<void> fp_StopApp() = 0;
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine); 
		virtual TCContinuation<void> fp_PreRunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		virtual void fp_PopulateAppInterfaceRegisterInfo(CDistributedAppInterfaceServer::CRegisterInfo &o_RegisterInfo, NEncoding::CEJSON const &_Params);

		virtual TCContinuation<void> fp_PreUpdate();
		virtual TCContinuation<void> fp_PreStop(); /// Stop generating new data here to make backups consistent
		virtual TCContinuation<CActorSubscription> fp_StartBackup
			(
				TCDistributedActorInterface<CDistributedAppInterfaceBackup> &&_BackupInterface
				, CActorSubscription &&_ManifestFinished
				, NStr::CStr const &_BackupRoot
			)
		;

#if DMibConfig_Tests_Enable
		virtual TCContinuation<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif

		void fp_Construct() override;
		TCContinuation<void> fp_Destroy() override;
		
		TCContinuation<void> fp_SaveStateDatabase();
		TCContinuation<void> fp_SaveConfigDatabase();
		
		CCallingHostInfoScope fp_PopulateCurrentHostInfoIfMissing(NStr::CStr const &_Description);

		struct CLocalAppState : public CDistributedAppState
		{
			CLocalAppState(CDistributedAppActor_Settings const &_Settings, CDistributedAppActor &_AppActor);

			TCContinuation<void> f_SaveStateDatabase() override;
			TCContinuation<void> f_SaveConfigDatabase() override;

		private:
			CDistributedAppActor &mp_AppActor;
		};
		
		CLocalAppState mp_State;
		CDistributedAppActor_Settings mp_Settings;
		
	private:
		struct CDistributedAppInterfaceClientImplementation;
		
		TCContinuation<void> fp_Initialize(NEncoding::CEJSON const &_Params);
		void fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		void fp_CleanupOldExecutables();
#endif
		TCContinuation<void> fp_SetupListen();
		TCContinuation<void> fp_SetupAppServerInterface(NEncoding::CEJSON const &_Params);
		TCContinuation<void> fp_SubscribeAppServerInterface(NEncoding::CEJSON const &_Params);
		TCContinuation<CDistributedActorTrustManager::CTrustTicket> fp_GetTicketThroughStdIn(NStr::CStr const &_RequestMagic);
		
		TCContinuation<void> fp_CreateCommandLineTrust();
		TCContinuation<void> fp_SetupCommandLineListen();
		TCContinuation<void> fp_SetupCommandLineTrust();
		
		TCContinuation<uint32> fp_RunCommandLineAndLogError
			(
				NStr::CStr const &_Description
				, NFunction::TCFunction<TCContinuation<uint32> ()> &&_fCommand
			)
		;
		
		TCContinuation<void> fp_PublishCommandLine();
		
		bool fp_HasCommandLineAccess(NStr::CStr const &_HostID);
		
		NHTTP::CURL fp_GetLocalAddress() const;
		NStr::CStr fp_GetLocalHostname(bool _bEnclaveSpecific) const;
		NContainer::TCMap<NStr::CStr, NStr::CStr> fp_GetTranslateHostnames() const;

		TCActor<ICDistributedActorTrustManagerDatabase> mp_TrustManagerDatabase;
		TCActor<CSeparateThreadActor> mp_FileOperationsActor;
		TCActor<CSeparateThreadActor> mp_CleanupFilesActor;
		TCDistributedActor<CCommandLine> mp_CommandLine;
		CDistributedActorPublication mp_CommandLinePublication;
		CDistributedActorTrustManager_Address mp_PrimaryListen;
		NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> mp_pCommandLineSpec;
		NPtr::TCUniquePointer<TCActorCallOnce<void>> mp_pInitOnce; 
		COnScopeExitShared mp_pStdInCleanup;
		
		TCTrustedActorSubscription<CDistributedAppInterfaceServer> mp_AppInteraceServerSubscription;
		TCDelegatedActorInterface<CDistributedAppInterfaceClientImplementation> mp_AppInterfaceClientImplementation;
		TCDistributedActor<CDistributedActorTrustManagerInterface> mp_AppInterfaceClientTrustProxy;
		CActorSubscription mp_AppInterfaceClientRegistrationSubscription;
		TCAsyncResult<void> mp_AppStartupResult;
		NContainer::TCVector<TCContinuation<void>> mp_DeferredAppStartupResults;

		NStr::CStr mp_CurrentLogDirectory;
		EDistributedAppType mp_AppType = EDistributedAppType_Unknown;
		
		bool mp_bDelegateTrustToAppInterface = false;
	};

	TCActor<CActor> fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params);
	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
			, bool _bStartApp = true 
		)
	;
}

#include "Malterlib_Concurrency_DistributedApp.hpp"
