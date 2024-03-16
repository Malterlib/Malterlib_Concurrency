// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline CActorSubscriptionHelperWithActor::CActorSubscriptionHelperWithActor(TCActor<> const &_Actor)
		: mp_Actor(_Actor)
	{
	}

	inline CActorSubscriptionHelperWithActor CActorSubscriptionHelper::operator ()(TCActor<> const &_Actor) const
	{
		return CActorSubscriptionHelperWithActor(_Actor); 
	}
	
	template
	<
		typename tf_FCleanup
		, TCEnableIfType
		<
			NTraits::TCIsSame
			<
				NTraits::TCCallableReturnTypeFor<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>
				, TCFuture<void>
			>::mc_Value
		>
		*
	>
	inline CActorSubscription CActorSubscriptionHelperWithActor::operator / (tf_FCleanup &&_fCleanup) const
	{ 
		return fg_ActorSubscriptionAsync(mp_Actor, fg_Forward<tf_FCleanup>(_fCleanup));
	}
	
	template
	<
		typename tf_FCleanup
		, TCEnableIfType
		<
			!NTraits::TCIsSame
			<
				NTraits::TCCallableReturnTypeFor<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>
				, TCFuture<void>
			>::mc_Value
		>
		*
	>
	inline CActorSubscription CActorSubscriptionHelperWithActor::operator / (tf_FCleanup &&_fCleanup) const
	{ 
		return fg_ActorSubscription(mp_Actor, fg_Forward<tf_FCleanup>(_fCleanup)); 
	}
	
	template
	<
		typename tf_FCleanup
		, TCEnableIfType
		<
			NTraits::TCIsSame
			<
				NTraits::TCCallableReturnTypeFor<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>
				, TCFuture<void>
			>::mc_Value
		>
		*
	>
	inline CActorSubscription CActorSubscriptionHelper::operator / (tf_FCleanup &&_fCleanup) const
	{ 
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		return fg_ActorSubscriptionAsync(CurrentActor, fg_Forward<tf_FCleanup>(_fCleanup));
	}

	template
	<
		typename tf_FCleanup
		, TCEnableIfType
		<
			!NTraits::TCIsSame
			<
				NTraits::TCCallableReturnTypeFor<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>
				, TCFuture<void>
			>::mc_Value
		>
		*
	>
	inline CActorSubscription CActorSubscriptionHelper::operator / (tf_FCleanup &&_fCleanup) const
	{ 
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		return fg_ActorSubscription(CurrentActor, fg_Forward<tf_FCleanup>(_fCleanup)); 
	}

	template <typename tf_FCleanup>
	inline CActorSubscription CBlockingActorSubscriptionHelper::operator / (tf_FCleanup &&_fCleanup) const
	{
		return g_ActorSubscription(fg_ThisConcurrentActor()) / [fOnExitScope = fg_Forward<tf_FCleanup>(_fCleanup)]() mutable -> TCFuture<void>
			{
				auto BlockingActorCheckout = fg_BlockingActor();
				co_await (g_Dispatch(BlockingActorCheckout) / fg_Move(fOnExitScope));

				co_return {};
			}
		;
	}
}
