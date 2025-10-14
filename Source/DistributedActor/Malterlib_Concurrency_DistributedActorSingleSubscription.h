// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	struct TCDistributedActorSingleSubscription : public CActor
	{
		struct CFilter
		{
			NStr::CStr m_Namespace;
			NStr::CStr m_HostID;
		};

		TCDistributedActorSingleSubscription(CFilter const &_Filter, TCActor<CActorDistributionManager> const &_DistributionManager = fg_GetDistributionManager());
		void f_Construct();

		TCFuture<TCDistributedActor<t_CActor>> f_GetActor();

	private:
		void fp_ResultAvailable();
		TCFuture<void> fp_Destroy() override;

		TCAsyncResult<TCDistributedActor<t_CActor>> mp_DistributedActor;
		TCActor<CActorDistributionManager> mp_DistributionManager;
		CActorSubscription mp_DistributedActorSubscription;
		CFilter mp_Filter;
		NContainer::TCLinkedList<TCPromise<TCDistributedActor<t_CActor>>> mp_GetActorPromises;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorSingleSubscription.hpp"
