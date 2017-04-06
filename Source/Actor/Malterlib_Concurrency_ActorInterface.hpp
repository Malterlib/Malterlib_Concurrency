// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CInterface>
	TCActorInterface<t_CInterface>::TCActorInterface(TCActor<t_CInterface> &&_Actor, CActorSubscription &&_Subscription)
		: TCActor<t_CInterface>(fg_Move(_Actor))
		, mp_Subscription(fg_Move(_Subscription))
	{
	}
	
	template <typename t_CInterface>
	TCActorInterface<t_CInterface>::TCActorInterface(CNullPtr)
	{
	}
	
	template <typename t_CInterface>
	CActorSubscription const &TCActorInterface<t_CInterface>::f_GetSubscription() const
	{
		return mp_Subscription;
	}

	template <typename t_CInterface>
	CActorSubscription &TCActorInterface<t_CInterface>::f_GetSubscription()
	{
		return mp_Subscription;
	}
	
	template <typename t_CInterface>
	TCContinuation<void> TCActorInterface<t_CInterface>::f_Destroy()
	{
		TCContinuation<void> Continuation;
		if (f_GetSubscription())
			Continuation = f_GetSubscription()->f_Destroy();
		else
			Continuation.f_SetResult();
		return Continuation;
	}
		
	template <typename t_CInterface>
	void TCActorInterface<t_CInterface>::f_Clear()
	{
		mp_Subscription.f_Clear();
		TCActor<t_CInterface>::f_Clear();
	}
	
	template <typename t_CInterface>
	TCActor<t_CInterface> const &TCActorInterface<t_CInterface>::f_GetActor() const
	{
		return *this;
	}
	
	template <typename t_CInterface>
	TCActor<t_CInterface> &TCActorInterface<t_CInterface>::f_GetActor()
	{
		return *this;
	}
}
