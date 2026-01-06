// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"
#include "Malterlib_Concurrency_ActorFunctor.h"

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctorWeak<t_CFunction>::TCActorFunctorWeak(TCActor<CActor> &&_Actor, CFunction &&_fFunctor)
		: mp_Actor(fg_Move(_Actor))
	{
		if (_fFunctor)
			mp_pFunctor = fg_Construct(fg_Move(_fFunctor));
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctorWeak<t_CFunction>::TCActorFunctorWeak(CNullPtr)
	{
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctorWeak<t_CFunction>::~TCActorFunctorWeak()
	{
		auto Actor = mp_Actor.f_Lock();
		if (!Actor || !mp_pFunctor)
			return;

		// Destroy functor on correct actor
		if (!Actor->f_IsCurrentActorAndProcessing())
			fg_Dispatch(Actor, [pFunctor = fg_Move(mp_pFunctor)] {}).f_DiscardResult();
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	bool TCActorFunctorWeak<t_CFunction>::f_IsEmpty() const
	{
		return !mp_pFunctor || mp_pFunctor->f_IsEmpty();
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCActorFunctorWeak<t_CFunction>::operator bool () const
	{
		return mp_pFunctor && !mp_pFunctor->f_IsEmpty();
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename ...tfp_CParams>
	auto TCActorFunctorWeak<t_CFunction>::f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		if (Actor->f_IsCurrentActorAndProcessing())
		{
			return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
				{
					return Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, CBindActorOptions{EVirtualCall::mc_NotVirtual, EActorCallType::mc_Direct}>
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
					return Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
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
	void TCActorFunctorWeak<t_CFunction>::f_CallDiscard(tfp_CParams &&...p_Params) const
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
			return;

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> void
			{
				return Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
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
	NStorage::TCOptional<TCActorFunctor<t_CFunction>> TCActorFunctorWeak<t_CFunction>::f_Lock() const
	{
		auto Actor = mp_Actor.f_Lock();
		if (!Actor)
			return {};

		TCActorFunctor<t_CFunction> Return;
		Return.mp_Actor = fg_Move(Actor);
		Return.mp_pFunctor = mp_pFunctor;

		return fg_Move(Return);
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	template <typename ...tfp_CParams>
	auto TCActorFunctorWeak<t_CFunction>::operator ()(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
			{
				return Actor.template f_Bind<&CActor::f_DispatchWithReturnShared<CReturn, tfp_CParams2...>, EVirtualCall::mc_NotVirtual>
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
	auto TCActorFunctorWeak<t_CFunction>::f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
		requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
			return DMibErrorInstance("Functor is empty");

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;

		return [&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&) -> TCFuture<CStripedReturn>
			{
				return fg_Dispatch
					(
						Actor
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
										return [&]<typename ...tfp_CParams3> mark_no_coroutine_debug (tfp_CParams3 &&...p_Params) mutable -> TCFuture<CStripedReturn>
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
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCWeakActor<CActor> const &TCActorFunctorWeak<t_CFunction>::f_GetActor() const
	{
		return mp_Actor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	auto TCActorFunctorWeak<t_CFunction>::f_GetFunctor() const -> CFunction const &
	{
		DMibFastCheck(mp_pFunctor);
		return *mp_pFunctor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCWeakActor<CActor> &TCActorFunctorWeak<t_CFunction>::f_GetActor()
	{
		DMibFastCheck(mp_pFunctor);
		return mp_Actor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	auto TCActorFunctorWeak<t_CFunction>::f_GetFunctor() -> CFunction &
	{
		return *mp_pFunctor;
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	TCFuture<void> TCActorFunctorWeak<t_CFunction>::f_Destroy() &&
	{
		TCFuture<void> DispatchFuture;
		auto Actor = mp_Actor.f_Lock();
		auto pFunctor = fg_Move(mp_pFunctor);
		mp_Actor.f_Clear();

		if (Actor && pFunctor)
		{
			if (Actor->f_IsCurrentActorAndProcessing())
			{
				pFunctor.f_Clear();
				DispatchFuture = g_Void;
			}
			else
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
		}
		else
		{
			pFunctor.f_Clear();
			DispatchFuture = g_Void;
		}

		TCPromiseFuturePair<void> Promise;
		fg_Move(DispatchFuture).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<void> &&) mutable
				{
					Promise.f_SetResult();
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	void TCActorFunctorWeak<t_CFunction>::f_Clear()
	{
		auto Actor = mp_Actor.f_Lock();
		if (Actor && mp_pFunctor)
		{
			// Destroy functor on correct actor
			if (Actor->f_IsCurrentActorAndProcessing())
				mp_pFunctor.f_Clear();
			else
				fg_Dispatch(Actor, [pFunctor = fg_Move(mp_pFunctor)] {}).f_DiscardResult();
		}

		mp_Actor.f_Clear();
		mp_pFunctor.f_Clear();
	}

	inline CActorFunctorWeakHelperWithProperties::CActorFunctorWeakHelperWithProperties(TCActor<> const &_Actor)
		: mp_Actor(_Actor)
	{
	}

	template <typename tf_FFunction>
	inline auto CActorFunctorWeakHelperWithProperties::operator / (tf_FFunction &&_fFunction) &&
	{
		return fg_ActorFunctorWeak(fg_Move(mp_Actor), fg_Forward<tf_FFunction>(_fFunction));
	}

	inline CActorFunctorWeakHelperWithProperties &&CActorFunctorWeakHelperWithProperties::operator () (TCActor<> const &_Actor) &&
	{
		mp_Actor = _Actor;
		return fg_Move(*this);
	}

	template <typename tf_FFunction>
	inline auto CActorFunctorWeakHelper::operator / (tf_FFunction &&_fFunction) const
	{
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		return fg_ActorFunctorWeak(fg_Move(CurrentActor), fg_Forward<tf_FFunction>(_fFunction));
	}

	inline CActorFunctorWeakHelperWithProperties CActorFunctorWeakHelper::operator () (TCActor<> const &_Actor) const
	{
		return CActorFunctorWeakHelperWithProperties(_Actor);
	}
}
