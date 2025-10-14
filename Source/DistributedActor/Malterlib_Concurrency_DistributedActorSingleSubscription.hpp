// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	TCDistributedActorSingleSubscription<t_CActor>::TCDistributedActorSingleSubscription(CFilter const &_Filter, TCActor<CActorDistributionManager> const &_DistributionManager)
		: mp_Filter(_Filter)
		, mp_DistributionManager(_DistributionManager)
	{
		DMibCheck(fg_CurrentActor() == fg_ThisActor(this));
		mp_DistributionManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, mp_Filter.m_Namespace
				, fg_CurrentActor()
				, [this](CAbstractDistributedActor _NewActor) -> TCFuture<void>
				{
					if (mp_Filter.m_HostID && _NewActor.f_GetHostInfo().m_HostID != mp_Filter.m_HostID)
						co_return {};

					mp_DistributedActor = {};
					auto Actor = _NewActor.f_GetActor<t_CActor>();
					if (!Actor)
					{
						mp_DistributedActor.f_SetException(DMibErrorInstance("Command line actor has wrong type"));
						fp_ResultAvailable();
						co_return {};
					}
					mp_DistributedActor.f_SetResult(fg_Move(Actor));
					fp_ResultAvailable();

					co_return {};
				}
				, [this](CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
				{
					if (mp_DistributedActor && *mp_DistributedActor == _RemovedActor)
						mp_DistributedActor = {};

					co_return {};
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
			co_return mp_DistributedActor;

		co_return co_await mp_GetActorPromises.f_Insert().f_Future();
	}

	template <typename t_CActor>
	void TCDistributedActorSingleSubscription<t_CActor>::fp_ResultAvailable()
	{
		for (auto &Promise : mp_GetActorPromises)
			Promise.f_SetResult(mp_DistributedActor);

		mp_GetActorPromises.f_Clear();
	}
}
