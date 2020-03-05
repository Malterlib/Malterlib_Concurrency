// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor_TestHelpers.h"

namespace NMib::NConcurrency
{
	template <typename ...tfp_CDistributedActors, typename tf_CActor>
	NStr::CStr CDistributedActorTestHelperCombined::f_Publish(TCDistributedActor<tf_CActor> const &_Actor, NStr::CStr const &_Namespace)
	{
		if (!mp_pServer)
			DMibError("Server not there");
		return mp_pServer->template f_Publish<tfp_CDistributedActors...>(_Actor, _Namespace);
	}	
	
	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CDistributedActorTestHelperCombined::f_GetRemoteActor(NStr::CStr const &_SubscriptionID)
	{
		if (!mp_pClient)
			DMibError("Client not connected");
		return mp_pClient->f_GetRemoteActor<tf_CActor>(_SubscriptionID);
	}

	template <typename ...tfp_CDistributedActors, typename tf_CActor>
	NStr::CStr CDistributedActorTestHelper::f_Publish(TCDistributedActor<tf_CActor> const &_Actor, NStr::CStr const &_Namespace)
	{
		auto PublicationRef = _Actor->template f_Publish<tfp_CDistributedActors...>(_Namespace).f_CallSync(60.0);
		
		NStr::CStr PublicationID = NCryptography::fg_RandomID(mp_Publications);

		auto &Publication = mp_Publications[PublicationID];
		Publication.m_Publication = fg_Move(PublicationRef);
		Publication.m_Actor = _Actor;
		return PublicationID;
	}
	
	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CDistributedActorTestHelper::f_GetRemoteActor(NStr::CStr const &_Subscription)
	{
		auto pSubscription = mp_Subscriptions.f_FindEqual(_Subscription);
		if (!pSubscription)
			DMibError("No such subscription");
		DMibLock(*mp_pRemoteLock);
		for (auto &RemoteActor : pSubscription->m_RemoteActors)
		{
			auto Actor = RemoteActor.f_GetActor<tf_CActor>();
			if (Actor)
				return Actor;
		}
		return {};
	}
}
