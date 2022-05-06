// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#if 0
#define DTestHelpersDebug DMibConErrOut2
#else
#define DTestHelpersDebug(...)
#endif

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CTrustedSubscriptionTestHelper::f_SubscribeFromHost(NStr::CStr const &_HostID, NStr::CStr const &_Namespace)
	{
		auto Subscriptions = mp_Internal(&CInternal::f_Subscribe<tf_CActor>, 1, _Namespace, _HostID).f_CallSync(mp_Timeout);
		return Subscriptions[0];
	}

	template <typename tf_CActor>
	TCDistributedActor<tf_CActor> CTrustedSubscriptionTestHelper::f_Subscribe(NStr::CStr const &_Namespace, NStr::CStr const &_HostID)
	{
		auto Subscriptions = mp_Internal(&CInternal::f_Subscribe<tf_CActor>, 1, _Namespace, _HostID).f_CallSync(mp_Timeout);
		return Subscriptions[0];
	}

	template <typename tf_CActor>
	TCFuture<TCDistributedActor<tf_CActor>> CTrustedSubscriptionTestHelper::f_SubscribeAsync(NStr::CStr _Namespace, NStr::CStr _HostID)
	{
		using namespace NStr;
		auto Subscriptions = co_await mp_Internal(&CInternal::f_Subscribe<tf_CActor>, 1, _Namespace, _HostID).f_Timeout(mp_Timeout, "Timed out subscribing to actors {}"_f << _Namespace);
		co_return Subscriptions[0];
	}

	template <typename tf_CActor>
	NContainer::TCVector<TCDistributedActor<tf_CActor>> CTrustedSubscriptionTestHelper::f_SubscribeMultiple(mint _nActors, NStr::CStr const &_Namespace, NStr::CStr const &_HostID)
	{
		return mp_Internal(&CInternal::f_Subscribe<tf_CActor>, _nActors, _Namespace, _HostID).f_CallSync(mp_Timeout);
	}

	template <typename tf_CActor>
	auto CTrustedSubscriptionTestHelper::f_SubscribeMultipleAsync(mint _nActors, NStr::CStr _Namespace, NStr::CStr _HostID)
		-> TCFuture<NContainer::TCVector<TCDistributedActor<tf_CActor>>>
	{
		using namespace NStr;
		co_return co_await mp_Internal(&CInternal::f_Subscribe<tf_CActor>, _nActors, _Namespace, _HostID)
			.f_Timeout(mp_Timeout, "Timed out subscribing to multiple actors {}"_f << _Namespace)
		;
	}

	template <typename tf_CActor>
	auto CTrustedSubscriptionTestHelper::CInternal::f_Subscribe(mint _nActors, NStr::CStr const &_Namespace, NStr::CStr const &_HostID)
		-> TCFuture<NContainer::TCVector<TCDistributedActor<tf_CActor>>>
	{
		TCPromise<NContainer::TCVector<TCDistributedActor<tf_CActor>>> Promise;
		mp_TrustManager(&CDistributedActorTrustManager::f_SubscribeTrustedActors<tf_CActor>, _Namespace, fg_ThisActor(this), 0, TCLimitsInt<uint32>::mc_Max)
			> Promise / [this, Promise, _nActors, _HostID, _Namespace](TCTrustedActorSubscription<tf_CActor> &&_Subscription)
			{
				struct CSubscriptionImpl : CSubscription
				{
					TCTrustedActorSubscription<tf_CActor> m_Subscription;
				};
				NStorage::TCUniquePointer<CSubscriptionImpl> pSubscription = fg_Construct<CSubscriptionImpl>();
				pSubscription->m_Subscription = fg_Move(_Subscription);
				pSubscription->m_Subscription.f_OnActor
					(
						g_ActorFunctor / [this, Promise, _nActors, Actors = NContainer::TCVector<TCDistributedActor<tf_CActor>>(), _HostID, _Namespace]
						(TCDistributedActor<tf_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo) mutable -> TCFuture<void>
						{
							if (Promise.f_IsSet())
								co_return {};

							if (!_HostID || _ActorInfo.m_HostInfo.m_HostID == _HostID)
							{
								if (!mp_SeenActors.f_Exists(fg_GetRemoteActorID(_NewActor)))
								{
									Actors.f_Insert(_NewActor);
									DTestHelpersDebug("INSERT {}\n", _NewActor);
								}
								else
									DTestHelpersDebug("SEEN {}\n", _NewActor);
							}
							else
								DTestHelpersDebug("IGNORE {} {} != {}\n", _NewActor, _ActorInfo.m_HostInfo.m_HostID, _HostID);

							if (Actors.f_GetLen() == _nActors)
							{
								for (auto &Actor : Actors)
									mp_SeenActors[fg_GetRemoteActorID(Actor)];

								DTestHelpersDebug("RESULT {vs}\n", Actors);
								Promise.f_SetResult(fg_Move(Actors));
							}

							co_return {};
						}
						, nullptr
					)
					> fg_DiscardResult()
				;
				mp_Subscriptions.f_Insert(fg_Move(pSubscription));
			}
		;
		return Promise.f_MoveFuture();
	}
}
