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
			CDistributedActorTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager);
			~CDistributedActorTestHelper();
			
			template <typename ...tfp_CDistributedActors>
			void f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace);
			void f_Unpublish();
			void f_Subscribe(NStr::CStr const &_Namespace);
			void f_Unsubscribe();
			NStr::CStr const &f_GetHostID() const;

			template <typename tf_CActor>
			TCDistributedActor<tf_CActor> f_GetRemoteActor();
			
			TCActor<CActorDistributionManager> const &f_GetManager() const;
		private:
			NStr::CStr mp_HostID;
			TCActor<CActorDistributionManager> mp_Manager;
			NThread::CMutual mp_RemoteLock;
			NThread::CEventAutoReset mp_RemoteEvent;
			CActorCallback mp_RemoteActorsSubscription;
			NContainer::TCVector<TCDistributedActor<CActor>> mp_PublishedActors;
			TCDistributedActor<CActor> mp_RemoteActor;
			
			CDistributedActorPublication mp_Publication;
		};
		
		struct CDistributedActorTestHelperCombined
		{
			CDistributedActorTestHelperCombined();
			~CDistributedActorTestHelperCombined();
			void f_SeparateServerManager();
			void f_Init();
			void f_InitClient(CDistributedActorTestHelperCombined &_Server);
			void f_InitServer();
			void f_Subscribe(NStr::CStr const &_Namespace);
			void f_Unsubscribe();
			template <typename ...tfp_CDistributedActors>
			void f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace);
			void f_Unpublish();
			
			NStr::CStr const &f_GetServerHostID() const;
			NStr::CStr const &f_GetClientHostID() const;
			
			CActorDistributionCryptographySettings const &f_GetClientCryptograhySettings() const;
			CActorDistributionCryptographySettings const &f_GetServerCryptograhySettings() const;
			
			template <typename tf_CActor>
			TCDistributedActor<tf_CActor> f_GetRemoteActor();
			
		private:
			NPtr::TCUniquePointer<CDistributedActorTestHelper> mp_pServer;
			NPtr::TCUniquePointer<CDistributedActorTestHelper> mp_pClient;
			
			CActorDistributionCryptographySettings mp_ServerCryptography;
			CActorDistributionListenSettings mp_ListenSettings;

			CActorDistributionCryptographySettings mp_ClientCryptography;
		};
	}
}

#include "Malterlib_Concurrency_TestHelpers.hpp"