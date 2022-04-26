// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	TCActorFunctorWeak<t_CFunction>::TCActorFunctorWeak(TCActor<CActor> &&_Actor, NFunction::TCFunctionMovable<t_CFunction> &&_fFunctor)
		: mp_Actor(fg_Move(_Actor))
	{
		if (_fFunctor)
			mp_pFunctor = fg_Construct(fg_Move(_fFunctor));
	}

	template <typename t_CFunction>
	TCActorFunctorWeak<t_CFunction>::TCActorFunctorWeak(CNullPtr)
	{
	}
	
	template <typename t_CFunction>
	TCActorFunctorWeak<t_CFunction>::~TCActorFunctorWeak()
	{
		auto Actor = mp_Actor.f_Lock();
		if (!Actor || !mp_pFunctor)
			return;

		// Destroy functor on correct actor
		if (!Actor->f_IsCurrentActorAndProcessing())
			fg_Dispatch(Actor, [pFunctor = fg_Move(mp_pFunctor)] {}) > fg_DiscardResult();
	}

	template <typename t_CFunction>
	bool TCActorFunctorWeak<t_CFunction>::f_IsEmpty() const
	{
		return !mp_pFunctor || mp_pFunctor->f_IsEmpty();
	}
	
	template <typename t_CFunction>
	TCActorFunctorWeak<t_CFunction>::operator bool () const
	{
		return mp_pFunctor && !mp_pFunctor->f_IsEmpty();
	}

	template <typename t_CFunction>
	template <typename ...tfp_CParams>
	auto TCActorFunctorWeak<t_CFunction>::f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
		{
			return fg_Dispatch
				(
					TCActor<>(fg_DirectCallActor())
					, []() mutable -> TCFuture<CStripedReturn>
					{
						return TCPromise<CStripedReturn>() <<= DMibErrorInstance("Functor is empty");
					}
				)
				.f_Future()
			;
		}

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;
		using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CType;

		auto fToDispatch = [Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...), pFunctor = mp_pFunctor]() mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
			{
				return NStorage::fg_TupleApplyAs<CMoveList>
					(
						[&](auto &&..._Params) mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
						{
							if constexpr (NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value)
							{
								TCPromise<CStripedReturn> Promise;
								try
								{
									Promise.f_SetResult(fg_CallSafe(*pFunctor, fg_Move(_Params)...));
								}
								catch (...)
								{
									Promise.f_SetException(NException::fg_CurrentException());
								}
								return Promise.f_MoveFuture();
							}
							else if constexpr (NPrivate::TCIsFuture<CReturn>::mc_Value)
								return (*pFunctor)(fg_Move(_Params)...);
							else if constexpr (NTraits::TCIsVoid<CReturn>::mc_Value)
							{
								TCPromise<CReturn> Promise;
								(*pFunctor)(fg_Move(_Params)...);
								Promise.f_SetResult();
								return Promise.f_Future();
							}
							else
							{
								TCPromise<CReturn> Promise;
								Promise.f_SetResult((*pFunctor)(fg_Move(_Params)...));
								return Promise.f_Future();
							}
						}
						, fg_Move(Params)
					)
				;
			}
		;

		if (Actor->f_IsCurrentActorAndProcessing())
			return fg_DirectDispatch(fg_Move(fToDispatch)).f_Future();
		else
			return fg_Dispatch(mp_Actor, fg_Move(fToDispatch)).f_Future();
	}


	template <typename t_CFunction>
	template <typename ...tfp_CParams>
	auto TCActorFunctorWeak<t_CFunction>::operator ()(tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
		{
			return fg_Dispatch
				(
					TCActor<>(fg_DirectCallActor())
					, []() mutable -> TCFuture<CStripedReturn>
					{
						return TCPromise<CStripedReturn>() <<= DMibErrorInstance("Functor is empty");
					}
				)
			;
		}

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;
		using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CType;

		return fg_Dispatch
			(
				Actor
				, [Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...), pFunctor = mp_pFunctor]() mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
				{
					return NStorage::fg_TupleApplyAs<CMoveList>
						(
							[&](auto &&..._Params) mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
							{
								if constexpr (NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value)
								{
									TCPromise<CStripedReturn> Promise;
									try
									{
										Promise.f_SetResult(fg_CallSafe(*pFunctor, fg_Move(_Params)...));
									}
									catch (...)
									{
										Promise.f_SetException(NException::fg_CurrentException());
									}
									return Promise.f_MoveFuture();
								}
								else if constexpr (NPrivate::TCIsFuture<CReturn>::mc_Value)
									return (*pFunctor)(fg_Move(_Params)...);
								else if constexpr (NTraits::TCIsVoid<CReturn>::mc_Value)
								{
									TCPromise<CReturn> Promise;
									(*pFunctor)(fg_Move(_Params)...);
									Promise.f_SetResult();
									return Promise.f_Future();
								}
								else
								{
									TCPromise<CReturn> Promise;
									Promise.f_SetResult((*pFunctor)(fg_Move(_Params)...));
									return Promise.f_Future();
								}
							}
							, fg_Move(Params)
						)
					;
				}
			)
		;
	}

	template <typename t_CFunction>
	template <typename tf_FDispatcher, typename ...tfp_CParams>
	auto TCActorFunctorWeak<t_CFunction>::f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>
	{
		auto Actor = mp_Actor.f_Lock();

		if (!Actor || !mp_pFunctor || !*mp_pFunctor)
		{
			return fg_Dispatch
				(
					TCActor<>(fg_DirectCallActor())
					, []() mutable -> TCFuture<CStripedReturn>
					{
						return TCPromise<CStripedReturn>() <<= DMibErrorInstance("Functor is empty");
					}
				)
			;
		}

		using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CMoveList;
		using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CType;

		return fg_Dispatch
			(
				Actor
				, [fDispatcher = fg_Forward<tf_FDispatcher>(_fDispatcher), Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...), pFunctor = mp_pFunctor]
			 	() mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
				{
					return fDispatcher
						(
						 	[&]() mark_no_coroutine_debug -> TCFuture<CStripedReturn>
						 	{
								return NStorage::fg_TupleApplyAs<CMoveList>
									(
										[&](auto &&..._Params) mutable mark_no_coroutine_debug -> TCFuture<CStripedReturn>
										{
											if constexpr (NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value)
											{
												TCPromise<CStripedReturn> Promise;
												try
												{
													Promise.f_SetResult(fg_CallSafe(*pFunctor, fg_Move(_Params)...));
												}
												catch (...)
												{
													Promise.f_SetException(NException::fg_CurrentException());
												}
												return Promise.f_MoveFuture();
											}
											else if constexpr (NPrivate::TCIsFuture<CReturn>::mc_Value)
												return (*pFunctor)(fg_Move(_Params)...);
											else if constexpr (NTraits::TCIsVoid<CReturn>::mc_Value)
											{
												TCPromise<CReturn> Promise;
												(*pFunctor)(fg_Move(_Params)...);
												Promise.f_SetResult();
												return Promise.f_Future();
											}
											else
											{
												TCPromise<CReturn> Promise;
												Promise.f_SetResult((*pFunctor)(fg_Move(_Params)...));
												return Promise.f_Future();
											}
										}
										, fg_Move(Params)
									)
								;
							}
						)
					;
				}
			)
		;
	}
	
	template <typename t_CFunction>
	TCWeakActor<CActor> const &TCActorFunctorWeak<t_CFunction>::f_GetActor() const
	{
		return mp_Actor;
	}
	
	template <typename t_CFunction>
	NFunction::TCFunctionMovable<t_CFunction> const &TCActorFunctorWeak<t_CFunction>::f_GetFunctor() const
	{
		DMibFastCheck(mp_pFunctor);
		return *mp_pFunctor;
	}
	
	template <typename t_CFunction>
	TCWeakActor<CActor> &TCActorFunctorWeak<t_CFunction>::f_GetActor()
	{
		DMibFastCheck(mp_pFunctor);
		return mp_Actor;
	}

	template <typename t_CFunction>
	NFunction::TCFunctionMovable<t_CFunction> &TCActorFunctorWeak<t_CFunction>::f_GetFunctor()
	{
		return *mp_pFunctor;
	}

	template <typename t_CFunction>
	TCFuture<void> TCActorFunctorWeak<t_CFunction>::f_Destroy() &&
	{
		TCPromise<void> Promise;

		TCFuture<void> DispatchFuture;
		auto Actor = mp_Actor.f_Lock();
		auto pFunctor = fg_Move(mp_pFunctor);
		mp_Actor.f_Clear();

		if (Actor && mp_pFunctor)
		{
			if (Actor->f_IsCurrentActorAndProcessing())
				DispatchFuture = TCPromise<void>() <<= g_Void;
			else
				DispatchFuture = fg_Dispatch(fg_Move(Actor), [pFunctor = fg_Move(pFunctor)] {}).f_Future();
		}
		else
			DispatchFuture = TCPromise<void>() <<= g_Void;

		fg_Move(DispatchFuture) > NPrivate::fg_DirectResultActor() / [Promise](TCAsyncResult<void> &&) mutable
			{
				Promise.f_SetResult();
			}
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CFunction>
	void TCActorFunctorWeak<t_CFunction>::f_Clear()
	{
		auto Actor = mp_Actor.f_Lock();
		if (Actor && mp_pFunctor)
		{
			// Destroy functor on correct actor
			if (Actor->f_IsCurrentActorAndProcessing())
				mp_pFunctor.f_Clear();
			else
				fg_Dispatch(Actor, [pFunctor = fg_Move(mp_pFunctor)] {}) > fg_DiscardResult();
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
