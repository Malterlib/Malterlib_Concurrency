// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CTrustedSubscriptionTestHelper::f_Subscribe(NStr::CStr const &_Namespace)
	{
		auto Subscriptions = mp_Internal(&CInternal::f_Subscribe<tf_CActor>, 1, _Namespace).f_CallSync(30.0);
		return Subscriptions[0];
	}
	
	template <typename tf_CActor>
	NContainer::TCVector<TCDistributedActor<tf_CActor>> CTrustedSubscriptionTestHelper::f_SubscribeMultiple(mint _nActors, NStr::CStr const &_Namespace)
	{
		return mp_Internal(&CInternal::f_Subscribe<tf_CActor>, _nActors, _Namespace).f_CallSync(30.0);
	}
	
	template <typename tf_CActor>
	TCContinuation<NContainer::TCVector<TCDistributedActor<tf_CActor>>> CTrustedSubscriptionTestHelper::CInternal::f_Subscribe(mint _nActors, NStr::CStr const &_Namespace)
	{
		TCContinuation<NContainer::TCVector<TCDistributedActor<tf_CActor>>> Continuation;
		mp_TrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<tf_CActor>, _Namespace, fg_ThisActor(this)) 
			> Continuation / [this, Continuation, _nActors](TCTrustedActorSubscription<tf_CActor> &&_Subscription)
			{
				struct CSubscriptionImpl : CSubscription
				{
					TCTrustedActorSubscription<tf_CActor> m_Subscription;
				};
				NStorage::TCUniquePointer<CSubscriptionImpl> pSubscription = fg_Construct<CSubscriptionImpl>();
				pSubscription->m_Subscription = fg_Move(_Subscription);
				pSubscription->m_Subscription.f_OnActor
					(
						[Continuation, _nActors, Actors = NContainer::TCVector<TCDistributedActor<tf_CActor>>()]
						(TCDistributedActor<tf_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo) mutable
						{
							Actors.f_Insert(_NewActor);
							if (Actors.f_GetLen() == _nActors && !Continuation.f_IsSet())
								Continuation.f_SetResult(fg_Move(Actors));
						}
					)
				;
				mp_Subscriptions.f_Insert(fg_Move(pSubscription));
			}
		;
		return Continuation;
	}
}
