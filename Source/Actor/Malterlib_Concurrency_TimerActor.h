// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	DMibImpErrorClassDefine(CExceptionAsyncTimeout, NMib::NException::CException);
#	define DMibErrorAsyncTimeout(d_Description) DMibImpError(NMib::NConcurrency::CExceptionAsyncTimeout, d_Description, false)
#	define DMibErrorInstanceAsyncTimeout(d_Description) DMibImpExceptionInstance(NMib::NConcurrency::CExceptionAsyncTimeout, d_Description, false)

	struct CTimerActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;
		CTimerActor();
		~CTimerActor();

		void f_FireAllTimeouts();
		void f_FireAtExit();
		void f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback, bool _bFireAtExit);
		CActorSubscription f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);
		CActorSubscription f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);
		CActorSubscription f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);

	private:
		struct CInternal;

		TCFuture<void> fp_Destroy();

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};

	struct CTimeoutHelper
	{
		CTimeoutHelper(fp64 _Period, bool _bFireAtExit);
		CTimeoutHelper &operator ()(TCActor<CActor> const &_DispatchActor);

		void operator > (FUnitVoidFutureFunction &&_fOnTimeout) const;

		TCFutureAwaiter<void, true, CVoidTag> operator co_await();

		TCFuture<void> f_Dispatch();

	private:
		fp64 mp_Period;
		TCActor<CActor> mp_DispatchActor;
		bool mp_bFireAtExit = false;
	};

	CTimeoutHelper fg_Timeout(fp64 _Period, bool _bFireAtExit = true);

	struct CTimeoutAbortable
	{
		CActorSubscription m_Subscription;
		TCFuture<void> m_Future;
	};

	TCFuture<CTimeoutAbortable> fg_TimeoutAbortable(fp64 _Period);

	void fg_OneshotTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor = nullptr, bool _bFireAtExit = true);
	TCFuture<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor = nullptr);
	TCFuture<CActorSubscription> fg_RegisterTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor = nullptr);
	TCFuture<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor = nullptr);
}

#include "Malterlib_Concurrency_TimerActor.hpp"
