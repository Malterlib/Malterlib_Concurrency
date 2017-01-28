// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CTrustedSubscriptionTestHelper::f_Subscribe(NStr::CStr const &_Namespace)
	{
		return mp_Internal(&CInternal::f_Subscribe<tf_CActor>, _Namespace).f_CallSync(30.0);
	}
	
	template <typename tf_CActor>
	TCContinuation<TCDistributedActor<tf_CActor>> CTrustedSubscriptionTestHelper::CInternal::f_Subscribe(NStr::CStr const &_Namespace)
	{
		TCContinuation<TCDistributedActor<tf_CActor>> Continuation;
		mp_TrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<tf_CActor>, _Namespace, fg_ThisActor(this)) 
			> Continuation / [this, Continuation](TCTrustedActorSubscription<tf_CActor> &&_Subscription)
			{
				struct CSubscriptionImpl : CSubscription
				{
					TCTrustedActorSubscription<tf_CActor> m_Subscription;
				};
				NPtr::TCUniquePointer<CSubscriptionImpl> pSubscription = fg_Construct<CSubscriptionImpl>();
				pSubscription->m_Subscription = fg_Move(_Subscription);
				pSubscription->m_Subscription.f_OnActor
					(
						[Continuation](TCDistributedActor<tf_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)
						{
							if (!Continuation.f_IsSet())
								Continuation.f_SetResult(_NewActor);
						}
					)
				;
				mp_Subscriptions.f_Insert(fg_Move(pSubscription));
			}
		;
		return Continuation;
	}
}
