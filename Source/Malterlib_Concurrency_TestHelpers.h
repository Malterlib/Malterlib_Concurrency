// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/WeakActor>

namespace NMib
{
	namespace NConcurrency
	{
		class CDistributedActorTrustManager;
		
		struct CDistributedActorTestHelper
		{
			CDistributedActorTestHelper(NStr::CStr const &_HostID, TCActor<CActorDistributionManager> const &_Manager);
			CDistributedActorTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager, bool _bAllowUnconnected = false);
			~CDistributedActorTestHelper();
			
			template <typename ...tfp_CDistributedActors>
			NStr::CStr f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace);
			void f_Unpublish(NStr::CStr const &_Publication);
			NStr::CStr f_Subscribe(NStr::CStr const &_Namespace);
			void f_SubscribeExpectFailure(NStr::CStr const &_Namespace);
			void f_Unsubscribe(NStr::CStr const &_Subscription);
			NStr::CStr const &f_GetHostID() const;

			template <typename tf_CActor>
			TCDistributedActor<tf_CActor> f_GetRemoteActor(NStr::CStr const &_Subscription);
			
			TCActor<CActorDistributionManager> const &f_GetManager() const;
			
			void f_SetSecurity(CDistributedActorSecurity const &_Security);
			
		private:
			struct CPublication
			{
				TCDistributedActor<CActor> m_Actor;
				CDistributedActorPublication m_Publication;
			};
			struct CSubscription
			{
				CActorSubscription m_Subscription;
				NContainer::TCVector<CAbstractDistributedActor> m_RemoteActors;
			};
			
			NStr::CStr fp_Subscribe(NStr::CStr const &_Namespace, bool _bExpectFailure);
			
			NStr::CStr mp_HostID;
			TCActor<CActorDistributionManager> mp_Manager;
			NThread::CMutual mp_RemoteLock;
			NThread::CEventAutoReset mp_RemoteEvent;
			
			NContainer::TCMap<NStr::CStr, CSubscription> mp_Subscriptions;
			NContainer::TCMap<NStr::CStr, CPublication> mp_Publications;
		};
		
		struct CDistributedActorTestHelperCombined
		{
			CDistributedActorTestHelperCombined(uint16 _TestPort);
			~CDistributedActorTestHelperCombined();
			void f_SeparateServerManager();
			void f_Init();
			void f_InitClient(CDistributedActorTestHelperCombined &_Server);
			void f_InitAnonymousClient(CDistributedActorTestHelperCombined &_Server);
			void f_InitServer();
			void f_DisconnectClient(bool _bNewExecutionID);
			NStr::CStr f_Subscribe(NStr::CStr const &_Namespace);
			void f_Unsubscribe(NStr::CStr const &_Subscription);
			template <typename ...tfp_CDistributedActors>
			NStr::CStr f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace);
			void f_Unpublish(NStr::CStr const &_Publication);
			
			NStr::CStr const &f_GetServerHostID() const;
			NStr::CStr const &f_GetClientHostID() const;
			
			CActorDistributionCryptographySettings const &f_GetClientCryptograhySettings() const;
			CActorDistributionCryptographySettings const &f_GetServerCryptograhySettings() const;
			
			template <typename tf_CActor>
			TCDistributedActor<tf_CActor> f_GetRemoteActor(NStr::CStr const &_SubscriptionID);
			
			CDistributedActorTestHelper &f_GetServer();
			CDistributedActorTestHelper &f_GetClient();
			
		private:
			NPtr::TCUniquePointer<CDistributedActorTestHelper> mp_pServer;
			NPtr::TCUniquePointer<CDistributedActorTestHelper> mp_pClient;
			
			CActorDistributionCryptographySettings mp_ServerCryptography;
			CActorDistributionListenSettings mp_ListenSettings;
			CDistributedActorListenReference mp_ListenReference;

			CActorDistributionCryptographySettings mp_ClientCryptography;
			CDistributedActorConnectionReference mp_ClientConnectionReference;
			
			uint16 mp_ListenPort;
		};
	}
}

#include "Malterlib_Concurrency_TestHelpers.hpp"
