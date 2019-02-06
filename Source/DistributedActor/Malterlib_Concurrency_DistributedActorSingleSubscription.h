// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	struct TCDistributedActorSingleSubscription : public CActor
	{
		TCDistributedActorSingleSubscription(NStr::CStr const &_Namespace, TCActor<CActorDistributionManager> const &_DistributionManager = fg_GetDistributionManager());
		void f_Construct();

		TCFuture<TCDistributedActor<t_CActor>> f_GetActor();

	private:
		void fp_ResultAvailable();

		TCAsyncResult<TCDistributedActor<t_CActor>> mp_DistributedActor;
		TCActor<CActorDistributionManager> mp_DistributionManager;
		CActorSubscription mp_DistributedActorSubscription;
		NStr::CStr mp_Namespace;
		NContainer::TCLinkedList<TCPromise<TCDistributedActor<t_CActor>>> mp_GetActorPromises;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorSingleSubscription.hpp"
