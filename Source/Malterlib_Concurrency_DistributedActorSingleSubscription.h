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
			TCDistributedActorSingleSubscription(NStr::CStr const &_Namespace);
			void f_Construct();
			
			TCContinuation<TCDistributedActor<t_CActor>> f_GetActor();
			
		private:
			void fp_ResultAvailable();
			
			TCAsyncResult<TCDistributedActor<t_CActor>> mp_DistributedActor;
			CActorCallback mp_DistributedActorSubscription;
			NStr::CStr mp_Namespace;
			TCLinkedList<TCContinuation<TCDistributedActor<t_CActor>>> mp_GetActorContinuations;
			mint mp_ThreadID = 0;
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorSingleSubscription.hpp"