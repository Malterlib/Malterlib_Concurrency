// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CParams>
	struct TCDecayedTupleHelper
	{
	};

	template <typename ...tp_CParams>
	struct TCDecayedTupleHelper<NMeta::TCTypeList<tp_CParams...>>
	{
		using CMoveList = NMeta::TCTypeList<NTraits::TCAddRValueReference<NTraits::TCDecay<tp_CParams>>...>;
		using CCallType = void (*)(NTraits::TCDecay<tp_CParams>...);
		using CType = NStorage::TCTuple<NTraits::TCDecay<tp_CParams>...>;
		using CIndices = NMeta::TCConsecutiveIndices<sizeof...(tp_CParams)>;
	};

	template <typename t_CFunction, typename ...tp_CParams>
	concept cIsActorFunctorCallableWith = NTraits::cIsCallableWith
		<
			typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CCallType, void (tp_CParams...)
		>
	;

	template <typename t_CFunction>
	struct TCAddRValueReferencesToFunctor
	{
	};

	template <typename t_CReturn, typename ...tp_CParams>
	struct TCAddRValueReferencesToFunctor<t_CReturn (tp_CParams ...p_Parems)>
	{
		static constexpr bool mc_bAnyReference = ((... || NTraits::cIsReference<tp_CParams>));

		using CType = t_CReturn (NTraits::TCRemoveQualifiersAndAddRValueReference<tp_CParams> ...p_Params);
	};

	template <typename t_CFunction>
	struct TCRemoveReferencesFromFunctor
	{
	};

	template <typename t_CReturn, typename ...tp_CParams>
	struct TCRemoveReferencesFromFunctor<t_CReturn (tp_CParams ...p_Parems)>
	{
		using CType = t_CReturn (NTraits::TCDecay<tp_CParams> ...p_Params);
	};
}
