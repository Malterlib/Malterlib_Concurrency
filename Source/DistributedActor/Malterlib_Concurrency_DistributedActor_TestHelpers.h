// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/WeakActor>
#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;
	
	using CDistributedActorTestKeySettings = NCryptography::CPublicKeySettings_EC_secp256r1;
		
	struct CDistributedActorTestHelper
	{
		CDistributedActorTestHelper(NStr::CStr const &_HostID, TCActor<CActorDistributionManager> const &_Manager, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop);
		CDistributedActorTestHelper
			(
				TCActor<CDistributedActorTrustManager> const &_TrustManager
				, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
				, bool _bAllowUnconnected = false
			)
		;
		~CDistributedActorTestHelper();
		
		template <typename ...tfp_CDistributedActors, typename tf_CActor>
		NStr::CStr f_Publish(TCDistributedActor<tf_CActor> const &_Actor, NStr::CStr const &_Namespace);
		void f_Unpublish(NStr::CStr const &_Publication);
		NStr::CStr f_Subscribe(NStr::CStr const &_Namespace, mint _nExpected = 1);
		void f_SubscribeExpectFailure(NStr::CStr const &_Namespace);
		void f_Unsubscribe(NStr::CStr const &_Subscription);
		NStr::CStr const &f_GetHostID() const;

		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> f_GetRemoteActor(NStr::CStr const &_Subscription);
		
		TCActor<CActorDistributionManager> const &f_GetManager() const;
		
		void f_SetSecurity(CDistributedActorSecurity const &_Security);

		void f_Destroy();
		
	private:
		struct CPublication
		{
			TCDistributedActor<CActor> m_Actor;
			CDistributedActorPublication m_Publication;
		};
		struct CSubscription
		{
			~CSubscription()
			{
				m_pDeleted->f_Exchange(true);
			}
			NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> m_pDeleted = fg_Construct(false);
			CActorSubscription m_Subscription;
			NContainer::TCVector<CAbstractDistributedActor> m_RemoteActors;
		};
		
		NStr::CStr fp_Subscribe(NStr::CStr const &_Namespace, bool _bExpectFailure, mint _nExpected);
		
		NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> mp_pDeleted = fg_Construct(false);
		NStorage::TCSharedPointer<CRunLoop> mp_pRunLoop;
		NStr::CStr mp_HostID;
		TCActor<CActorDistributionManager> mp_Manager;
		NStorage::TCSharedPointer<NThread::CMutual> mp_pRemoteLock;
		NThread::CEventAutoReset mp_RemoteEvent;
		
		NContainer::TCMap<NStr::CStr, CSubscription> mp_Subscriptions;
		NContainer::TCMap<NStr::CStr, CPublication> mp_Publications;


		TCActor<CActor> mp_ProcessingActor = fg_ConcurrentActor();
	};
	
	struct CDistributedActorTestHelperCombined
	{
		CDistributedActorTestHelperCombined(NStr::CStr const &_ListenDirectory, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop);
		~CDistributedActorTestHelperCombined();
		void f_SeparateServerManager();
		void f_Init(bool _bReconnect = false);
		void f_InitClient(CDistributedActorTestHelperCombined &_Server, bool _bReconnect = false);
		void f_InitAnonymousClient(CDistributedActorTestHelperCombined &_Server);
		void f_InitServer();
		void f_DisconnectClient(bool _bNewExecutionID);
		NStr::CStr f_Subscribe(NStr::CStr const &_Namespace);
		void f_Unsubscribe(NStr::CStr const &_Subscription);
		template <typename ...tfp_CDistributedActors, typename tf_CActor>
		NStr::CStr f_Publish(TCDistributedActor<tf_CActor> const &_Actor, NStr::CStr const &_Namespace);
		void f_Unpublish(NStr::CStr const &_Publication);
		NStr::CStr f_GetListenSocketPath() const;
		
		NStr::CStr const &f_GetServerHostID() const;
		NStr::CStr const &f_GetClientHostID() const;
		
		CActorDistributionCryptographySettings const &f_GetClientCryptograhySettings() const;
		CActorDistributionCryptographySettings const &f_GetServerCryptograhySettings() const;

		void f_BreakClientConnection(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags);
		void f_BreakServerConnections(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags);

		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> f_GetRemoteActor(NStr::CStr const &_SubscriptionID);
		
		CDistributedActorTestHelper &f_GetServer();
		CDistributedActorTestHelper &f_GetClient();
		
	private:
		NStorage::TCSharedPointer<CRunLoop> mp_pRunLoop;

		NStorage::TCUniquePointer<CDistributedActorTestHelper> mp_pServer;
		NStorage::TCUniquePointer<CDistributedActorTestHelper> mp_pClient;
		
		CActorDistributionCryptographySettings mp_ServerCryptography;
		CActorDistributionListenSettings mp_ListenSettings;
		CDistributedActorListenReference mp_ListenReference;

		CActorDistributionCryptographySettings mp_ClientCryptography;
		CDistributedActorConnectionReference mp_ClientConnectionReference;
		NStr::CStr mp_ListenSocketPath;
	};

	struct CActorRunLoopTestHelper
	{
		~CActorRunLoopTestHelper();

		NStorage::TCSharedPointer<CDefaultRunLoop> m_pRunLoop = fg_Construct();
		TCActor<CDispatchingActor> m_HelperActor{fg_Construct(), m_pRunLoop->f_Dispatcher()};
		NStorage::TCOptional<CCurrentlyProcessingActorScope> m_CurrentActor{fg_Construct(m_HelperActor)};
	};
}

#include "Malterlib_Concurrency_DistributedActor_TestHelpers.hpp"
