// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_TestHelpers.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename ...tfp_CDistributedActors>
		NStr::CStr CDistributedActorTestHelperCombined::f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace)
		{
			if (!mp_pServer)
				DMibError("Server not there");
			return mp_pServer->f_Publish<tfp_CDistributedActors...>(_Actor, _Namespace);
		}	
		
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> CDistributedActorTestHelperCombined::f_GetRemoteActor(NStr::CStr const &_SubscriptionID)
		{
			if (!mp_pClient)
				DMibError("Client not corrected");
			return mp_pClient->f_GetRemoteActor<tf_CActor>(_SubscriptionID);
		}

		template <typename ...tfp_CDistributedActors>
		NStr::CStr CDistributedActorTestHelper::f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace)
		{
			NStr::CStr PublicationID = NCryptography::fg_RandomID();
			auto &Publication = mp_Publications[PublicationID];
			Publication.m_Actor = _Actor;
			Publication.m_Publication = mp_Manager
				(
					&CActorDistributionManager::f_PublishActor
					, fg_TempCopy(_Actor)
					, _Namespace
					, NMib::NConcurrency::CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy<tfp_CDistributedActors...>()
				).f_CallSync(60.0)
			;
			
			return PublicationID;
		}
		
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> CDistributedActorTestHelper::f_GetRemoteActor(NStr::CStr const &_Subscription)
		{
			auto pSubscription = mp_Subscriptions.f_FindEqual(_Subscription);
			if (!pSubscription)
				DMibError("No such subscription");
			DMibLock(mp_RemoteLock);
			for (auto &RemoteActor : pSubscription->m_RemoteActors)
			{
				auto Actor = RemoteActor.f_GetActor<tf_CActor>();
				if (Actor)
					return Actor;
			}
			return {};
		}
	}
}
