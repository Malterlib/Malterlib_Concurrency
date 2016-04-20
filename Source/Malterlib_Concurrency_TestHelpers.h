// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/WeakActor>

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedActorTestHelper
		{
			CDistributedActorTestHelper();
			void f_SeparateServerManager();
			void f_Init();
			void f_Subscribe(NStr::CStr const &_Namespace);
			void f_Unsubscribe();
			template <typename ...tfp_CDistributedActors>
			void f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace);
			void f_Unpublish();
			
			template <typename tf_CActor>
			TCDistributedActor<tf_CActor> f_GetRemoteActor();
			
		private:
			TCActor<CActorDistributionManager> mp_ServerManager;
			TCActor<CActorDistributionManager> mp_ClientManager;

			NThread::CMutual mp_RemoteLock;
			NThread::CEventAutoReset mp_RemoteEvent;
			CActorCallback mp_RemoteActorsSubscription;
			
			NContainer::TCVector<TCDistributedActor<CActor>> mp_PublishedActors;
			TCDistributedActor<CActor> mp_RemoteActor;
			
			CDistributedActorPublication mp_Publication;

			mint mp_RemoteEvents = 0;
		};
	}
}

#include "Malterlib_Concurrency_TestHelpers.hpp"