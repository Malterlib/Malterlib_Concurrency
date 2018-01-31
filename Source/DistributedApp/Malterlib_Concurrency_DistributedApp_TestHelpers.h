// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
		NHTTP::CURL m_Address;
	};

	struct CDistributedApp_LaunchInfoData
	{
		CDistributedApp_LaunchInfoData();
		~CDistributedApp_LaunchInfoData();

		CDistributedApp_LaunchInfoData(CDistributedApp_LaunchInfoData &&) = default;
		CDistributedApp_LaunchInfoData(CDistributedApp_LaunchInfoData const &_Other) = default;
	
		TCContinuation<void> f_Destroy();

#if DMibConfig_Tests_Enable
		TCContinuation<NEncoding::CEJSON> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
#endif
		
		TCActor<CDistributedAppInterfaceLaunchActor> m_Launch;
		NPtr::TCSharedPointer<CActorSubscription> m_pLaunchSubscription;

		TCActor<CDistributedAppInProcessActor> m_InProcess;

		NStr::CStr m_HostID;
		NStr::CStr m_LaunchID;
		NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient>> m_pClientInterface;
		NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> m_pTrustInterface;
	};
	
	struct CDistributedApp_LaunchInfo : public CDistributedApp_LaunchInfoData
	{
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfoData const &_LaunchInfo, CActorSubscription &&_Subscription);
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo &&_Other) = default;
		~CDistributedApp_LaunchInfo();
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo const &_Other) = delete;

		TCContinuation<void> f_Destroy();

		CActorSubscription m_Subscription;
	};
	
	struct CDistributedApp_LaunchHelper : public CActor
	{
		struct CDistributedAppInterfaceServerImplementation : public CDistributedAppInterfaceServer
		{
			NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterDistributedApp
				(
					NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
					, NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
					, CRegisterInfo const &_RegisterInfo
				) override
			;

			CDistributedApp_LaunchHelper *m_pThis = nullptr;
		};
		
		struct CLaunchInfo : public CDistributedApp_LaunchInfoData
		{
			void f_Abort();
			
			TCContinuation<CDistributedApp_LaunchInfo> m_Continuation;
		};
		
		struct CPendingLaunch
		{
			NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient>> m_pClientInterface;
			NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> m_pTrustInterface;
		};
	
		CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr);
		~CDistributedApp_LaunchHelper();
		
		CActorSubscription fp_GetLaunchSubscription(NStr::CStr const &_LaunchID);
		
		TCContinuation<void> fp_Destroy() override;
		TCContinuation<CDistributedApp_LaunchInfo> f_Launch(NStr::CStr const &_Description, NStr::CStr const &_Executable);
		TCContinuation<CDistributedApp_LaunchInfo> f_LaunchWithParams(NStr::CStr const &_Description, NStr::CStr const &_Executable, NContainer::TCVector<NStr::CStr> &&_ExtraParams);
		TCContinuation<CDistributedApp_LaunchInfo> f_LaunchInProcess(NStr::CStr const &_Description, NStr::CStr const &_HomeDirectory, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory);

		CDistributedApp_LaunchHelperDependencies m_Dependencies;
		NContainer::TCMap<NStr::CStr, CLaunchInfo> m_Launches;
		NContainer::TCMap<NStr::CStr, CPendingLaunch> m_PendingLaunches;
		NContainer::TCMap<NStr::CStr, TCContinuation<void>> m_PendingDestroys;
		TCDelegatedActorInterface<CDistributedAppInterfaceServerImplementation> m_AppInterfaceServer;
		bool m_bLogToStderr = false;
	};
}
