// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>

namespace NMib::NConcurrency
{
	struct CTimerActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;
		CTimerActor();
		~CTimerActor();

		void f_FireAllTimeouts();
		void f_FireAtExit();
		void f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback, bool _bFireAtExit);
		CActorSubscription f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
		CActorSubscription f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, FUnitVoidFutureFunction &&_fCallback);
		CActorSubscription f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, FUnitVoidFutureFunction &&_fCallback);

	private:
		struct CInternal;

		TCFuture<void> fp_Destroy();

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};

	struct CTimeoutHelper
	{
		CTimeoutHelper(fp64 _Period, bool _bFireAtExit);
		CTimeoutHelper &operator ()(TCActor<CActor> const &_DispatchActor);

		void operator > (NFunction::TCFunctionMutable<void ()> &&_fOnTimeout) const;

		TCFutureAwaiter<void, true, void *> operator co_await();

	private:
		fp64 mp_Period;
		TCActor<CActor> mp_DispatchActor;
		bool mp_bFireAtExit = false;
	};

	CTimeoutHelper fg_Timeout(fp64 _Period, bool _bFireAtExit = true);
	void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMutable<void ()> &&_fCallback, TCActor<CActor> const &_pActor = nullptr, bool _bFireAtExit = true);
	TCFuture<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, NFunction::TCFunctionMutable<void ()> &&_fCallback, TCActor<CActor> const &_pActor = nullptr);
	TCFuture<CActorSubscription> fg_RegisterTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_pActor = nullptr);
	TCFuture<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_pActor = nullptr);
}

#include "Malterlib_Concurrency_TimerActor.hpp"
