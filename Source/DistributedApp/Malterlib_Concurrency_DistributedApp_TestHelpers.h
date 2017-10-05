// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Concurrency/DistributedAppInterfaceLaunch>

namespace NMib::NConcurrency
{
	struct CDistributedApp_LaunchHelperDependencies
	{
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		NHTTP::CURL m_Address;
	};

	struct CDistributedApp_LaunchInfo
	{
		CDistributedApp_LaunchInfo();
		~CDistributedApp_LaunchInfo();
		
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo &&) = default;
		CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfo const &_Other) = default;
		
		TCActor<CDistributedAppInterfaceLaunchActor> m_Launch;
		NPtr::TCSharedPointer<CActorSubscription> m_pLaunchSubscription;
		NStr::CStr m_HostID;
		NStr::CStr m_LaunchID;
		NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient>> m_pClientInterface;
		NPtr::TCSharedPointer<NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface>> m_pTrustInterface;
		TCContinuation<CDistributedApp_LaunchInfo> m_Continuation;
		
		void f_Abort();
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
		
		CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr);
		~CDistributedApp_LaunchHelper();
		
		TCContinuation<void> fp_Destroy() override;
		TCContinuation<CDistributedApp_LaunchInfo> f_Launch(NStr::CStr const &_Description, NStr::CStr const &_Executable);
		TCContinuation<CDistributedApp_LaunchInfo> f_LaunchWithParams(NStr::CStr const &_Description, NStr::CStr const &_Executable, NContainer::TCVector<NStr::CStr> &&_ExtraParams);

		CDistributedApp_LaunchHelperDependencies m_Dependencies;
		NContainer::TCMap<NStr::CStr, CDistributedApp_LaunchInfo> m_Launches;
		TCDelegatedActorInterface<CDistributedAppInterfaceServerImplementation> m_AppInterfaceServer;
		bool m_bLogToStderr = false;
	};
}
