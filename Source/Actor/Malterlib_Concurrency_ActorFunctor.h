// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	struct TCActorFunctor
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMutable<t_CFunction>;
		static_assert(NPrivate::TCIsContinuation<CReturn>::mc_Value, "You need to return a continuation");
		
		TCActorFunctor() = default;
		TCActorFunctor(TCActorFunctor &&) = default;
		TCActorFunctor & operator = (TCActorFunctor &&) = default;
		TCActorFunctor(TCActorFunctor const &) = delete;
		TCActorFunctor & operator = (TCActorFunctor const &) = delete;
		
		TCActorFunctor(TCActor<CActor> &&_Actor, NFunction::TCFunctionMutable<t_CFunction> &&_fFunctor, CActorSubscription &&_Subscription = nullptr);
		
		template <typename ...tfp_CParams>
		auto operator ()(tfp_CParams &&...p_Params);
		
		TCActor<CActor> const &f_GetActor() const;
		NFunction::TCFunctionMutable<t_CFunction> const &f_GetFunctor() const;
		CActorSubscription const &f_GetSubscription() const;

		TCActor<CActor> &f_GetActor();
		NFunction::TCFunctionMutable<t_CFunction> &f_GetFunctor();
		CActorSubscription &f_GetSubscription();
		
		void f_Clear();

	protected:
		TCActor<CActor> mp_Actor;
		NFunction::TCFunctionMutable<t_CFunction> mp_fFunctor;
		CActorSubscription mp_Subscription;
	};
	
	template <typename tf_CFunctor>
	auto fg_ActorFunctor(TCActor<> const &_Actor, tf_CFunctor &&_fFunctor, CActorSubscription &&_Subscription = nullptr)
	{
		using CFunction = typename NTraits::NPrivate::TCFunctionObjectType_Helper<typename NTraits::TCRemoveReference<tf_CFunctor>::CType>::CType;
		return TCActorFunctor<CFunction>{fg_TempCopy(_Actor), fg_Forward<tf_CFunctor>(_fFunctor), fg_Move(_Subscription)}; 
	}
	
	struct CActorFunctorHelperWithProperties
	{
		inline CActorFunctorHelperWithProperties(TCActor<> const &_Actor);
		inline CActorFunctorHelperWithProperties(TCActor<> const &_Actor, CActorSubscription &&_Subscription);
		
		template <typename tf_FFunction>
		inline auto operator > (tf_FFunction &&_fFunction) &&;
		inline CActorFunctorHelperWithProperties &&operator () (TCActor<> const &_Actor) &&;
		inline CActorFunctorHelperWithProperties &&operator () (CActorSubscription &&_Subscription) &&;
		
	private:
		TCActor<> mp_Actor;
		CActorSubscription mp_Subscription;
	};
	
	struct CActorFunctorHelper
	{
		template <typename tf_FFunction>
		inline auto operator > (tf_FFunction &&_fFunction) const;
		inline CActorFunctorHelperWithProperties operator () (TCActor<> const &_Actor) const;
		inline CActorFunctorHelperWithProperties operator () (CActorSubscription &&_Subscription) const;
	};
	
	extern CActorFunctorHelper const &g_ActorFunctor;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorFunctor.hpp"
