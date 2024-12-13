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
		using CMoveList = NMeta::TCTypeList<typename NTraits::TCAddRValueReference<typename NTraits::TCDecay<tp_CParams>::CType>::CType...>;
		using CCallType = void (*)(typename NTraits::TCDecay<tp_CParams>::CType...);
		using CType = NStorage::TCTuple<typename NTraits::TCDecay<tp_CParams>::CType...>;
		using CIndices = NMeta::TCConsecutiveIndices<sizeof...(tp_CParams)>;
	};

	template <typename t_CFunction, typename ...tp_CParams>
	concept cIsActorFunctorCallableWith = NTraits::TCIsCallableWith
		<
			typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCFunctionTraits<t_CFunction>::CParams>::CCallType, void (tp_CParams...)
		>::mc_Value
	;

	template <typename t_CFunction>
	struct TCAddRValueReferencesToFunctor
	{
	};

	template <typename t_CReturn, typename ...tp_CParams>
	struct TCAddRValueReferencesToFunctor<t_CReturn (tp_CParams ...p_Parems)>
	{
		static constexpr bool mc_bAnyReference = ((... || NTraits::TCIsReference<tp_CParams>::mc_Value));

		using CType = t_CReturn (typename NTraits::TCRemoveQualifiersAndAddRValueReference<tp_CParams>::CType ...p_Params);
	};

	template <typename t_CFunction>
	struct TCRemoveReferencesFromFunctor
	{
	};

	template <typename t_CReturn, typename ...tp_CParams>
	struct TCRemoveReferencesFromFunctor<t_CReturn (tp_CParams ...p_Parems)>
	{
		using CType = t_CReturn (NTraits::TCDecayType<tp_CParams> ...p_Params);
	};
}
