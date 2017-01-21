// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::TCActorFunctor(TCActor<CActor> &&_Actor, NFunction::TCFunctionMutable<t_CFunction> &&_fFunctor, CActorSubscription &&_Subscription)
		: mp_Actor(fg_Move(_Actor))
		, mp_fFunctor(fg_Move(_fFunctor))
		, mp_Subscription(fg_Move(_Subscription))
	{
	}

	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::TCActorFunctor(CNullPtr)
	{
	}

	template <typename t_CFunction>
	bool TCActorFunctor<t_CFunction>::f_IsEmpty() const
	{
		return mp_fFunctor.f_IsEmpty();
	}
	
	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::operator bool () const
	{
		return !mp_fFunctor.f_IsEmpty();
	}
	
	template <typename t_CFunction>
	template <typename ...tfp_CParams>
	auto TCActorFunctor<t_CFunction>::operator ()(tfp_CParams &&...p_Params) const
	{
		if (!mp_Actor || !mp_fFunctor)
		{
			return fg_Dispatch
				(
					fg_AnyConcurrentActor()
					, []() -> CReturn
					{
						return DMibErrorInstance("Functor is empty");
					}
				)
			;
		}
		return fg_Dispatch
			(
				mp_Actor
				, [=, fFunctor = mp_fFunctor]() mutable -> CReturn
				{
					return fFunctor(fg_Forward<tfp_CParams>(p_Params)...);
				}
			)
		;
	}
	
	template <typename t_CFunction>
	TCActor<CActor> const &TCActorFunctor<t_CFunction>::f_GetActor() const
	{
		return mp_Actor;
	}
	
	template <typename t_CFunction>
	NFunction::TCFunctionMutable<t_CFunction> const &TCActorFunctor<t_CFunction>::f_GetFunctor() const
	{
		return mp_fFunctor;
	}
	
	template <typename t_CFunction>
	CActorSubscription const &TCActorFunctor<t_CFunction>::f_GetSubscription() const
	{
		return mp_Subscription;
	}
	
	template <typename t_CFunction>
	TCActor<CActor> &TCActorFunctor<t_CFunction>::f_GetActor()
	{
		return mp_Actor;
	}

	template <typename t_CFunction>
	NFunction::TCFunctionMutable<t_CFunction> &TCActorFunctor<t_CFunction>::f_GetFunctor()
	{
		return mp_fFunctor;
	}

	template <typename t_CFunction>
	CActorSubscription &TCActorFunctor<t_CFunction>::f_GetSubscription()
	{
		return mp_Subscription;
	}
	
	template <typename t_CFunction>
	void TCActorFunctor<t_CFunction>::f_Clear()
	{
		mp_Actor.f_Clear();
		mp_fFunctor.f_Clear();
		mp_Subscription.f_Clear();
	}
	
	inline CActorFunctorHelperWithProperties::CActorFunctorHelperWithProperties(TCActor<> const &_Actor)
		: mp_Actor(_Actor)
	{
	}
	
	inline CActorFunctorHelperWithProperties::CActorFunctorHelperWithProperties(TCActor<> const &_Actor, CActorSubscription &&_Subscription)
		: mp_Actor(_Actor)
		, mp_Subscription(fg_Move(_Subscription))	
	{
	}
		
	template <typename tf_FFunction>
	inline auto CActorFunctorHelperWithProperties::operator > (tf_FFunction &&_fFunction) &&
	{
		return fg_ActorFunctor(fg_Move(mp_Actor), fg_Forward<tf_FFunction>(_fFunction), fg_Move(mp_Subscription));
	}
	
	inline CActorFunctorHelperWithProperties &&CActorFunctorHelperWithProperties::operator () (TCActor<> const &_Actor) &&
	{
		mp_Actor = _Actor;
		return fg_Move(*this);
	}
	
	inline CActorFunctorHelperWithProperties &&CActorFunctorHelperWithProperties::operator () (CActorSubscription &&_Subscription) &&
	{
		mp_Subscription = fg_Move(_Subscription);
		return fg_Move(*this);
	}
	
	template <typename tf_FFunction>
	inline auto CActorFunctorHelper::operator > (tf_FFunction &&_fFunction) const
	{
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		return fg_ActorFunctor(fg_Move(CurrentActor), fg_Forward<tf_FFunction>(_fFunction));
	}
	
	inline CActorFunctorHelperWithProperties CActorFunctorHelper::operator () (TCActor<> const &_Actor) const
	{
		return CActorFunctorHelperWithProperties(_Actor);
	}
	
	inline CActorFunctorHelperWithProperties CActorFunctorHelper::operator () (CActorSubscription &&_Subscription) const
	{
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		return CActorFunctorHelperWithProperties(CurrentActor, fg_Move(_Subscription));
	}
}
