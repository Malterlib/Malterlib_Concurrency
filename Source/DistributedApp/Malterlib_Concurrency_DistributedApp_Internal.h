// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/DistributedAppSensorStoreLocal>
#include <Mib/Concurrency/DistributedAppLogStoreLocal>

namespace NMib::NConcurrency
{
	struct CDistributedAppActor::CDistributedAppInterfaceClientImplementation : public CDistributedAppInterfaceClient
	{
		NConcurrency::TCFuture<void> f_GetAppStartResult() override;
		NConcurrency::TCFuture<void> f_PreUpdate() override;
		NConcurrency::TCFuture<void> f_PreStop() override;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_StartBackup
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> _BackupInterface
				, NConcurrency::CActorSubscription _ManifestFinished
				, NStr::CStr _BackupRoot
			) override
		;

		DMibDelegatedActorImplementation(CDistributedAppActor);
	};

	struct CDistributedAppActor::CInternal
	{
		CInternal();
		~CInternal();

		TCActor<ICDistributedActorTrustManagerDatabase> m_TrustManagerDatabase;
		TCDistributedActor<CCommandLine> m_CommandLine;
		CDistributedActorPublication m_CommandLinePublication;
		NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> m_pCommandLineSpec;
		NStorage::TCUniquePointer<TCActorCallOnce<void>> m_pInitOnce;
		COnScopeExitShared m_pStdInCleanup;

		TCTrustedActorSubscription<CDistributedAppInterfaceServer> m_AppInteraceServerSubscription;
		TCDistributedActorInstance<CDistributedAppInterfaceClientImplementation> m_AppInterfaceClientImplementation;
		TCDistributedActor<CDistributedActorTrustManagerInterface> m_AppInterfaceClientTrustProxy;
		CActorSubscription m_AppInterfaceClientRegistrationSubscription;
		CActorSubscription m_AppInterfaceClientRegistrationConfigSubscription;
		TCAsyncResult<void> m_AppStartupResult;
		NContainer::TCVector<TCPromise<void>> m_DeferredAppStartupResults;
		TCAsyncResult<void> m_DistributedTrustInitResult;
		NContainer::TCVector<TCPromise<void>> m_DeferredDistributedTrustInit;

		NContainer::TCMap<NStr::CStr, CActorSubscription> m_TicketPermissionSubscriptions;

		CTrustedPermissionSubscription m_Permissions;

		NStr::CStr m_CurrentLogDirectory;
		EDistributedAppType m_AppType = fg_DistributedAppThreadLocal().m_DefaultAppType;

		NContainer::TCMap<NStr::CStr, TCActorFunctor<TCFuture<void> (TCDistributedActor<CDistributedAppInterfaceServer> _AppInterfaceServer, CTrustedActorInfo _TrustInfo)>>
			m_AppInterfaceServerChangeSubscriptions
		;

		TCActor<CDistributedAppSensorStoreLocal> m_AppSensorStoreLocal;
		CSequencer m_AppSensorStoreLocalInitSequencer{"DistributedAppActor AppSensorStoreLocalInitSequencer"};
		CActorSubscription m_AppSensorStoreLocalAppServerChangeSubscription;
		CActorSubscription m_AppSensorStoreLocalExtraSensorInterfaceSubscription;
		CSequencer m_AppSensorStoreLocalAppServerChangeSequencer{"DistributedAppActor AppSensorStoreLocalAppServerChangeSequencer"};

		TCActor<CDistributedAppLogStoreLocal> m_AppLogStoreLocal;
		CSequencer m_AppLogStoreLocalInitSequencer{"DistributedAppActor AppLogStoreLocalInitSequencer"};
		CActorSubscription m_AppLogStoreLocalAppServerChangeSubscription;
		CActorSubscription m_AppLogStoreLocalExtraLogInterfaceSubscription;
		CSequencer m_AppLogStoreLocalAppServerChangeSequencer{"DistributedAppActor AppLogStoreLocalAppServerChangeSequencer"};

		CSequencer m_AuditLogReporterInitSequencer{"DistributedAppActor AuditLogReporterInitSequencer"};
		NStorage::TCOptional<CDistributedAppLogReporter::CLogReporter> m_AuditLogReporter;

		NStorage::TCSharedPointer<CCanDestroyTracker> m_pCanDestroyAuditLogs = fg_Construct();

		bool m_bDelegateTrustToAppInterface = false;
		bool m_bAuditLogsDestroyed = false;
#if DMibEnableSafeCheck > 0
		bool m_bDestroyCalled = false;
#endif
	};
}
