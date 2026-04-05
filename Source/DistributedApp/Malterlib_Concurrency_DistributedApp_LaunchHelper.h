// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedAppInterfaceLaunch>
#include <Mib/Concurrency/DistributedAppInProcess>

namespace NMib::NConcurrency
{
	struct CDistributedApp_LaunchHelperDependencies
	{
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		NWeb::NHTTP::CURL m_Address;
	};

	struct CDistributedApp_LaunchInfoData
	{
		CDistributedApp_LaunchInfoData();
		~CDistributedApp_LaunchInfoData();

		CDistributedApp_LaunchInfoData(CDistributedApp_LaunchInfoData &&) = default;
		CDistributedApp_LaunchInfoData(CDistributedApp_LaunchInfoData const &_Other) = default;

		CDistributedApp_LaunchInfoData &operator = (CDistributedApp_LaunchInfoData &&_Other) = default;

		TCFuture<void> f_Destroy();

#if DMibConfig_Tests_Enable
		TCFuture<NEncoding::CEJsonSorted> f_Test_Command(NStr::CStr &&_Command, NEncoding::CEJsonSorted &&_Params);

		TCFuture<uint32> f_RunCommandLine
			(
				CCallingHostInfo &&_CallingHost
				, NStr::CStr &&_Command
				, NEncoding::CEJsonSorted &&_Params
				, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine
			)
		;
#endif

		TCActor<CDistributedAppInterfaceLaunchActor> m_Launch;
		NStorage::TCSharedPointer<CActorSubscription> m_pLaunchSubscription;

		TCActor<CDistributedAppInProcessActor> m_InProcess;

		NStr::CStr m_HostID;
		NStr::CStr m_LaunchID;
		NStorage::TCSharedPointer<TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient>> m_pClientInterface;
		NStorage::TCSharedPointer<TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> m_pTrustInterface;
	};

	struct CDistributedApp_LaunchInfo : public CDistributedApp_LaunchInfoData
	{
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfoData const &_LaunchInfo, CActorSubscription &&_Subscription);
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo &&_Other) = default;
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo const &_Other) = delete;

		~CDistributedApp_LaunchInfo();

		CDistributedApp_LaunchInfo &operator = (CDistributedApp_LaunchInfo &&_Other) = default;

		TCFuture<void> f_Destroy();

		CActorSubscription m_Subscription;
	};

	struct CDistributedApp_LaunchHelper : public CActor
	{
		struct CDistributedAppInterfaceServerImplementation : public CDistributedAppInterfaceServer
		{
			TCFuture<TCActorSubscriptionWithID<>> f_RegisterDistributedApp
				(
					TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> _ClientInterface
					, TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> _TrustInterface
					, CRegisterInfo _RegisterInfo
				) override
			;
			TCFuture<TCActorSubscriptionWithID<>> f_RegisterConfigFiles(CConfigFiles _ConfigFiles) override;

			TCFuture<TCDistributedActorInterfaceWithID<CDistributedAppSensorReporter>> f_GetSensorReporter() override;
			TCFuture<TCDistributedActorInterfaceWithID<CDistributedAppLogReporter>> f_GetLogReporter() override;

			DMibDelegatedActorImplementation(CDistributedApp_LaunchHelper);
		};

		struct CLaunchInfo : public CDistributedApp_LaunchInfoData
		{
			void f_Abort();

			TCPromise<CDistributedApp_LaunchInfo> m_Promise;
			DMibListLinkDS_Link(CLaunchInfo, m_Link);
		};

		struct CPendingLaunch
		{
			NStorage::TCSharedPointer<TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient>> m_pClientInterface;
			NStorage::TCSharedPointer<TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> m_pTrustInterface;
		};

		CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr);
		~CDistributedApp_LaunchHelper();

		CActorSubscription fp_GetLaunchSubscription(NStr::CStr const &_LaunchID);

		TCFuture<void> fp_Destroy() override;
		TCFuture<CDistributedApp_LaunchInfo> f_Launch(NStr::CStr _Description, NStr::CStr _Executable);
		TCFuture<CDistributedApp_LaunchInfo> f_LaunchWithLaunch(NStr::CStr _Description, NProcess::CProcessLaunchActor::CLaunch _Launch, TCActor<CActor> _NotificationActor);

		TCFuture<CDistributedApp_LaunchInfo> f_LaunchWithParams
			(
				NStr::CStr _Description
				, NStr::CStr _Executable
				, NContainer::TCVector<NStr::CStr> _ExtraParams
				, CSystemEnvironment _Environment
			)
		;
		TCFuture<CDistributedApp_LaunchInfo> f_LaunchInProcess
			(
				NStr::CStr _Description
				, NStr::CStr _HomeDirectory
				, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> _fDistributedAppFactory
				, NContainer::TCVector<NStr::CStr> _Params
			)
		;

		CDistributedApp_LaunchHelperDependencies m_Dependencies;
		NContainer::TCMap<NStr::CStr, CLaunchInfo> m_Launches;
		DMibListLinkDS_List(CLaunchInfo, m_Link) m_OrderdedLaunches;
		NContainer::TCMap<NStr::CStr, CPendingLaunch> m_PendingLaunches;
		NContainer::TCMap<NStr::CStr, TCPromise<void>> m_PendingDestroys;
		TCDistributedActorInstance<CDistributedAppInterfaceServerImplementation> m_AppInterfaceServer;
		bool m_bLogToStderr = false;
	};
}
