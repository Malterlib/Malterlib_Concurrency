// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctor<t_CFunction>::TCActorFunctor(TCActor<CActor> &&_Actor, CFunction &&_fFunctor, CActorSubscription &&_Subscription)
		: mp_Actor(fg_Move(_Actor))
		, mp_Subscription(fg_Move(_Subscription))
	{
		if (_fFunctor)
			mp_pFunctor = fg_Construct(fg_Move(_fFunctor));
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctor<t_CFunction>::TCActorFunctor(CNullPtr)
	{
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctor<t_CFunction>::~TCActorFunctor()
	{
		if (!mp_Actor || !mp_pFunctor)
			return;

		// Destroy functor on correct actor
		if (!mp_Actor->f_IsCurrentActorAndProcessing())
			fg_Dispatch(mp_Actor, [pFunctor = fg_Move(mp_pFunctor)] {}).f_DiscardResult();
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	bool TCActorFunctor<t_CFunction>::f_IsEmpty() const
	{
		return !mp_pFunctor || mp_pFunctor->f_IsEmpty();
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctor<t_CFunction>::operator bool () const
	{
		return mp_pFunctor && !mp_pFunctor->f_IsEmpty();
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename ...tfp_CParams>
	auto TCActorFunctor<t_CFunction>::f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		if (!mp_Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		if (mp_Actor->f_IsCurrentActorAndProcessing())
		{
			return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
				{
					return mp_Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, CBindActorOptions{EVirtualCall::mc_NotVirtual, EActorCallType::mc_Direct}>
						(
							fg_TempCopy(mp_pFunctor)
							, fg_CopyOrMove<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))...
						)
						.f_Call()
					;
				}
				(CMoveList())
			;
		}
		else
		{
			return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
				{
					return mp_Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
						(
							fg_TempCopy(mp_pFunctor)
							, fg_CopyOrMove<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))...
						)
						.f_Call()
					;
				}
				(CMoveList())
			;
		}
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename ...tfp_CParams>
	void TCActorFunctor<t_CFunction>::f_CallDiscard(tfp_CParams &&...p_Params) const
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		if (!mp_Actor || !mp_pFunctor || !*mp_pFunctor)
			return;

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> void
			{
				return mp_Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
					(
						fg_TempCopy(mp_pFunctor)
						, fg_CopyOrMove<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))...
					)
					.f_DiscardResult()
				;
			}
			(CMoveList())
		;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename ...tfp_CParams>
	auto TCActorFunctor<t_CFunction>::operator ()(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		if (!mp_Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
			{
				return mp_Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
					(
						fg_TempCopy(mp_pFunctor)
						, fg_CopyOrMove<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))...
					)
					.f_Call()
				;
			}
			(CMoveList())
		;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename tf_FDispatcher, typename ...tfp_CParams>
	auto TCActorFunctor<t_CFunction>::f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		if (!mp_Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

#ifdef DCompiler_MSVC_Workaround
		using CIndices = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CIndices;
		return [&]<typename ...tfp_CParams2, mint ...tfp_iParam>(NMeta::TCTypeList<tfp_CParams2...> &&, NMeta::TCIndices<tfp_iParam...> &&) -> TCFuture<CStripedReturn>
			{
				return fg_Dispatch
					(
						mp_Actor
						,
						[
							fDispatcher = fg_Forward<tf_FDispatcher>(_fDispatcher)
							, Params = NStorage::fg_Tuple(fg_Forward<tfp_CParams>(p_Params)...)
							, pFunctor = mp_pFunctor
						] mark_no_coroutine_debug
						() mutable
						{
							return fDispatcher
								(
									[&] mark_no_coroutine_debug ()
									{
										return [&]<typename ...tfp_CParams3> mark_no_coroutine_debug (tfp_CParams3 &&...p_Params) mutable
											{
												return (*pFunctor)(fg_Move(p_Params)...);
											}
											(fg_Move(fg_Get<tfp_iParam>(Params))...)
										;
									}
								)
							;
						}
					)
				;
			}
			(CMoveList(), CIndices())
		;
#else
		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
			{
				return fg_Dispatch
					(
						mp_Actor
						,
						[
							fDispatcher = fg_Forward<tf_FDispatcher>(_fDispatcher)
							, ...p_Params2 = NTraits::TCDecay<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))
							, pFunctor = mp_pFunctor
						] mark_no_coroutine_debug
						() mutable
						{
							return fDispatcher
								(
									[&] mark_no_coroutine_debug ()
									{
										return [&]<typename ...tfp_CParams3> mark_no_coroutine_debug (tfp_CParams3 &&...p_Params) mutable
											{
												return (*pFunctor)(fg_Move(p_Params)...);
											}
											(fg_Move(p_Params2)...)
										;
									}
								)
							;
						}
					)
				;
			}
			(CMoveList())
		;
#endif
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActor<CActor> const &TCActorFunctor<t_CFunction>::f_GetActor() const
	{
		return mp_Actor;
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	auto TCActorFunctor<t_CFunction>::f_GetFunctor() const -> CFunction const &
	{
		DMibFastCheck(mp_pFunctor);
		return *mp_pFunctor;
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	CActorSubscription const &TCActorFunctor<t_CFunction>::f_GetSubscription() const
	{
		return mp_Subscription;
	}
	
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActor<CActor> &TCActorFunctor<t_CFunction>::f_GetActor()
	{
		return mp_Actor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	auto TCActorFunctor<t_CFunction>::f_GetFunctor( )-> CFunction &
	{
		DMibFastCheck(mp_pFunctor);
		return *mp_pFunctor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	CActorSubscription &TCActorFunctor<t_CFunction>::f_GetSubscription()
	{
		return mp_Subscription;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCFuture<void> TCActorFunctor<t_CFunction>::f_Destroy() &&
	{
		auto Subscription = fg_Move(f_GetSubscription());

		TCFuture<void> DestroySubscriptionFuture;
		if (Subscription)
			DestroySubscriptionFuture = fg_Exchange(Subscription, nullptr)->f_Destroy();
		else
			DestroySubscriptionFuture = g_Void;

		TCPromiseFuturePair<void> Promise;
		fg_Move(DestroySubscriptionFuture).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise), Actor = fg_Move(mp_Actor), pFunctor = fg_Move(mp_pFunctor)](TCAsyncResult<void> &&_Result) mutable
				{
					TCFuture<void> DispatchFuture;

					if (Actor && pFunctor)
					{
						DispatchFuture = fg_Dispatch
							(
								fg_Move(Actor)
								, [pFunctor = fg_Move(pFunctor)]() mutable
								{
									pFunctor.f_Clear();
								}
							)
						;
					}
					else
					{
						pFunctor.f_Clear();
						DispatchFuture = g_Void;
					}

					fg_Move(DispatchFuture).f_OnResultSet
						(
							[Promise = fg_Move(Promise), Result = fg_Move(_Result)](TCAsyncResult<void> &&) mutable
							{
								Promise.f_SetResult(fg_Move(Result));
							}
						)
					;
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	void TCActorFunctor<t_CFunction>::f_Clear()
	{
		mp_Subscription.f_Clear();
		
		if (mp_Actor && mp_pFunctor)
		{
			// Destroy functor on correct actor
			if (mp_Actor->f_IsCurrentActorAndProcessing())
				mp_pFunctor.f_Clear();
			else
				fg_Dispatch(mp_Actor, [pFunctor = fg_Move(mp_pFunctor)] {}).f_DiscardResult();
		}

		mp_Actor.f_Clear();
		mp_pFunctor.f_Clear();
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
