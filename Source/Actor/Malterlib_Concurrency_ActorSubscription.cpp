// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_Concurrency_ActorSubscription.h"

namespace NMib::NConcurrency
{
	constexpr CActorSubscriptionHelper g_ActorSubscriptionInit{};
	CActorSubscriptionHelper const &g_ActorSubscription = g_ActorSubscriptionInit;

	constexpr CBlockingActorSubscriptionHelper g_BlockingActorSubscriptionInit;
	CBlockingActorSubscriptionHelper const &g_BlockingActorSubscription = g_BlockingActorSubscriptionInit;

	template <typename t_CReturnValue>
	struct TCActorSubscriptionCleanupFunctor : public CActorSubscriptionReference
	{
		TCActorSubscriptionCleanupFunctor(TCActorSubscriptionCleanupFunctor const &) = delete;
		TCActorSubscriptionCleanupFunctor &operator = (TCActorSubscriptionCleanupFunctor const &) = delete;
		TCActorSubscriptionCleanupFunctor(TCActorSubscriptionCleanupFunctor &&) = delete;
		TCActorSubscriptionCleanupFunctor &operator = (TCActorSubscriptionCleanupFunctor &&) = delete;
		TCActorSubscriptionCleanupFunctor(TCActor<> const &_DispatchActor, NFunction::TCFunctionMovable<t_CReturnValue ()> &&_fCleanup);
		~TCActorSubscriptionCleanupFunctor();

		TCFuture<void> f_Destroy() override;

	private:
		TCWeakActor<> mp_DispatchActor;
		NFunction::TCFunctionMovable<t_CReturnValue ()> mp_fCleanup;
	};

	template <typename t_CReturnValue>
	TCActorSubscriptionCleanupFunctor<t_CReturnValue>::TCActorSubscriptionCleanupFunctor
		(
			TCActor<> const &_DispatchActor
			, NFunction::TCFunctionMovable<t_CReturnValue ()> &&_fCleanup
		)
		: mp_fCleanup(fg_Move(_fCleanup))
		, mp_DispatchActor(_DispatchActor)
	{
	}

	template <typename t_CReturnValue>
	TCActorSubscriptionCleanupFunctor<t_CReturnValue>::~TCActorSubscriptionCleanupFunctor()
	{
		if (!mp_DispatchActor)
			return;
		fg_Dispatch(mp_DispatchActor, fg_Move(mp_fCleanup)).f_DiscardResult();
	}

	template <typename t_CReturnValue>
	TCFuture<void> TCActorSubscriptionCleanupFunctor<t_CReturnValue>::f_Destroy()
	{
		using namespace NStr;

		if (!mp_DispatchActor)
			return g_Void;

		auto pDispatchActor = fg_Move(mp_DispatchActor);
		auto pLockedActor = pDispatchActor.f_Lock();
		if (!pLockedActor)
		{
			mp_fCleanup.f_Clear();
			return DMibErrorInstance("Actor in subscription has been deleted");
		}

		return fg_Dispatch(pLockedActor, fg_Move(mp_fCleanup));
	}

	CActorSubscription fg_ActorSubscription(TCActor<> const &_DispatchActor, NFunction::TCFunctionMovable<void ()> &&_fCleanup)
	{
		NStorage::TCUniquePointer<TCActorSubscriptionCleanupFunctor<void>> pFunctorRef = fg_Construct(_DispatchActor, fg_Move(_fCleanup));
		return fg_Move(pFunctorRef);
	}

	CActorSubscription fg_ActorSubscriptionAsync(TCActor<> const &_DispatchActor, NFunction::TCFunctionMovable<TCFuture<void> ()> &&_fCleanup)
	{
		NStorage::TCUniquePointer<TCActorSubscriptionCleanupFunctor<TCFuture<void>>> pFunctorRef = fg_Construct(_DispatchActor, fg_Move(_fCleanup));
		return fg_Move(pFunctorRef);
	}
}
