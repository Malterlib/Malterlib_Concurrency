// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCActor<tf_CActor> CActorHolder::fp_GetAsActor()
	{
		return TCActorHolderSharedPointer<TCActorInternal<tf_CActor>>
			(
				fg_Explicit(static_cast<TCActorInternal<tf_CActor> *>(this))
			)
		;
	}

	inline_always CConcurrencyManager &CActorHolder::f_ConcurrencyManager() const
	{
		return *mp_pConcurrencyManager;
	}

	inline_always EPriority CActorHolder::f_GetPriority() const
	{
		return (EPriority)mp_Priority;
	}
}
