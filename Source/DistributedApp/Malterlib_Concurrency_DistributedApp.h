// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJsonDatabase>
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
	struct CDistributedAppSensorReader_SensorFilter;
	struct CDistributedAppSensorReader_SensorReadingFilter;
	struct CDistributedAppSensorReader_SensorStatusFilter;
	struct CDistributedAppSensorReader_SensorKeyAndReading;
	struct CDistributedAppSensorStoreLocal;

	struct CDistributedAppLogReader_LogFilter;
	struct CDistributedAppLogReader_LogEntryFilter;
	struct CDistributedAppLogReader_LogDataFilter;
	struct CDistributedAppLogReader_LogKeyAndEntry;
	struct CDistributedAppLogStoreLocal;

	struct CDistiributedAppLogActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		TCFuture<CActorSubscription> f_OnStopDeferring(TCActorFunctorWeak<TCFuture<void> ()> _fOnStopDefer);

		TCFuture<void> f_StopDeferring();

	private:
		TCFuture<void> fp_Destroy() override;

		NContainer::TCMap<NStr::CStr, TCActorFunctorWeak<TCFuture<void> ()>> mp_StopDeferSubscriptions;
	};

	struct CDistributedAppState
	{
		NEncoding::CSimpleJsonDatabase m_StateDatabase;
		NEncoding::CSimpleJsonDatabase m_ConfigDatabase;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCDistributedActor<CDistributedAppInterfaceServer> m_AppInterfaceServer;
		CTrustedActorInfo m_AppInterfaceServerActorInfo;
		TCActor<CDistiributedAppLogActor> m_LogActor;
		NWeb::NHTTP::CURL m_LocalAddress;
		TCWeakActor<CDistributedAppActor> m_AppActor;
		NStr::CStr m_HostID;
		NStr::CStr m_CommandLineHostID;
		NStr::CStr m_RootDirectory;
		bool m_bStoppingApp = false;

		CDistributedAppAuditor f_Auditor(NStr::CStr const &_Category = {}, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;
		NFunction::TCFunctionMovable<CDistributedAppAuditor (CCallingHostInfo const &_CallingHostInfo, NStr::CStr const &_Category)> f_AuditorFactory(NStr::CStr const &_Category = {}) const;

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

			TCFuture<uint32> f_RunCommandLine(NStr::CStr _Command, NEncoding::CEJsonSorted _Params, CCommandLineControl _CommandLine) override;

		private:
			TCWeakActor<CDistributedAppActor> mp_Actor;
		};

		enum ESensorOutputFlag : uint32
		{
			ESensorOutputFlag_None = 0
			, ESensorOutputFlag_Verbose = DMibBit(0)
			, ESensorOutputFlag_Json = DMibBit(1)
			, ESensorOutputFlag_Status = DMibBit(2)
		};

		enum ELogOutputFlag : uint32
		{
			ELogOutputFlag_None = 0
			, ELogOutputFlag_Json = DMibBit(0)
			, ELogOutputFlag_Raw = DMibBit(1)
		};

		CDistributedAppActor(CDistributedAppActor_Settings const &_Settings);
		~CDistributedAppActor();

		TCFuture<NStr::CStr> f_StartApp(NEncoding::CEJsonSorted const _Params, TCActor<CDistiributedAppLogActor> _LogActor, EDistributedAppType _AppType);
		TCFuture<void> f_StopApp();
		TCFuture<CDistributedAppLogReporter::CLogReporter> f_OpenDefaultLogReporter();

		void f_SetAppType(EDistributedAppType _AppType);
		void f_LogApplicationInfo();

		TCFuture<CDistributedAppCommandLineClient> f_GetCommandLineClient(NStorage::TCSharedPointer<CRunLoop> _pRunLoop);

		TCFuture<uint32> f_RunCommandLine
			(
				NStr::CStr _Command
				, NEncoding::CEJsonSorted const _Params
				, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
			)
		;

		uint32 f_CommandLine_RemoveAllTrust(bool _bConfirm);

		TCFuture<uint32> f_CommandLine_AddConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _Ticket
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
				, NContainer::TCSet<NStr::CStr> _TrustedNamespaces
			)
		;
		TCFuture<uint32> f_CommandLine_GenerateTrustTicket
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _ForListen
				, NContainer::TCSet<NStr::CStr> _Permissions
				, NStr::CStr _UserID
				, NEncoding::CEJsonSorted _AuthenticationFactors
				, int64 _AuthenticationLifetime
			)
		;

		TCFuture<uint32> f_CommandLine_ListTrustedHosts(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, bool _bIncludeFriendlyHostName, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_RemoveTrustedHost(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _HostID);

		TCFuture<uint32> f_CommandLine_GetHostID(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine);

		TCFuture<uint32> f_CommandLine_GetConnetionStatus(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_GetDebugStats(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _TableType);

		TCFuture<uint32> f_CommandLine_ListConnections(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, bool _bIncludeFriendlyHostName, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_RemoveConnection(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _URL);
		TCFuture<uint32> f_CommandLine_AddAdditionalConnection
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _URL
				, bool _bIncludeFriendlyHostName
				, int32 _ConnectionConcurrency
			)
		;
		TCFuture<uint32> f_CommandLine_SetConnectionConcurrency
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _URL
				, int32 _ConnectionConcurrency
			)
		;

		TCFuture<uint32> f_CommandLine_AddListen(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _URL, bool _bPrimary);
		TCFuture<uint32> f_CommandLine_RemoveListen(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _URL);
		TCFuture<uint32> f_CommandLine_SetPrimaryListen(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _URL);
		TCFuture<uint32> f_CommandLine_ListListen(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _TableType);

		TCFuture<uint32> f_CommandLine_ListNamespaces(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, bool _bIncludeTrustedHosts, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_TrustHostForNamespace
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _Namespace
				, NStr::CStr _Host
			)
		;
		TCFuture<uint32> f_CommandLine_UntrustHostForNamespace
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _Namespace
				, NStr::CStr _Host
			)
		;

		TCFuture<uint32> f_CommandLine_ListPermissions(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, bool _bIncludeTargets, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_AddPermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _HostID
				, NStr::CStr _UserID
				, NContainer::TCVector<NStr::CStr> _Permissions
				, NEncoding::CEJsonSorted _AuthenticationFactors
				, int64 _AuthenticationLifetime
			)
		;
		TCFuture<uint32> f_CommandLine_RemovePermission
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _HostID
				, NStr::CStr _UserID
				, NStr::CStr _Permission
			)
		;

		TCFuture<uint32> f_CommandLine_ListUsers(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_AddUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NEncoding::CEJsonSorted const _Params);
		TCFuture<uint32> f_CommandLine_RemoveUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _UserID);
		TCFuture<uint32> f_CommandLine_SetUserInfo(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NEncoding::CEJsonSorted const _Params);
		TCFuture<uint32> f_CommandLine_RemoveMetadata(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _UserID, NStr::CStr _Key);
		TCFuture<uint32> f_CommandLine_ExportUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _UserID, bool _bIncludePrivate);
		TCFuture<uint32> f_CommandLine_ImportUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _UserData);
		TCFuture<uint32> f_CommandLine_GetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine);
		TCFuture<uint32> f_CommandLine_SetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _UserID);

		TCFuture<uint32> f_CommandLine_RegisterAuthenticationFactor
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _UserID
				, NStr::CStr _Factor
				, bool _bQuiet
			)
		;
		TCFuture<uint32> f_CommandLine_UnregisterAuthenticationFactor
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _UserID
				, NStr::CStr _Factor
			)
		;
		TCFuture<uint32> f_CommandLine_EnumUserAuthenticationFactors
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _UserID
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_EnumAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NStr::CStr _TableType);
		TCFuture<uint32> f_CommandLine_AuthenticatePermissionPattern(NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine, NEncoding::CEJsonSorted const _Params);
		TCFuture<uint32> f_CommandLine_SensorList
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, CDistributedAppSensorReader_SensorFilter _Filter
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorListOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> _Sensors
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorStatus
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, CDistributedAppSensorReader_SensorStatusFilter _Filter
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorReadingsList
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, CDistributedAppSensorReader_SensorReadingFilter _Filter
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorReadingsOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> _SensorReadings
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> _Sensors
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
				, CDistributedAppSensorReader_SensorReadingFilter _Filter
			)
		;
		TCFuture<uint32> fp_CommandLine_SensorReadingsOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> _SensorReadings
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> _Sensors
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr _TableType
				, CDistributedAppSensorReader_SensorReadingFilter _Filter
			)
		;

		TCFuture<uint32> f_CommandLine_LogList
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, CDistributedAppLogReader_LogFilter _Filter
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogListOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> _Logs
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogEntriesList
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, CDistributedAppLogReader_LogEntryFilter _Filter
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr _TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogEntriesOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> _LogEntries
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> _Logs
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr _TableType
				, CDistributedAppLogReader_LogEntryFilter _Filter
			)
		;

		static void fs_LogAudit(CDistributedAppAuditParams const &_AuditParams, NStr::CStr const &_DefaultCategory);
		void f_Audit(CDistributedAppAuditParams &&_AuditParams);

		CDistributedAppAuditor f_Auditor(NStr::CStr const &_Category = {}, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

		auto f_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo _SensorInfo) -> TCFuture<CDistributedAppSensorReporter::CSensorReporter>;
		TCFuture<TCActor<CDistributedAppSensorStoreLocal>> f_OpenSensorStoreLocal();

		auto f_OpenLogReporter(CDistributedAppLogReporter::CLogInfo _LogInfo) -> TCFuture<CDistributedAppLogReporter::CLogReporter>;
		TCFuture<TCActor<CDistributedAppLogStoreLocal>> f_OpenLogStoreLocal();

#if DMibConfig_Tests_Enable
		TCFuture<NEncoding::CEJsonSorted> f_Test_Command(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params);
#endif

	protected:
		struct CRegisteredAuthentication
		{
			CTrustedActorInfo m_ActorInfo;
			CActorSubscription m_Subscription;
		};

		struct CAuthenticationSetupResult
		{
			CActorSubscription m_Subscription;
			uint32 m_HandlerID = 0;
		};

		struct CAuthenticationHandlerState
		{
			TCDistributedActor<ICDistributedActorAuthenticationHandler> m_Handler;
			TCTrustedActorSubscription<ICDistributedActorAuthentication> m_AuthenticationRemotes;
			NContainer::TCMap<TCWeakDistributedActor<ICDistributedActorAuthentication>, CRegisteredAuthentication> m_RegistrationSubscriptions;
		};

		virtual TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) = 0;
		virtual TCFuture<void> fp_StopApp() = 0;

		TCFuture<void> fp_WaitForAppStartup();

		TCFuture<void> fp_AuditToDistributedLogger(CDistributedAppAuditParams _AuditParams);

		void fp_BuildDefaultCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine, EDefaultCommandLineFunctionality _Functionalities);
		void fp_BuildDefaultCommandLine_Logging(CDistributedAppCommandLineSpecification &o_CommandLine);
		void fp_BuildDefaultCommandLine_DistributedComputingAuthentication(CDistributedAppCommandLineSpecification &o_CommandLine);
		void fp_BuildDefaultCommandLine_DistributedComputing(CDistributedAppCommandLineSpecification &o_CommandLine);
		void fp_BuildDefaultCommandLine_DistributedLog(CDistributedAppCommandLineSpecification &o_CommandLine);
		void fp_BuildDefaultCommandLine_DistributedLog_Customizable
			(
				CDistributedAppCommandLineSpecification::CSection _Section
				, NStr::CStr const &_Prefix
				, TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJsonSorted const _Params
						, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
						, CDistributedAppLogReader_LogFilter _Filter
						, ELogOutputFlag _Flags
						, uint32 _Verbosity
						, NStr::CStr _TableType
					)
				> &&_fLogList
				,
				TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJsonSorted const _Params
						, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
						, CDistributedAppLogReader_LogEntryFilter _Filter
						, uint64 _MaxEntries
						, ELogOutputFlag _Flags
						, uint32 _Verbosity
						, NStr::CStr _TableType
					)
				> && _fLogEntriesList
				, EDistributedAppCommandFlag _CommandFlags
			)
		;
		TCFuture<uint32> fp_CommandLine_LogEntriesOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> _LogEntries
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> _Logs
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr _TableType
				, CDistributedAppLogReader_LogEntryFilter _Filter
			)
		;
		void fp_BuildDefaultCommandLine_Sensor(CDistributedAppCommandLineSpecification &o_CommandLine);
		void fp_BuildDefaultCommandLine_Sensor_Customizable
			(
				CDistributedAppCommandLineSpecification::CSection _Section
				, NStr::CStr const &_Prefix
				, TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJsonSorted const _Params
						, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
						, CDistributedAppSensorReader_SensorFilter _Filter
						, ESensorOutputFlag _Flags
						, NStr::CStr _TableType
					)
				> &&_fSensorList
				, TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJsonSorted const _Params
						, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
						, CDistributedAppSensorReader_SensorStatusFilter _Filter
						, ESensorOutputFlag _Flags
						, NStr::CStr _TableType
					)
				> &&_fSensorStatus
				,
				TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJsonSorted const _Params
						, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
						, CDistributedAppSensorReader_SensorReadingFilter _Filter
						, uint64 _MaxEntries
						, ESensorOutputFlag _Flags
						, NStr::CStr _TableType
					)
				> && _fSensorReadingsList
				, EDistributedAppCommandFlag _CommandFlags
			)
		;
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine);

		virtual TCFuture<void> fp_PreRunCommandLine(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine);
		virtual void fp_PopulateAppInterfaceInfo
			(
				CDistributedAppInterfaceServer::CRegisterInfo &o_RegisterInfo
				, CDistributedAppInterfaceServer::CConfigFiles &o_ConfigFiles
				, NEncoding::CEJsonSorted const &_Params
			)
		;

		virtual TCFuture<void> fp_PreUpdate();
		virtual TCFuture<void> fp_PreStop(); /// Stop generating new data here to make backups consistent
		virtual TCFuture<CActorSubscription> fp_StartBackup
			(
				TCDistributedActorInterface<CDistributedAppInterfaceBackup> _BackupInterface
				, CActorSubscription _ManifestFinished
				, NStr::CStr _BackupRoot
			)
		;

		// To enable authentication override fp_SetupAuthentication and call fp_EnableAuthentication
        virtual TCFuture<CAuthenticationSetupResult> fp_SetupAuthentication
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, int64 _AuthenticationLifetime
				, NStr::CStr _UserID
			)
		;
        TCFuture<CAuthenticationSetupResult> fp_EnableAuthentication
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, int64 _AuthenticationLifetime
				, NStr::CStr _UserID
			)
		;

#if DMibConfig_Tests_Enable
		virtual TCFuture<NEncoding::CEJsonSorted> fp_Test_Command(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params);
#endif

		void fp_Construct() override;
		TCFuture<void> fp_Destroy() override;

		TCFuture<void> fp_SaveStateDatabase();
		TCFuture<void> fp_SaveConfigDatabase();

		CFutureCoroutineContextOnResumeScopeAwaiter fp_CheckStoppedOrDestroyedOnResume() const;

		CCallingHostInfoScope fp_PopulateCurrentHostInfoIfMissing(NStr::CStr _Description);

		TCFuture<CActorSubscription> fp_RegisterForAppInterfaceServerChanges
			(
				TCActorFunctor
				<
					TCFuture<void> (TCDistributedActor<CDistributedAppInterfaceServer> _AppInterfaceServer, CTrustedActorInfo _TrustInfo)
				> _fOnChangeInterfaceServer
			)
		;

		auto fp_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo _SensorInfo) -> TCFuture<CDistributedAppSensorReporter::CSensorReporter>;
		TCFuture<TCActor<CDistributedAppSensorStoreLocal>> fp_OpenSensorStoreLocal();

		auto fp_OpenLogReporter(CDistributedAppLogReporter::CLogInfo _LogInfo) -> TCFuture<CDistributedAppLogReporter::CLogReporter>;
		TCFuture<TCActor<CDistributedAppLogStoreLocal>> fp_OpenLogStoreLocal();

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
		NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> mp_SensorMetadata;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> mp_LogMetadata;

		NContainer::TCMap<uint32, CAuthenticationHandlerState> mp_AuthenticationHandlers;
		uint32 mp_NextAuthenticationHandlerID = 1;

	private:
		struct CDistributedAppInterfaceClientImplementation;

		TCFuture<void> fp_WaitForDistributedTrustInitialization();
		TCFuture<void> fp_InitializeDistributedTrust();
		TCFuture<void> fp_Initialize(NEncoding::CEJsonSorted const _Params);
		void fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		void fp_CleanupOldExecutables();
#endif

		TCFuture<void> fp_SetupListen();
		TCFuture<void> fp_SetupAppServerInterface(NEncoding::CEJsonSorted const _Params);
		TCFuture<void> fp_SubscribeAppServerInterface(NEncoding::CEJsonSorted const _Params);
		TCFuture<CDistributedActorTrustManager::CTrustTicket> fp_GetTicketThroughStdIn(NStr::CStr _RequestMagic);

		TCFuture<void> fp_CreateCommandLineTrust();
		TCFuture<void> fp_SetupCommandLineListen();
		TCFuture<void> fp_SetupCommandLineTrust();
		TCFuture<void> fp_SetupRemoteCommandLine();

		TCFuture<uint32> fp_RunCommandLineAndLogError
			(
				NStr::CStr _Description
				, NFunction::TCFunctionMovable<TCFuture<uint32> ()> _fCommand
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

		struct CInternal;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};

	struct CApplyLoggingResults
	{
		void f_Destroy(CActorDestroyEventLoop const &_DestroyLoop);

		TCActor<CDistiributedAppLogActor> m_LogActor;
		COnScopeExitShared m_CleanupLogging;
	};

	CApplyLoggingResults fg_ApplyLoggingOption(NEncoding::CEJsonSorted const &_Params, TCActor<CDistributedAppActor> const &_DistributedLoggingApp);

	struct CRunDistributedAppHelper
	{
		aint f_RunCommandLine
			(
				NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
				, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
				, bool _bStartApp = true
				, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop = nullptr
			)
		;

		void f_Stop();

		NStorage::TCSharedPointer<CRunLoop> m_pRunLoop;
		TCActor<CDistributedAppActor> m_AppActor;
		CApplyLoggingResults m_ApplyLoggingResults;
		bool m_bStartedApp = false;
	};

	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
			, bool _bStartApp = true
			, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop = nullptr
		)
	;
}

#include "Malterlib_Concurrency_DistributedApp.hpp"
