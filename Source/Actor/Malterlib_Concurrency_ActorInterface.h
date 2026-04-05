// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CInterface>
	struct TCActorInterface : public TCActor<t_CInterface>
	{
		TCActorInterface() = default;
		TCActorInterface(CNullPtr);
		TCActorInterface(TCActor<t_CInterface> &&_Actor, CActorSubscription &&_Subscription = nullptr);

		CActorSubscription const &f_GetSubscription() const;
		CActorSubscription &f_GetSubscription();
		TCFuture<void> f_Destroy();

		TCActor<t_CInterface> const &f_GetActor() const;
		TCActor<t_CInterface> &f_GetActor();

		void f_Clear();

	protected:
		CActorSubscription mp_Subscription;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorInterface.hpp"
