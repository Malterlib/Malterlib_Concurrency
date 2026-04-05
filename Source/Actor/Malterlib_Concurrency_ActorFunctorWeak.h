// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_ActorFunctorShared.hpp"

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctor;

	template <typename t_CFunction>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctorWeak
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<typename NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::CType>;
		static_assert(NPrivate::TCIsFuture<CReturn>::mc_Value || NPrivate::TCIsAsyncGenerator<CReturn>::mc_Value, "You need to return a future or async generator");
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

		NStorage::TCOptional<TCActorFunctor<t_CFunction>> f_Lock() const;

		void f_Clear();

		bool f_IsEmpty() const;
		explicit operator bool () const;

	protected:
		TCWeakActor<CActor> mp_Actor;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<typename NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::CType>> mp_pFunctor;
	};

	template <typename tf_CFunctor>
	auto fg_ActorFunctorWeak(TCActor<> const &_Actor, tf_CFunctor &&_fFunctor)
	{
		using CFunction = NPrivate::TCRemoveReferencesFromFunctor
			<
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>::operator ())>::CFunctionType
			>::CType
		;

		return TCActorFunctorWeak<CFunction>{fg_TempCopy(_Actor), fg_Forward<tf_CFunctor>(_fFunctor)};
	}

	struct CActorFunctorWeakHelperWithProperties
	{
		inline CActorFunctorWeakHelperWithProperties(TCActor<> const &_Actor);

		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) &&;
		inline CActorFunctorWeakHelperWithProperties &&operator () (TCActor<> const &_Actor) &&;

	private:
		TCActor<> mp_Actor;
	};

	struct CActorFunctorWeakHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const;
		inline CActorFunctorWeakHelperWithProperties operator () (TCActor<> const &_Actor) const;
	};

	extern CActorFunctorWeakHelper const &g_ActorFunctorWeak;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_ActorFunctorWeak.hpp"
