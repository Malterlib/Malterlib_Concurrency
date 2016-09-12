// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_Concurrency_ActorCallbackManager.h"

namespace NMib::NConcurrency
{
	struct CActorSubscriptionCleanupFunctor : public CActorSubscriptionReference
	{
		CActorSubscriptionCleanupFunctor(CActorSubscriptionCleanupFunctor const &) = delete;
		CActorSubscriptionCleanupFunctor &operator = (CActorSubscriptionCleanupFunctor const &) = delete;
		CActorSubscriptionCleanupFunctor(CActorSubscriptionCleanupFunctor &&) = delete;
		CActorSubscriptionCleanupFunctor &operator = (CActorSubscriptionCleanupFunctor &&) = delete;
		CActorSubscriptionCleanupFunctor(TCActor<> const &_DispatchActor, NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fCleanup);
		~CActorSubscriptionCleanupFunctor();
	private:
		TCWeakActor<> mp_DispatchActor;
		NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> mp_fCleanup;
	};
	
	CActorSubscriptionCleanupFunctor::CActorSubscriptionCleanupFunctor
		(
			TCActor<> const &_DispatchActor
			, NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fCleanup
		)
		: mp_fCleanup(fg_Move(_fCleanup))
		, mp_DispatchActor(_DispatchActor)
	{
	}
	
	CActorSubscriptionCleanupFunctor::~CActorSubscriptionCleanupFunctor()
	{
		fg_Dispatch(mp_DispatchActor, fg_Move(mp_fCleanup)) > fg_DiscardResult();
	}
	
	CActorSubscription fg_ActorSubscription(TCActor<> const &_DispatchActor, NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fCleanup)
	{
		NPtr::TCUniquePointer<CActorSubscriptionCleanupFunctor> pFunctorRef = fg_Construct(_DispatchActor, fg_Move(_fCleanup));
		return fg_Move(pFunctorRef);
	}		
}
