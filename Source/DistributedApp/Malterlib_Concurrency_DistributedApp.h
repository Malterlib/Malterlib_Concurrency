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

		TCFuture<CActorSubscription> f_OnStopDeferring(TCActorFunctorWeak<TCFuture<void> ()> &&_fOnStopDefer);

		TCFuture<void> f_StopDeferring();

	private:
		TCFuture<void> fp_Destroy() override;

		NContainer::TCMap<NStr::CStr, TCActorFunctorWeak<TCFuture<void> ()>> mp_StopDeferSubscriptions;
	};

	struct CDistributedAppState
	{
		NEncoding::CSimpleJSONDatabase m_StateDatabase;
		NEncoding::CSimpleJSONDatabase m_ConfigDatabase;
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

			TCFuture<uint32> f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSONSorted const &_Params, CCommandLineControl &&_CommandLine) override;

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

		TCFuture<NStr::CStr> f_StartApp(NEncoding::CEJSONSorted const &_Params, TCActor<CDistiributedAppLogActor> const &_LogActor, EDistributedAppType _AppType);
		TCFuture<void> f_StopApp();
		TCFuture<CDistributedAppLogReporter::CLogReporter> f_OpenDefaultLogReporter();

		void f_SetAppType(EDistributedAppType _AppType);
		void f_LogApplicationInfo();

		TCFuture<CDistributedAppCommandLineClient> f_GetCommandLineClient();

		TCFuture<uint32> f_RunCommandLine
			(
				NStr::CStr const &_Command
				, NEncoding::CEJSONSorted const &_Params
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
				, NEncoding::CEJSONSorted const &_AuthenticationFactors
			)
		;

		TCFuture<uint32> f_CommandLine_ListTrustedHosts(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_RemoveTrustedHost(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID);

		TCFuture<uint32> f_CommandLine_GetHostID(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);

		TCFuture<uint32> f_CommandLine_GetConnetionStatus(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);
		TCFuture<uint32> f_CommandLine_GetDebugStats(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_TableType);

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

		TCFuture<uint32> f_CommandLine_AddListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL, bool _bPrimary);
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
				, NContainer::TCVector<NStr::CStr> const &_Permissions
				, NEncoding::CEJSONSorted const &_AuthenticationFactors
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
		TCFuture<uint32> f_CommandLine_AddUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSONSorted const &_Params);
		TCFuture<uint32> f_CommandLine_RemoveUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID);
		TCFuture<uint32> f_CommandLine_SetUserInfo(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSONSorted const &_Params);
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
		TCFuture<uint32> f_CommandLine_AuthenticatePermissionPattern(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NEncoding::CEJSONSorted const &_Params);
		TCFuture<uint32> f_CommandLine_SensorList
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CDistributedAppSensorReader_SensorFilter const &_Filter
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorListOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorStatus
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CDistributedAppSensorReader_SensorStatusFilter const &_Filter
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorReadingsList
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_SensorReadingsOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> &&_SensorReadings
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
				, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
			)
		;
		TCFuture<uint32> fp_CommandLine_SensorReadingsOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> &&_SensorReadings
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
				, uint64 _MaxEntries
				, ESensorOutputFlag _Flags
				, NStr::CStr const &_TableType
				, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
			)
		;

		TCFuture<uint32> f_CommandLine_LogList
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CDistributedAppLogReader_LogFilter const &_Filter
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogListOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogEntriesList
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CDistributedAppLogReader_LogEntryFilter const &_Filter
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr const &_TableType
			)
		;
		TCFuture<uint32> f_CommandLine_LogEntriesOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> &&_LogEntries
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr const &_TableType
				, CDistributedAppLogReader_LogEntryFilter const &_Filter
			)
		;

		static void fs_LogAudit(CDistributedAppAuditParams const &_AuditParams, NStr::CStr const &_DefaultCategory);
		void f_Audit(CDistributedAppAuditParams &&_AuditParams);

		CDistributedAppAuditor f_Auditor(NStr::CStr const &_Category = {}, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

#if DMibConfig_Tests_Enable
		TCFuture<NEncoding::CEJSONSorted> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSONSorted const &_Params);
#endif

	protected:
		struct CRegisteredAuthentication
		{
			CTrustedActorInfo m_ActorInfo;
			CActorSubscription m_Subscription;
		};

		virtual TCFuture<void> fp_StartApp(NEncoding::CEJSONSorted const &_Params) = 0;
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
						NEncoding::CEJSONSorted const &_Params
						, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
						, CDistributedAppLogReader_LogFilter const &_Filter
						, ELogOutputFlag _Flags
						, uint32 _Verbosity
						, NStr::CStr const &_TableType
					)
				> &&_fLogList
				,
				TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJSONSorted const &_Params
						, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
						, CDistributedAppLogReader_LogEntryFilter const &_Filter
						, uint64 _MaxEntries
						, ELogOutputFlag _Flags
						, uint32 _Verbosity
						, NStr::CStr const &_TableType
					)
				> && _fLogEntriesList
				, EDistributedAppCommandFlag _CommandFlags
			)
		;
		TCFuture<uint32> fp_CommandLine_LogEntriesOutput
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> &&_LogEntries
				, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
				, uint64 _MaxEntries
				, ELogOutputFlag _Flags
				, uint32 _Verbosity
				, NStr::CStr const &_TableType
				, CDistributedAppLogReader_LogEntryFilter const &_Filter
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
						NEncoding::CEJSONSorted const &_Params
						, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
						, CDistributedAppSensorReader_SensorFilter const &_Filter
						, ESensorOutputFlag _Flags
						, NStr::CStr const &_TableType
					)
				> &&_fSensorList
				, TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJSONSorted const &_Params
						, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
						, CDistributedAppSensorReader_SensorStatusFilter const &_Filter
						, ESensorOutputFlag _Flags
						, NStr::CStr const &_TableType
					)
				> &&_fSensorStatus
				,
				TCActorFunctor
				<
					TCFuture<uint32>
					(
						NEncoding::CEJSONSorted const &_Params
						, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
						, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
						, uint64 _MaxEntries
						, ESensorOutputFlag _Flags
						, NStr::CStr const &_TableType
					)
				> && _fSensorReadingsList
				, EDistributedAppCommandFlag _CommandFlags
			)
		;
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine);

		virtual TCFuture<void> fp_PreRunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSONSorted const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine);
		virtual void fp_PopulateAppInterfaceInfo
			(
				CDistributedAppInterfaceServer::CRegisterInfo &o_RegisterInfo
				, CDistributedAppInterfaceServer::CConfigFiles &o_ConfigFiles
				, NEncoding::CEJSONSorted const &_Params
			)
		;

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
		virtual TCFuture<NEncoding::CEJSONSorted> fp_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSONSorted const &_Params);
#endif

		void fp_Construct() override;
		TCFuture<void> fp_Destroy() override;

		TCFuture<void> fp_SaveStateDatabase();
		TCFuture<void> fp_SaveConfigDatabase();

		CCallingHostInfoScope fp_PopulateCurrentHostInfoIfMissing(NStr::CStr const &_Description);

		TCFuture<CActorSubscription> fp_RegisterForAppInterfaceServerChanges
			(
				TCActorFunctor
				<
					TCFuture<void> (TCDistributedActor<CDistributedAppInterfaceServer> const &_AppInterfaceServer, CTrustedActorInfo const &_TrustInfo)
				> &&_fOnChangeInterfaceServer
			)
		;

		auto fp_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo) -> TCFuture<CDistributedAppSensorReporter::CSensorReporter>;
		TCFuture<TCActor<CDistributedAppSensorStoreLocal>> fp_OpenSensorStoreLocal();

		auto fp_OpenLogReporter(CDistributedAppLogReporter::CLogInfo &&_LogInfo) -> TCFuture<CDistributedAppLogReporter::CLogReporter>;
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

		TCTrustedActorSubscription<ICDistributedActorAuthentication> mp_AuthenticationRemotes;
		TCDistributedActor<ICDistributedActorAuthenticationHandler> mp_AuthenticationHandlerImplementation;
		NContainer::TCMap<TCWeakDistributedActor<ICDistributedActorAuthentication>, CRegisteredAuthentication> mp_AuthenticationRegistrationSubscriptions;

	private:
		struct CDistributedAppInterfaceClientImplementation;

		TCFuture<void> fp_WaitForDistributedTrustInitialization();
		TCFuture<void> fp_InitializeDistributedTrust();
		TCFuture<void> fp_Initialize(NEncoding::CEJSONSorted _Params);
		void fp_CleanupEnclaveSockets();
#ifdef DPlatformFamily_Windows
		void fp_CleanupOldExecutables();
#endif

		TCFuture<void> fp_SetupListen();
		TCFuture<void> fp_SetupAppServerInterface(NEncoding::CEJSONSorted _Params);
		TCFuture<void> fp_SubscribeAppServerInterface(NEncoding::CEJSONSorted _Params);
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

		struct CInternal;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};

	struct CApplyLoggingResults
	{
		void f_Destroy(CActorDestroyEventLoop const &_DestroyLoop);

		TCActor<CDistiributedAppLogActor> m_LogActor;
		COnScopeExitShared m_CleanupLogging;
	};

	CApplyLoggingResults fg_ApplyLoggingOption(NEncoding::CEJSONSorted const &_Params, TCActor<CDistributedAppActor> const &_DistributedLoggingApp);

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
		bool m_bInstalledLogDispatcher = false;
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
