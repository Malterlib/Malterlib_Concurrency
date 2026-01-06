// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor, uint32 tf_SubscriptionID>
	void CDistributedActorWriteStream::f_Feed(TCDistributedActorInterfaceWithID<tf_CActor, tf_SubscriptionID> &&_ActorInterface)
	{
		f_FeedInterface
			(
				_ActorInterface.f_GetID()
				, fg_Move(_ActorInterface.f_GetSubscription())
				, _ActorInterface
				, fg_Move(_ActorInterface.f_GetHierarchy())
				, fg_Move(_ActorInterface.f_GetProtocolVersions())
			)
		;
	}

	template <typename tf_CActor, uint32 tf_SubscriptionID>
	void CDistributedActorReadStream::f_Consume(TCDistributedActorInterfaceWithID<tf_CActor, tf_SubscriptionID> &_ActorInterface)
	{
		uint32 SubscriptionID;

		(TCDistributedActor<> &)_ActorInterface = f_ConsumeInterface
			(
				DMibConstantTypeHash(tf_CActor)
				, fg_SubscribeVersions<tf_CActor>()
				, _ActorInterface.f_GetHierarchy()
				, _ActorInterface.f_GetProtocolVersions()
				, _ActorInterface.f_GetSubscription()
				, SubscriptionID
			)
		;

		_ActorInterface.f_SetID(SubscriptionID);
	}
}
