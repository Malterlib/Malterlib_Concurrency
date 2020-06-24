// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	struct TCActorFunctor
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<t_CFunction>;
		static_assert(NPrivate::TCIsFuture<CReturn>::mc_Value, "You need to return a future");
		using CStripedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;

		TCActorFunctor() = default;
		TCActorFunctor(TCActorFunctor &&) = default;
		TCActorFunctor & operator = (TCActorFunctor &&) = default;
		TCActorFunctor(TCActorFunctor const &) = delete;
		TCActorFunctor & operator = (TCActorFunctor const &) = delete;
		~TCActorFunctor();
		
		TCActorFunctor(CNullPtr);
		TCActorFunctor(TCActor<CActor> &&_Actor, NFunction::TCFunctionMovable<t_CFunction> &&_fFunctor, CActorSubscription &&_Subscription = nullptr);
		
		template <typename ...tfp_CParams>
		auto operator ()(tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>;

		template <typename ...tfp_CParams>
		auto f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>;

		template <typename tf_FDispatcher, typename ...tfp_CParams>
		auto f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>;

		TCActor<CActor> const &f_GetActor() const;
		NFunction::TCFunctionMovable<t_CFunction> const &f_GetFunctor() const;
		CActorSubscription const &f_GetSubscription() const;

		TCActor<CActor> &f_GetActor();
		NFunction::TCFunctionMovable<t_CFunction> &f_GetFunctor();
		CActorSubscription &f_GetSubscription();
		TCFuture<void> f_Destroy() &;
		TCFuture<void> f_Destroy() &&;

		void f_Clear();
		
		bool f_IsEmpty() const;
		explicit operator bool () const;

	protected:
		TCActor<CActor> mp_Actor;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<t_CFunction>> mp_pFunctor = fg_Construct();
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
		inline auto operator / (tf_FFunction &&_fFunction) &&;
		inline CActorFunctorHelperWithProperties &&operator () (TCActor<> const &_Actor) &&;
		inline CActorFunctorHelperWithProperties &&operator () (CActorSubscription &&_Subscription) &&;
		
	private:
		TCActor<> mp_Actor;
		CActorSubscription mp_Subscription;
	};
	
	struct CActorFunctorHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const;
		inline CActorFunctorHelperWithProperties operator () (TCActor<> const &_Actor) const;
		inline CActorFunctorHelperWithProperties operator () (CActorSubscription &&_Subscription) const;
	};
	
	extern CActorFunctorHelper const &g_ActorFunctor;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorFunctor.hpp"
