// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJSONDatabase>
#include <Mib/Concurrency/ActorCallOnce>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Concurrency/DistributedActorAuthentication>

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
		NWeb::NHTTP::CURL m_LocalAddress;
		TCWeakActor<CDistributedAppActor> m_AppActor;
		NStr::CStr m_HostID;
		NStr::CStr m_CommandLineHostID;
		NStr::CStr m_RootDirectory;
		bool m_bStoppingApp = false;

		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

		virtual TCContinuation<void> f_SaveStateDatabase();
		virtual TCContinuation<void> f_SaveConfigDatabase();

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
				, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			)
		;

		uint32 f_CommandLine_RemoveAllTrust(bool _bConfirm);

		TCContinuation<uint32> f_CommandLine_AddConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Ticket
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
			 	, NContainer::TCSet<NStr::CStr> &&_TrustedNamespaces
			)
		;
		TCContinuation<uint32> f_CommandLine_GenerateTrustTicket
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_ForListen
			 	, NContainer::TCSet<NStr::CStr> const &_Permissions
				, NStr::CStr const &_UserID
				, NEncoding::CEJSON const &_AuthenticationFactors
			)
		;

		TCContinuation<uint32> f_CommandLine_ListTrustedHosts(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName);
		TCContinuation<uint32> f_CommandLine_RemoveTrustedHost(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID);

		TCContinuation<uint32> f_CommandLine_GetHostID(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCContinuation<uint32> f_CommandLine_GetConnetionStatus(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCContinuation<uint32> f_CommandLine_ListConnections(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName);
		TCContinuation<uint32> f_CommandLine_RemoveConnection(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_AddAdditionalConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_URL
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
			)
		;
		TCContinuation<uint32> f_CommandLine_SetConnectionConcurrency
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_URL
			 	, int32 _ConnectionConcurrency
			)
		;

		TCContinuation<uint32> f_CommandLine_AddListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_RemoveListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_SetPrimaryListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCContinuation<uint32> f_CommandLine_ListListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCContinuation<uint32> f_CommandLine_ListNamespaces(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTrustedHosts);
		TCContinuation<uint32> f_CommandLine_TrustHostForNamespace
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_Namespace
			 	, NStr::CStr const &_Host
			)
		;
		TCContinuation<uint32> f_CommandLine_UntrustHostForNamespace
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_Namespace
			 	, NStr::CStr const &_Host
			)
		;

		TCContinuation<uint32> f_CommandLine_ListPermissions(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeHosts);
		TCContinuation<uint32> f_CommandLine_AddPermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_HostID
				, NStr::CStr const &_UserID
				, NStr::CStr const &_Permission
				, NEncoding::CEJSON const &_AuthenticationFactors
				, int64 _AuthenticationLifetime
			)
		;
		TCContinuation<uint32> f_CommandLine_RemovePermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_HostID
				, NStr::CStr const &_UserID
				, NStr::CStr const &_Permission
			)
		;

		TCContinuation<uint32> f_CommandLine_ListUsers(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		TCContinuation<uint32> f_CommandLine_AddUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCContinuation<uint32> f_CommandLine_RemoveUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);
		TCContinuation<uint32> f_CommandLine_SetUserInfo(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCContinuation<uint32> f_CommandLine_RemoveMetadata(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, NStr::CStr const &_Key);
		TCContinuation<uint32> f_CommandLine_ExportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, bool _bIncludePrivate);
		TCContinuation<uint32> f_CommandLine_ImportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserData);
		TCContinuation<uint32> f_CommandLine_GetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		TCContinuation<uint32> f_CommandLine_SetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);

		TCContinuation<uint32> f_CommandLine_RegisterAuthenticationFactor
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_Factor
			 	, bool _bQuiet
			)
		;
		TCContinuation<uint32> f_CommandLine_UnregisterAuthenticationFactor
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_Factor
			)
		;
		TCContinuation<uint32> f_CommandLine_EnumUserAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);
		TCContinuation<uint32> f_CommandLine_EnumAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		TCContinuation<uint32> f_CommandLine_AuthenticatePermissionPattern(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);

		void f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo);

		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

#if DMibConfig_Tests_Enable
		TCContinuation<NEncoding::CEJSON> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif

		static bool fs_ColorEnabledDefault();

	protected:
		virtual TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) = 0;
		virtual TCContinuation<void> fp_StopApp() = 0;

		void fp_BuildDefaultCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine, EDefaultCommandLineFunctionality _Functionalities);
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine);

		virtual TCContinuation<void> fp_PreRunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
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

		// To enable authentication override fp_SetupAuthentication and call fp_EnableAuthentication
        virtual TCContinuation<CActorSubscription> fp_SetupAuthentication
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, NStr::CStr const &_UserID
			)
		;
        TCContinuation<CActorSubscription> fp_EnableAuthentication
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, NStr::CStr const &_UserID
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

		NWeb::NHTTP::CURL fp_GetLocalAddress() const;
		NStr::CStr fp_GetLocalHostname(bool _bEnclaveSpecific) const;
		NContainer::TCMap<NStr::CStr, NStr::CStr> fp_GetTranslateHostnames() const;
		void fp_MakeActive();

		TCActor<ICDistributedActorTrustManagerDatabase> mp_TrustManagerDatabase;
		TCActor<CSeparateThreadActor> mp_FileOperationsActor;
		TCActor<CSeparateThreadActor> mp_CleanupFilesActor;
		TCDistributedActor<CCommandLine> mp_CommandLine;
		CDistributedActorPublication mp_CommandLinePublication;
		CDistributedActorTrustManager_Address mp_PrimaryListen;
		NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> mp_pCommandLineSpec;
		NStorage::TCUniquePointer<TCActorCallOnce<void>> mp_pInitOnce;
		COnScopeExitShared mp_pStdInCleanup;

		TCTrustedActorSubscription<CDistributedAppInterfaceServer> mp_AppInteraceServerSubscription;
		TCDelegatedActorInterface<CDistributedAppInterfaceClientImplementation> mp_AppInterfaceClientImplementation;
		TCDistributedActor<CDistributedActorTrustManagerInterface> mp_AppInterfaceClientTrustProxy;
		CActorSubscription mp_AppInterfaceClientRegistrationSubscription;
		TCAsyncResult<void> mp_AppStartupResult;
		NContainer::TCVector<TCContinuation<void>> mp_DeferredAppStartupResults;

		NContainer::TCMap<NStr::CStr, CActorSubscription> mp_TicketPermissionSubscriptions;

		NStr::CStr mp_CurrentLogDirectory;
		EDistributedAppType mp_AppType = EDistributedAppType_Unknown;

		bool mp_bDelegateTrustToAppInterface = false;
	protected:
		struct CRegisteredAuthentication
		{
			CTrustedActorInfo m_ActorInfo;
			CActorSubscription m_Subscription;
		};

		TCTrustedActorSubscription<ICDistributedActorAuthentication> mp_AuthenticationRemotes;
		TCDistributedActor<ICDistributedActorAuthenticationHandler> mp_AuthenticationHandlerImplementation;
		NContainer::TCMap<TCWeakDistributedActor<ICDistributedActorAuthentication>, CRegisteredAuthentication> mp_AuthenticationRegistrationSubscriptions;
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
