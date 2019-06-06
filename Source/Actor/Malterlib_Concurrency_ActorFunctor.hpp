// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::TCActorFunctor(TCActor<CActor> &&_Actor, NFunction::TCFunctionMovable<t_CFunction> &&_fFunctor, CActorSubscription &&_Subscription)
		: mp_Actor(fg_Move(_Actor))
		, mp_pFunctor(fg_Construct(fg_Move(_fFunctor)))
		, mp_Subscription(fg_Move(_Subscription))
	{
	}

	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::TCActorFunctor(CNullPtr)
	{
	}
	
	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::~TCActorFunctor()
	{
		if (!mp_Actor || !mp_pFunctor)
			return;

		// Destroy functor on correct actor
		fg_Dispatch(mp_Actor, [pFunctor = fg_Move(mp_pFunctor)] {}) > fg_DiscardResult();
	}

	template <typename t_CFunction>
	bool TCActorFunctor<t_CFunction>::f_IsEmpty() const
	{
		return !mp_pFunctor || mp_pFunctor->f_IsEmpty();
	}
	
	template <typename t_CFunction>
	TCActorFunctor<t_CFunction>::operator bool () const
	{
		return mp_pFunctor && !mp_pFunctor->f_IsEmpty();
	}


	namespace NPrivate
	{
		template <typename t_CParams>
		struct TCDecayedTupleHelper
		{
		};

		template <typename ...tp_CParams>
		struct TCDecayedTupleHelper<NMeta::TCTypeList<tp_CParams...>>
		{
			using CMoveList = NMeta::TCTypeList<typename NTraits::TCAddRValueReference<typename NTraits::TCDecay<tp_CParams>::CType>::CType...>;
			using CType = NStorage::TCTuple<typename NTraits::TCDecay<tp_CParams>::CType...>;
		};
	}


	template <typename t_CFunction>
	template <typename ...tfp_CParams>
	auto TCActorFunctor<t_CFunction>::operator ()(tfp_CParams &&...p_Params) const
	{
		if (!mp_Actor || !*mp_pFunctor)
		{
			return fg_Dispatch
				(
					fg_DirectCallActor()
					, []() -> CReturn
					{
						return DMibErrorInstance("Functor is empty");
					}
				)
			;
		}

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;
		using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CType;

		return fg_Dispatch
			(
				mp_Actor
				, [Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...), pFunctor = mp_pFunctor]() mutable -> CReturn
				{
					return NStorage::fg_TupleApplyAs<CMoveList>
						(
							[&](auto &&..._Params) mutable
							{
#if DMibEnableSafeCheck > 0
								CReturn ReturnFuture;

								NPrivate::fg_WrapDispatchWithReturn
									(
										[&]
										{
											ReturnFuture = (*pFunctor)(fg_Move(_Params)...);
										}
									)
								;

								return ReturnFuture;
#else
								return (*pFunctor)(fg_Move(_Params)...);
#endif
							}
							, fg_Move(Params) 
						)
					;
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
	NFunction::TCFunctionMovable<t_CFunction> const &TCActorFunctor<t_CFunction>::f_GetFunctor() const
	{
		return *mp_pFunctor;
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
	NFunction::TCFunctionMovable<t_CFunction> &TCActorFunctor<t_CFunction>::f_GetFunctor()
	{
		return *mp_pFunctor;
	}

	template <typename t_CFunction>
	CActorSubscription &TCActorFunctor<t_CFunction>::f_GetSubscription()
	{
		return mp_Subscription;
	}

	template <typename t_CFunction>
	TCFuture<void> TCActorFunctor<t_CFunction>::f_Destroy()
	{
		co_await ECoroutineFlag_AllowReferences;

		auto Subscription = fg_Move(f_GetSubscription());

		if (mp_Actor && mp_pFunctor)
			co_await fg_Dispatch(mp_Actor, [pFunctor = fg_Move(mp_pFunctor)] {}).f_Wrap();

		if (Subscription)
			co_await Subscription->f_Destroy();

		co_return {};
	}
	
	template <typename t_CFunction>
	void TCActorFunctor<t_CFunction>::f_Clear()
	{
		if (mp_Actor && mp_pFunctor)
		{
			// Destroy functor on correct actor
			fg_Dispatch(mp_Actor, [pFunctor = fg_Move(mp_pFunctor)] {}) > fg_DiscardResult();
		}

		mp_Actor.f_Clear();
		mp_pFunctor.f_Clear();
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
	inline auto CActorFunctorHelperWithProperties::operator / (tf_FFunction &&_fFunction) &&
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
	inline auto CActorFunctorHelper::operator / (tf_FFunction &&_fFunction) const
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
