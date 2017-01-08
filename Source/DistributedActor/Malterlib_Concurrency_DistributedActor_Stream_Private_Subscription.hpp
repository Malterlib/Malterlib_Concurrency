// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <uint32 tf_SubscriptionID>
	void CDistributedActorReadStream::f_Consume(TCActorSubscriptionWithID<tf_SubscriptionID> &_Subscription)
	{
		f_Consume(_Subscription, _Subscription.f_GetID());
	}
	
	template <uint32 tf_SubscriptionID>
	void CDistributedActorWriteStream::f_Feed(TCActorSubscriptionWithID<tf_SubscriptionID> &&_Subscription)
	{
		f_Feed(fg_Move(_Subscription), _Subscription.f_GetID());
	}
}
