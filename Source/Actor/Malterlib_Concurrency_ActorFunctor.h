// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctorWeak;

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctor
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<typename NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::CType>;
		static_assert(NPrivate::TCIsFuture<CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value, "You need to return a future or async generator");
		using CStripedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;

		friend struct TCActorFunctorWeak<t_CFunction>;

		TCActorFunctor() = default;
		TCActorFunctor(TCActorFunctor &&) = default;
		TCActorFunctor & operator = (TCActorFunctor &&) = default;
		TCActorFunctor(TCActorFunctor const &) = delete;
		TCActorFunctor & operator = (TCActorFunctor const &) = delete;
		~TCActorFunctor();
		
		TCActorFunctor(CNullPtr);
		TCActorFunctor(TCActor<CActor> &&_Actor, CFunction &&_fFunctor, CActorSubscription &&_Subscription = nullptr);
		
		template <typename ...tfp_CParams>
		auto operator ()(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
			requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename ...tfp_CParams>
		auto f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
			requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename ...tfp_CParams>
		void f_CallDiscard(tfp_CParams &&...p_Params) const
			requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename tf_FDispatcher, typename ...tfp_CParams>
		auto f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>
			requires(NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		TCActor<CActor> const &f_GetActor() const;
		CFunction const &f_GetFunctor() const;
		CActorSubscription const &f_GetSubscription() const;

		TCActor<CActor> &f_GetActor();
		CFunction &f_GetFunctor();
		CActorSubscription &f_GetSubscription();
		TCFuture<void> f_Destroy() &&;

		void f_Clear();
		
		bool f_IsEmpty() const;
		explicit operator bool () const;

	protected:
		TCActor<CActor> mp_Actor;
		NStorage::TCSharedPointer<CFunction> mp_pFunctor;
		CActorSubscription mp_Subscription;
	};
	
	template <typename tf_CFunctor>
	auto fg_ActorFunctor(TCActor<> const &_Actor, tf_CFunctor &&_fFunctor, CActorSubscription &&_Subscription = nullptr)
	{
		using CFunction = NPrivate::TCRemoveReferencesFromFunctor
			<
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>::CType::operator ())>::CFunctionType
			>
			::CType
		;

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
