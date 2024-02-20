// Copyright © 2022 Favro Holding AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CFunction>
	struct TCActorFunctorWeak
	{
		using CReturn = typename NTraits::TCFunctionTraits<t_CFunction>::CReturn;
		using CFunction = NFunction::TCFunctionMovable<t_CFunction>;
		using CStripedReturn = typename NPrivate::TCIsFuture<CReturn>::CType;

		TCActorFunctorWeak() = default;
		TCActorFunctorWeak(TCActorFunctorWeak &&) = default;
		TCActorFunctorWeak & operator = (TCActorFunctorWeak &&) = default;
		TCActorFunctorWeak(TCActorFunctorWeak const &) = delete;
		TCActorFunctorWeak & operator = (TCActorFunctorWeak const &) = delete;
		~TCActorFunctorWeak();
		
		TCActorFunctorWeak(CNullPtr);
		TCActorFunctorWeak(TCActor<CActor> &&_Actor, NFunction::TCFunctionMovable<t_CFunction> &&_fFunctor);
		
		template <typename ...tfp_CParams>
		auto operator ()(tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>;

		template <typename ...tfp_CParams>
		auto f_CallDirect(tfp_CParams &&...p_Params) const -> TCFuture<CStripedReturn>;

		template <typename tf_FDispatcher, typename ...tfp_CParams>
		auto f_CallWrapped(tf_FDispatcher &&_fDispatcher, tfp_CParams &&...p_Params) const -> TCDispatchedActorCall<CStripedReturn>;

		TCWeakActor<CActor> const &f_GetActor() const;
		NFunction::TCFunctionMovable<t_CFunction> const &f_GetFunctor() const;

		TCWeakActor<CActor> &f_GetActor();
		NFunction::TCFunctionMovable<t_CFunction> &f_GetFunctor();
		TCFuture<void> f_Destroy() &&;

		void f_Clear();
		
		bool f_IsEmpty() const;
		explicit operator bool () const;

	protected:
		TCWeakActor<CActor> mp_Actor;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<t_CFunction>> mp_pFunctor;
	};
	
	template <typename tf_CFunctor>
	auto fg_ActorFunctorWeak(TCActor<> const &_Actor, tf_CFunctor &&_fFunctor)
	{
		using CFunction = typename NTraits::TCMemberFunctionPointerTraits<decltype(&NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>::CType::operator ())>::CFunctionType;
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
