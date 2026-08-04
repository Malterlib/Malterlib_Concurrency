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
	struct TCActorFunctorWeak
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<typename NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::CType>;
		using CStorage = typename NPrivate::TCActorFunctorStorage<t_CFunction, t_bCoalesced>::CType;
		static_assert(NPrivate::TCIsFuture<CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value, "You need to return a future or async generator");
		static_assert(!t_bCoalesced || NPrivate::cIsCoalescableActorFunction<t_CFunction>, "Only TCFuture<void> () functors can be coalesced");
		using CStripedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;

		TCActorFunctorWeak() = default;
		TCActorFunctorWeak(TCActorFunctorWeak &&) = default;
		TCActorFunctorWeak & operator = (TCActorFunctorWeak &&) = default;
		TCActorFunctorWeak(TCActorFunctorWeak const &) = delete;
		TCActorFunctorWeak & operator = (TCActorFunctorWeak const &) = delete;
		~TCActorFunctorWeak();

		TCActorFunctorWeak(CNullPtr);
		TCActorFunctorWeak(TCActor<CActor> &&_Actor, CFunction &&_fFunctor);

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

		TCWeakActor<CActor> const &f_GetActor() const;
		CFunction const &f_GetFunctor() const;

		TCWeakActor<CActor> &f_GetActor();
		CFunction &f_GetFunctor();
		TCFuture<void> f_Destroy() &&;

		NStorage::TCOptional<TCActorFunctor<t_CFunction, t_bCoalesced>> f_Lock() const;

		void f_Clear();

		bool f_IsEmpty() const;
		explicit operator bool () const;

	protected:
		TCWeakActor<CActor> mp_Actor;
		NStorage::TCSharedPointer<CStorage> mp_pFunctor;
	};

	template <bool t_bCoalesced = false, typename tf_CFunctor>
	auto fg_ActorFunctorWeak(TCActor<> const &_Actor, tf_CFunctor &&_fFunctor)
	{
		using CFunction = NPrivate::TCRemoveReferencesFromFunctor
			<
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>::operator ())>::CFunctionType
			>::CType
		;

		return TCActorFunctorWeak<CFunction, t_bCoalesced>{fg_TempCopy(_Actor), fg_Forward<tf_CFunctor>(_fFunctor)};
	}

	template <bool t_bCoalesced>
	struct TCActorFunctorWeakHelperWithProperties
	{
		inline TCActorFunctorWeakHelperWithProperties(TCActor<> const &_Actor);

		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) &&;
		inline TCActorFunctorWeakHelperWithProperties &&operator () (TCActor<> const &_Actor) &&;

	private:
		TCActor<> mp_Actor;
	};

	template <bool t_bCoalesced>
	struct TCActorFunctorWeakHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const;
		inline TCActorFunctorWeakHelperWithProperties<t_bCoalesced> operator () (TCActor<> const &_Actor) const;
	};

	using CActorFunctorWeakHelperWithProperties = TCActorFunctorWeakHelperWithProperties<false>;
	using CActorFunctorWeakHelper = TCActorFunctorWeakHelper<false>;
	using CActorFunctorWeakCoalesced = TCActorFunctorWeak<TCFuture<void> (), true>;

	extern CActorFunctorWeakHelper const &g_ActorFunctorWeak;
	extern TCActorFunctorWeakHelper<true> const &g_ActorFunctorWeakCoalesced;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorFunctorWeak.hpp"
