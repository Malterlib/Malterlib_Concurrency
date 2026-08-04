// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ActorFunctorWeak.h"

namespace NMib::NConcurrency
{
	constexpr CActorFunctorWeakHelper g_ActorFunctorWeakInit{};
	CActorFunctorWeakHelper const &g_ActorFunctorWeak = g_ActorFunctorWeakInit;

	constexpr CActorFunctorWeakCoalescedHelper g_ActorFunctorWeakCoalescedInit{};
	CActorFunctorWeakCoalescedHelper const &g_ActorFunctorWeakCoalesced = g_ActorFunctorWeakCoalescedInit;

	CActorFunctorWeakCoalesced::CControl::CControl(NStorage::TCSharedPointer<NPrivate::CActorFunctorWeakCoalescedGate> &&_pGate, TCActorFunctorWeak<TCFuture<void> ()> &&_fTarget)
		: m_pGate(fg_Move(_pGate))
		, m_fTarget(fg_Move(_fTarget))
	{
	}

	// Lets containers store coalesced and non-coalesced subscribers uniformly
	CActorFunctorWeakCoalesced::CActorFunctorWeakCoalesced(TCActorFunctorWeak<TCFuture<void> ()> &&_fTarget)
		: mp_pControl(fg_Construct(NStorage::TCSharedPointer<NPrivate::CActorFunctorWeakCoalescedGate>(), fg_Move(_fTarget)))
	{
	}

	CActorFunctorWeakCoalesced::CActorFunctorWeakCoalesced
		(
			NStorage::TCSharedPointer<NPrivate::CActorFunctorWeakCoalescedGate> &&_pGate
			, TCActorFunctorWeak<TCFuture<void> ()> &&_fTarget
		)
		: mp_pControl(fg_Construct(fg_Move(_pGate), fg_Move(_fTarget)))
	{
	}

	TCFuture<void> CActorFunctorWeakCoalesced::operator ()() const
	{
		if (!mp_pControl)
			return DMibErrorInstance("Functor is empty");

		auto &Control = *mp_pControl;

		if (!Control.m_pGate)
			return Control.m_fTarget();

		// Arming admits exactly one queued delivery; callers that lose the exchange are covered by
		// it because the delivery disarms on its actor before the target runs
		if (Control.m_pGate->m_bArmed.f_Exchange(true))
			return g_Void;

		// Read while this delivery holds the gate: nothing else can disarm until it does
		uint32 Generation = Control.m_pGate->m_Generation.f_Load();

		TCFuture<void> CallFuture = Control.m_fTarget();

		// A delivery that fails without running the target (for example a dead actor) never reaches
		// the disarm at the target's start, so reopen the gate here. One that did reach it has
		// stepped the generation, and the gate may since have been armed by a later delivery whose
		// place a reopen would give away
		TCPromiseFuturePair<void> Pair;
		fg_Move(CallFuture).f_OnResultSet
			(
				[pGate = Control.m_pGate, Generation, Promise = fg_Move(Pair.m_Promise)](TCAsyncResult<void> &&_Result) mutable
				{
					if (!_Result && pGate->m_Generation.f_Load() == Generation)
						pGate->m_bArmed.f_Exchange(false);

					Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;

		return fg_Move(Pair.m_Future);
	}

	// Releases this copy; the target functor is destroyed on its actor by the control block's
	// destructor when the last copy is released
	TCFuture<void> CActorFunctorWeakCoalesced::f_Destroy() &&
	{
		mp_pControl.f_Clear();

		return g_Void;
	}

	void CActorFunctorWeakCoalesced::f_Clear()
	{
		mp_pControl.f_Clear();
	}

	bool CActorFunctorWeakCoalesced::f_IsEmpty() const
	{
		return !mp_pControl || mp_pControl->m_fTarget.f_IsEmpty();
	}

	CActorFunctorWeakCoalesced::operator bool () const
	{
		return !f_IsEmpty();
	}
}
