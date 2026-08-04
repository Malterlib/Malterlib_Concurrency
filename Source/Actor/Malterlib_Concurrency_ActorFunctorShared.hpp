// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Atomic/Atomic>

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

	template <typename t_CFunction>
	concept cIsCoalescableActorFunction = NTraits::cIsSame<t_CFunction, TCFuture<void> ()>;

	// Admits one queued delivery at a time. The delivery disarms on its actor right before the function
	// runs, so every call that lost the exchange is covered by the queued one
	struct CActorFunctorCoalescedGate
	{
		bool f_Arm();
		void f_Disarm();
		uint32 f_GetGeneration() const;
		void f_ReopenOnFailure(uint32 _Generation);

		NAtomic::TCAtomic<bool> m_bArmed = false;
		NAtomic::TCAtomic<uint32> m_Generation = 0; // Stepped on disarm so a late failure cannot reopen a newer delivery's gate
	};

	template <typename t_CFunction>
	struct TCActorFunctorCoalescedStorage : NFunction::TCFunctionMovable<typename TCAddRValueReferencesToFunctor<t_CFunction>::CType>
	{
		CActorFunctorCoalescedGate m_Gate;
	};

	template <typename t_CFunction, bool t_bCoalesced>
	struct TCActorFunctorStorage
	{
		using CType = NFunction::TCFunctionMovable<typename TCAddRValueReferencesToFunctor<t_CFunction>::CType>;
	};

	template <typename t_CFunction>
	struct TCActorFunctorStorage<t_CFunction, true>
	{
		using CType = TCActorFunctorCoalescedStorage<t_CFunction>;
	};

	template <typename t_CFunction, bool t_bCoalesced>
	auto fg_ConstructActorFunctorStorage(NFunction::TCFunctionMovable<typename TCAddRValueReferencesToFunctor<t_CFunction>::CType> &&_fFunctor)
		-> NStorage::TCSharedPointer<typename TCActorFunctorStorage<t_CFunction, t_bCoalesced>::CType>
	;

	template <typename t_CStorage>
	auto fg_ReopenCoalescedGateOnFailure(NStorage::TCSharedPointer<t_CStorage> const &_pStorage, uint32 _Generation, TCFuture<void> &&_Delivery) -> TCFuture<void>;
}

namespace NMib::NConcurrency
{
	template <typename t_CFunction, bool t_bCoalesced = false>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctor;

	template <typename t_CFunction, bool t_bCoalesced = false>
		requires (!NPrivate::TCAddRValueReferencesToFunctor<t_CFunction>::mc_bAnyReference)
	struct TCActorFunctorWeak;
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CFunction, bool t_bCoalesced>
	auto fg_ConstructActorFunctorStorage(NFunction::TCFunctionMovable<typename TCAddRValueReferencesToFunctor<t_CFunction>::CType> &&_fFunctor)
		-> NStorage::TCSharedPointer<typename TCActorFunctorStorage<t_CFunction, t_bCoalesced>::CType>
	{
		using CStorage = typename TCActorFunctorStorage<t_CFunction, t_bCoalesced>::CType;

		if constexpr (!t_bCoalesced)
		{
			NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(fg_Move(_fFunctor));

			return pStorage;
		}
		else
		{
			// The gate shares the function's allocation, so the pointer the delivery disarms through lives as
			// long as the function it wraps. A coroutine, so the disarm runs on the actor as part of the
			// delivery and not on the caller when the delivery is queued
			NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct();
			static_cast<NFunction::TCFunctionMovable<typename TCAddRValueReferencesToFunctor<t_CFunction>::CType> &>(*pStorage)
				= [pGate = &pStorage->m_Gate, fFunctor = fg_Move(_fFunctor)]() mutable -> TCFuture<void>
				{
					pGate->f_Disarm();
					co_await fFunctor();

					co_return {};
				}
			;

			return pStorage;
		}
	}

	// A delivery that never ran must reopen the gate it armed, and only that one
	template <typename t_CStorage>
	auto fg_ReopenCoalescedGateOnFailure(NStorage::TCSharedPointer<t_CStorage> const &_pStorage, uint32 _Generation, TCFuture<void> &&_Delivery) -> TCFuture<void>
	{
		TCPromiseFuturePair<void> Pair;
		fg_Move(_Delivery).f_OnResultSet
			(
				[pStorage = _pStorage, _Generation, Promise = fg_Move(Pair.m_Promise)](TCAsyncResult<void> &&_Result) mutable
				{
					if (!_Result)
						pStorage->m_Gate.f_ReopenOnFailure(_Generation);

					Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;

		return fg_Move(Pair.m_Future);
	}
}
