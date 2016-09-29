// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CActor>
		struct TCDistributedActorSingleSubscription : public CActor
		{
			TCDistributedActorSingleSubscription(NStr::CStr const &_Namespace, TCActor<CActorDistributionManager> const &_DistributionManager = fg_GetDistributionManager());
			void f_Construct();
			
			TCContinuation<TCDistributedActor<t_CActor>> f_GetActor();
			
		private:
			void fp_ResultAvailable();
			
			TCAsyncResult<TCDistributedActor<t_CActor>> mp_DistributedActor;
			TCActor<CActorDistributionManager> mp_DistributionManager;
			CActorSubscription mp_DistributedActorSubscription;
			NStr::CStr mp_Namespace;
			NContainer::TCLinkedList<TCContinuation<TCDistributedActor<t_CActor>>> mp_GetActorContinuations;
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorSingleSubscription.hpp"