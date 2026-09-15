// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	DMibImpErrorClassDefine(CExceptionAsyncTimeout, NMib::NException::CException);
#	define DMibErrorAsyncTimeout(d_Description) DMibImpError(NMib::NConcurrency::CExceptionAsyncTimeout, d_Description, false)
#	define DMibErrorInstanceAsyncTimeout(d_Description) DMibImpExceptionInstance(NMib::NConcurrency::CExceptionAsyncTimeout, d_Description, false)

	struct CTimerActor;

	struct CTimerActorHolder : public CDefaultActorHolder
	{
		CTimerActorHolder
			(
				CConcurrencyManager *_pManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, CConcurrencyManager::CQueue *_pQueue
			)
		;

		void f_SetNextElapse(fp64 _NextElapse);
		void f_ClearNextElapse();
		void f_StopTimers();

	private:
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;

		CConcurrencyManager::CQueue *mp_pQueue;
	};


	struct CTimerActor : public CActor
	{
		using CActorHolder = CTimerActorHolder;

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;

		CTimerActor();
		~CTimerActor();

		void f_FireAllTimeouts();
		void f_FireAtExit();
		void f_OneshotTimer(fp64 _Period, int64 _StartTicks, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback, bool _bFireAtExit);
		CActorSubscription f_OneshotTimerAbortable(fp64 _Period, int64 _StartTicks, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);
		CActorSubscription f_RegisterTimer(fp64 _Period, int64 _StartTicks, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);
		CActorSubscription f_RegisterExactTimer(fp64 _Period, int64 _StartTicks, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback);

	private:
		friend struct CTimerActorHolder;
		friend class CConcurrencyManager;

		struct CInternal;

		TCFuture<void> fp_Destroy();
		void fp_PrepareShutdown();
		TCFuture<void> fp_ProcessTimers();
		CTimerActorHolder &fp_GetHolder() const;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};

	struct CTimerActorImpl : public CTimerActor
	{
		static constexpr bool mc_bIsAlwaysAliveImpl = false;
		~CTimerActorImpl();
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
