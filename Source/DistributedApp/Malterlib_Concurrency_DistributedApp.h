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
		NFunction::TCFunctionMovable<CDistributedAppAuditor (CCallingHostInfo const &_CallingHostInfo)> f_AuditorFactory() const;

		virtual TCFuture<void> f_SaveStateDatabase();
		virtual TCFuture<void> f_SaveConfigDatabase();

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

			TCFuture<uint32> f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params, CCommandLineControl &&_CommandLine) override;

		private:
			TCWeakActor<CDistributedAppActor> mp_Actor;
		};

		CDistributedAppActor(CDistributedAppActor_Settings const &_Settings);
		~CDistributedAppActor();

		TCFuture<NStr::CStr> f_StartApp(NEncoding::CEJSON const &_Params, TCActor<CActor> const &_LogActor, EDistributedAppType _AppType);
		TCFuture<void> f_StopApp();

		void f_SetAppType(EDistributedAppType _AppType);
		void f_LogApplicationInfo();

		TCFuture<CDistributedAppCommandLineClient> f_GetCommandLineClient();

		TCFuture<uint32> f_RunCommandLine
			(
				CCallingHostInfo const &_CallingHost
				, NStr::CStr const &_Command
				, NEncoding::CEJSON const &_Params
				, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			)
		;

		uint32 f_CommandLine_RemoveAllTrust(bool _bConfirm);

		TCFuture<uint32> f_CommandLine_AddConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Ticket
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
			 	, NContainer::TCSet<NStr::CStr> &&_TrustedNamespaces
			)
		;
		TCFuture<uint32> f_CommandLine_GenerateTrustTicket
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_ForListen
			 	, NContainer::TCSet<NStr::CStr> const &_Permissions
				, NStr::CStr const &_UserID
				, NEncoding::CEJSON const &_AuthenticationFactors
			)
		;

		TCFuture<uint32> f_CommandLine_ListTrustedHosts(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_RemoveTrustedHost(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID);

		TCFuture<uint32> f_CommandLine_GetHostID(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCFuture<uint32> f_CommandLine_GetConnetionStatus(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);

		TCFuture<uint32> f_CommandLine_ListConnections(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_RemoveConnection(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCFuture<uint32> f_CommandLine_AddAdditionalConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_URL
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
			)
		;
		TCFuture<uint32> f_CommandLine_SetConnectionConcurrency
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_URL
			 	, int32 _ConnectionConcurrency
			)
		;

		TCFuture<uint32> f_CommandLine_AddListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCFuture<uint32> f_CommandLine_RemoveListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCFuture<uint32> f_CommandLine_SetPrimaryListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL);
		TCFuture<uint32> f_CommandLine_ListListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);

		TCFuture<uint32> f_CommandLine_ListNamespaces(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTrustedHosts, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_TrustHostForNamespace
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_Namespace
			 	, NStr::CStr const &_Host
			)
		;
		TCFuture<uint32> f_CommandLine_UntrustHostForNamespace
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_Namespace
			 	, NStr::CStr const &_Host
			)
		;

		TCFuture<uint32> f_CommandLine_ListPermissions(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTargets, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_AddPermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_HostID
				, NStr::CStr const &_UserID
				, NStr::CStr const &_Permission
				, NEncoding::CEJSON const &_AuthenticationFactors
				, int64 _AuthenticationLifetime
			)
		;
		TCFuture<uint32> f_CommandLine_RemovePermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_HostID
				, NStr::CStr const &_UserID
				, NStr::CStr const &_Permission
			)
		;

		TCFuture<uint32> f_CommandLine_ListUsers(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_AddUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCFuture<uint32> f_CommandLine_RemoveUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);
		TCFuture<uint32> f_CommandLine_SetUserInfo(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);
		TCFuture<uint32> f_CommandLine_RemoveMetadata(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, NStr::CStr const &_Key);
		TCFuture<uint32> f_CommandLine_ExportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, bool _bIncludePrivate);
		TCFuture<uint32> f_CommandLine_ImportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserData);
		TCFuture<uint32> f_CommandLine_GetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		TCFuture<uint32> f_CommandLine_SetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);

		TCFuture<uint32> f_CommandLine_RegisterAuthenticationFactor
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_Factor
			 	, bool _bQuiet
			)
		;
		TCFuture<uint32> f_CommandLine_UnregisterAuthenticationFactor
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_Factor
			)
		;
		TCFuture<uint32> f_CommandLine_EnumUserAuthenticationFactors
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_EnumAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_AuthenticatePermissionPattern(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSON const &_Params);

		void f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo);

		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

#if DMibConfig_Tests_Enable
		TCFuture<NEncoding::CEJSON> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif

		static NCommandLine::EAnsiEncodingFlag fs_ColorAnsiFlagsDefault();
		static bool fs_ColorEnabledDefault();
		static bool fs_Color24BitEnabledDefault();
		static bool fs_ColorLightBackgroundDefault();
		static bool fs_BoxDrawingDefault();

	protected:
		virtual TCFuture<void> fp_StartApp(NEncoding::CEJSON const &_Params) = 0;
		virtual TCFuture<void> fp_StopApp() = 0;

		void fp_BuildDefaultCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine, EDefaultCommandLineFunctionality _Functionalities);
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine);

		virtual TCFuture<void> fp_PreRunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		virtual void fp_PopulateAppInterfaceRegisterInfo(CDistributedAppInterfaceServer::CRegisterInfo &o_RegisterInfo, NEncoding::CEJSON const &_Params);

		virtual TCFuture<void> fp_PreUpdate();
		virtual TCFuture<void> fp_PreStop(); /// Stop generating new data here to make backups consistent
		virtual TCFuture<CActorSubscription> fp_StartBackup
			(
				TCDistributedActorInterface<CDistributedAppInterfaceBackup> &&_BackupInterface
				, CActorSubscription &&_ManifestFinished
				, NStr::CStr const &_BackupRoot
			)
		;

		// To enable authentication override fp_SetupAuthentication and call fp_EnableAuthentication
        virtual TCFuture<CActorSubscription> fp_SetupAuthentication
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, NStr::CStr const &_UserID
			)
		;
        TCFuture<CActorSubscription> fp_EnableAuthentication
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
			 	, int64 _AuthenticationLifetime
			 	, NStr::CStr _UserID
			)
		;

#if DMibConfig_Tests_Enable
		virtual TCFuture<NEncoding::CEJSON> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif

		void fp_Construct() override;
		TCFuture<void> fp_Destroy() override;

		TCFuture<void> fp_SaveStateDatabase();
		TCFuture<void> fp_SaveConfigDatabase();

		CCallingHostInfoScope fp_PopulateCurrentHostInfoIfMissing(NStr::CStr const &_Description);

		struct CLocalAppState : public CDistributedAppState
		{
			CLocalAppState(CDistributedAppActor_Settings const &_Settings, CDistributedAppActor &_AppActor);

			TCFuture<void> f_SaveStateDatabase() override;
			TCFuture<void> f_SaveConfigDatabase() override;

		private:
			CDistributedAppActor &mp_AppActor;
		};

		CLocalAppState mp_State;
		CDistributedAppActor_Settings mp_Settings;

	private:
		struct CDistributedAppInterfaceClientImplementation;

		TCFuture<void> fp_Initialize(NEncoding::CEJSON const &_Params);
		void fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		void fp_CleanupOldExecutables();
#endif

		TCFuture<void> fp_SetupListen();
		TCFuture<void> fp_SetupAppServerInterface(NEncoding::CEJSON _Params);
		TCFuture<void> fp_SubscribeAppServerInterface(NEncoding::CEJSON _Params);
		TCFuture<CDistributedActorTrustManager::CTrustTicket> fp_GetTicketThroughStdIn(NStr::CStr _RequestMagic);

		TCFuture<void> fp_CreateCommandLineTrust();
		TCFuture<void> fp_SetupCommandLineListen();
		TCFuture<void> fp_SetupCommandLineTrust();

		TCFuture<uint32> fp_RunCommandLineAndLogError
			(
				NStr::CStr const &_Description
				, NFunction::TCFunctionMovable<TCFuture<uint32> ()> &&_fCommand
			)
		;

		TCFuture<void> fp_PublishCommandLine();

		bool fp_HasCommandLineAccess(NStr::CStr const &_HostID);

		NWeb::NHTTP::CURL fp_GetLocalAddress() const;
		NStr::CStr fp_GetLocalHostname(bool _bEnclaveSpecific) const;
		NContainer::TCMap<NStr::CStr, NStr::CStr> fp_GetTranslateHostnames() const;
		void fp_MakeActive();
#if DMibEnableSafeCheck > 0
		void fp_CheckDestroy() override;
#endif

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
		TCDistributedActorInstance<CDistributedAppInterfaceClientImplementation> mp_AppInterfaceClientImplementation;
		TCDistributedActor<CDistributedActorTrustManagerInterface> mp_AppInterfaceClientTrustProxy;
		CActorSubscription mp_AppInterfaceClientRegistrationSubscription;
		TCAsyncResult<void> mp_AppStartupResult;
		NContainer::TCVector<TCPromise<void>> mp_DeferredAppStartupResults;

		NContainer::TCMap<NStr::CStr, CActorSubscription> mp_TicketPermissionSubscriptions;

		NStr::CStr mp_CurrentLogDirectory;
		EDistributedAppType mp_AppType = EDistributedAppType_Unknown;

		bool mp_bDelegateTrustToAppInterface = false;
#if DMibEnableSafeCheck > 0
		bool mp_bDestroyCalled = false;
#endif
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
