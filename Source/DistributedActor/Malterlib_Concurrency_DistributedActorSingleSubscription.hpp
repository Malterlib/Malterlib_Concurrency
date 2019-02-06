// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	TCDistributedActorSingleSubscription<t_CActor>::TCDistributedActorSingleSubscription(NStr::CStr const &_Namespace, TCActor<CActorDistributionManager> const &_DistributionManager)
		: mp_Namespace(_Namespace)
		, mp_DistributionManager(_DistributionManager)
	{
		DMibCheck(fg_CurrentActor() == fg_ThisActor(this));
		mp_DistributionManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, NContainer::fg_CreateVector(mp_Namespace)
				, fg_CurrentActor()
				, [this](CAbstractDistributedActor &&_NewActor)
				{
					mp_DistributedActor = {};
					auto Actor = _NewActor.f_GetActor<t_CActor>();
					if (!Actor)
					{
						mp_DistributedActor.f_SetException(DMibErrorInstance("Command line actor has wrong type"));
						fp_ResultAvailable();
						return;
					}
					mp_DistributedActor.f_SetResult(fg_Move(Actor));
					fp_ResultAvailable();
				}
				, [this](const CDistributedActorIdentifier &_RemovedActor)
				{
					if (mp_DistributedActor && *mp_DistributedActor == _RemovedActor)
						mp_DistributedActor = {};
				}
			)
			> [this](TCAsyncResult<CActorSubscription> &&_Subscription)
			{
				if (!_Subscription)
				{
					mp_DistributedActor.f_SetException(fg_Move(_Subscription));
					fp_ResultAvailable();
					return;
				}
				mp_DistributedActorSubscription = fg_Move(*_Subscription);
			}
		;
	}

	template <typename t_CActor>
	TCFuture<TCDistributedActor<t_CActor>> TCDistributedActorSingleSubscription<t_CActor>::f_GetActor()
	{
		if (mp_DistributedActor.f_IsSet())
			return mp_DistributedActor;

		TCPromise<TCDistributedActor<t_CActor>> Promise;
		mp_GetActorPromises.f_Insert(Promise);
		return Promise.f_MoveFuture();
	}

	template <typename t_CActor>
	void TCDistributedActorSingleSubscription<t_CActor>::fp_ResultAvailable()
	{
		for (auto &Promise : mp_GetActorPromises)
			Promise.f_SetResult(mp_DistributedActor);

		mp_GetActorPromises.f_Clear();
	}
}
