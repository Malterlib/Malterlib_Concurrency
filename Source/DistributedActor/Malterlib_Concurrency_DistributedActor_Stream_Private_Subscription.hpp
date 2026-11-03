// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <uint32 tf_SubscriptionID>
	// NOLINTNEXTLINE(malterlib-conceptual-prefix): a subscription handle like CActorSubscription, which the check misses because it is derived from it
	void CDistributedActorReadStream::f_Consume(TCActorSubscriptionWithID<tf_SubscriptionID> &_Subscription)
	{
		f_Consume(_Subscription, _Subscription.f_GetID());
	}

	template <uint32 tf_SubscriptionID>
	// NOLINTNEXTLINE(malterlib-conceptual-prefix): a subscription handle like CActorSubscription, which the check misses because it is derived from it
	void CDistributedActorWriteStream::f_Feed(TCActorSubscriptionWithID<tf_SubscriptionID> &&_Subscription)
	{
		f_Feed(fg_Move(_Subscription), _Subscription.f_GetID());
	}
}
