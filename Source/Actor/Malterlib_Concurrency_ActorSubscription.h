// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/WeakActor>

namespace NMib::NConcurrency
{
	CActorSubscription fg_ActorSubscription(TCActor<> const &_DispatchActor, NFunction::TCFunctionMovable<void ()> &&_fCleanup);
	CActorSubscription fg_ActorSubscriptionAsync(TCActor<> const &_DispatchActor, NFunction::TCFunctionMovable<TCFuture<void> ()> &&_fCleanup);

	struct CActorSubscriptionHelperWithActor
	{
		inline CActorSubscriptionHelperWithActor(TCActor<> const &_Actor);
		
		template
		<
			typename tf_FCleanup
			, TCEnableIfType
			<
				NTraits::TCIsSame
				<
					typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>::CReturnType
					, TCFuture<void>
				>::mc_Value
			>
			* = nullptr
		>
		inline CActorSubscription operator / (tf_FCleanup &&_fCleanup) const;
		
		template
		<
			typename tf_FCleanup
			, TCEnableIfType
			<
				!NTraits::TCIsSame
				<
					typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>::CReturnType
					, TCFuture<void>
				>::mc_Value
			>
			* = nullptr
		>
		inline CActorSubscription operator / (tf_FCleanup &&_fCleanup) const;
		
	private:
		TCActor<> const &mp_Actor;
	};
	
	struct CActorSubscriptionHelper
	{
		template
		<
			typename tf_FCleanup
			, TCEnableIfType
			<
				NTraits::TCIsSame
				<
					typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>::CReturnType
					, TCFuture<void>
				>::mc_Value
			>
			* = nullptr
		>
		inline CActorSubscription operator / (tf_FCleanup &&_fCleanup) const;

		template
		<
			typename tf_FCleanup
			, TCEnableIfType
			<
				!NTraits::TCIsSame
				<
					typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReferenceAndQualifiers<tf_FCleanup>::CType, void ()>::CReturnType
					, TCFuture<void>
				>::mc_Value
			>
			* = nullptr
		>
		inline CActorSubscription operator / (tf_FCleanup &&_fCleanup) const;
		
		inline CActorSubscriptionHelperWithActor operator ()(TCActor<> const &_Actor) const;
	};
	
	extern CActorSubscriptionHelper const &g_ActorSubscription;
}

#include "Malterlib_Concurrency_ActorSubscription.hpp"
