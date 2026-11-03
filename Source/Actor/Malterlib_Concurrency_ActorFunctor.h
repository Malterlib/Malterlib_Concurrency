// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"

namespace NMib::NConcurrency
{
	// A coalesced functor admits one queued delivery at a time; calls made while one is queued resolve
	// at once and are covered by it. Only TCFuture<void> () functions can be coalesced
	template <typename t_CFunction, bool t_bCoalesced>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctor
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<typename NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::FType>;
		using CStorage = typename NPrivate::TCActorFunctorStorage<t_CFunction, t_bCoalesced>::CType;
		static_assert(NPrivate::TCIsFuture<CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value, "You need to return a future or async generator");
		static_assert(!t_bCoalesced || NPrivate::cIsCoalescableActorFunction<t_CFunction>, "Only TCFuture<void> () functors can be coalesced");
		using CStripedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;

		friend struct TCActorFunctorWeak<t_CFunction, t_bCoalesced>;

		TCActorFunctor() = default;
		TCActorFunctor(TCActorFunctor &&_fOther) = default;
		TCActorFunctor &operator = (TCActorFunctor &&_fOther) = default;
		TCActorFunctor(TCActorFunctor const &_fOther) = delete;
		TCActorFunctor &operator = (TCActorFunctor const &_fOther) = delete;
		~TCActorFunctor();

		TCActorFunctor(CNullPtr _pNull);
		TCActorFunctor(TCActor<CActor> &&_Actor, CFunction &&_fFunctor, CActorSubscription &&_Subscription = nullptr);

		template <typename ...tfp_CParams>
		auto operator () (tfp_CParams && ...p_Params) const -> TCFuture<CStripedReturn>
			requires (NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename ...tfp_CParams>
		auto f_CallDirect(tfp_CParams && ...p_Params) const -> TCFuture<CStripedReturn>
			requires (NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename ...tfp_CParams>
		void f_CallDiscard(tfp_CParams && ...p_Params) const
			requires (NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
		;

		template <typename tf_FDispatcher, typename ...tfp_CParams>
		auto f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams && ...p_Params) const -> TCFuture<CStripedReturn>
			requires (NPrivate::cIsActorFunctorCallableWith<t_CFunction, tfp_CParams...>)
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
		NStorage::TCSharedPointer<CStorage> mp_pFunctor;
		CActorSubscription mp_Subscription;
	};

	template <bool tf_bCoalesced = false, typename tf_FFunctor>
	auto fg_ActorFunctor(TCActor<> const &_Actor, tf_FFunctor &&_fFunctor, CActorSubscription &&_Subscription = nullptr)
	{
		using FFunction = NPrivate::TCRemoveReferencesFromFunctor
			<
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReferenceAndQualifiers<tf_FFunctor>::operator ())>::FFunctionType
			>
			::FType
		;

		return TCActorFunctor<FFunction, tf_bCoalesced>{fg_TempCopy(_Actor), fg_Forward<tf_FFunctor>(_fFunctor), fg_Move(_Subscription)};
	}

	template <bool t_bCoalesced>
	struct TCActorFunctorHelperWithProperties
	{
		inline TCActorFunctorHelperWithProperties(TCActor<> const &_Actor);
		inline TCActorFunctorHelperWithProperties(TCActor<> const &_Actor, CActorSubscription &&_Subscription);

		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) &&;
		inline TCActorFunctorHelperWithProperties &&operator () (TCActor<> const &_Actor) &&;
		inline TCActorFunctorHelperWithProperties &&operator () (CActorSubscription &&_Subscription) &&;

	private:
		TCActor<> mp_Actor;
		CActorSubscription mp_Subscription;
	};

	template <bool t_bCoalesced>
	struct TCActorFunctorHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const;
		inline TCActorFunctorHelperWithProperties<t_bCoalesced> operator () (TCActor<> const &_Actor) const;
		inline TCActorFunctorHelperWithProperties<t_bCoalesced> operator () (CActorSubscription &&_Subscription) const;
	};

	using CActorFunctorHelperWithProperties = TCActorFunctorHelperWithProperties<false>;
	using CActorFunctorHelper = TCActorFunctorHelper<false>;
	using CActorFunctorCoalesced = TCActorFunctor<TCFuture<void> (), true>;

	extern CActorFunctorHelper const &g_ActorFunctor; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
	extern TCActorFunctorHelper<true> const &g_ActorFunctorCoalesced; // NOLINT(malterlib-conceptual-prefix): binds an actor and returns a builder, not a function object
}

#ifndef DMibPNoShortCuts
using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorFunctor.hpp"
