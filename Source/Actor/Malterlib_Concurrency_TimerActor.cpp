// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Coroutine>
#include <Mib/Concurrency/ActorFunctorWeak>
#include <Mib/Concurrency/ActorSubscription>

namespace NMib::NConcurrency
{
	DMibImpErrorClassImplement(CExceptionAsyncTimeout);

	static int64 fg_FloatToLexicographicInt(fp64 const &_Value)
	{
		int64 Integer = reinterpret_cast<int64 const &>(_Value);

		auto SignBit = int64(1) << fp64::mc_SignBitPos;
		auto SignExtended = (Integer << ((sizeof(int64)*8 - 1) - fp64::mc_SignBitPos)) >> (sizeof(int64)*8 - 1);
		Integer ^= (SignExtended | SignBit);

		return Integer;
	}

	struct CTimerActor::CInternal
	{
		//friend class CConcurrencyManager;
		//template <typename t_CActor>
		//friend class TCActorInternal;

		enum ETimerType
		{
			ETimerType_Oneshot
			, ETimerType_Normal
			, ETimerType_Exact
		};

		struct CTimerSubscriptionState
		{
			~CTimerSubscriptionState();

			TCActorFunctorWeak<TCFuture<void> ()> m_fCallback;
			NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
			TCPromise<void> m_DestroyPromise = CPromiseConstructEmpty();
			bool m_bOutstanding = false;
			bool m_bMissed = false;
			bool m_bDestroying = false;
		};

		struct CTimer
		{
			struct CAVLCompare_NextElapse
			{
				CTimer const &operator ()(CTimer const &_Node) const;
			};

			~CTimer();

			COrdering_Strong operator <=> (CTimer const &_Right) const;

			NIntrusive::TCAVLLink<> m_TreeLink;
			mint m_NextCallbackID = 0;
			NContainer::TCLinkedList<CTimerSubscriptionState> m_Callbacks;
			TCActorFunctorWeak<void ()> m_fOneshotCallback;

			NStorage::TCSharedPointer<bool> m_pDestroyed;
			fp64 m_NextElapse = 0.0;
			int64 m_NextElapseLexicographic = 0;
			fp64 m_Period = 0.0;
			ETimerType m_TimerType = ETimerType_Oneshot;
			bool m_bFireAtExit = false;
		};

		CInternal(CTimerActor *_pThis);

		void fp_CallTimerCallback(CTimer &_Timer, CTimerSubscriptionState &_Callback);
		void fp_CallTimer(CTimer &_Timer);
		void fp_ProcessTimers();
		void fp_StartThread();
		void fp_InsertTimerInQueue(CTimer &_Timer);

		CTimerActor *m_pThis;
		NStorage::TCUniquePointer<NThread::CThreadObject> m_pTimerThread;
		NContainer::TCMap<fp64, CTimer> m_RegisteredTimers;
		NContainer::TCLinkedList<CTimer> m_OneshotTimers;
		NContainer::TCLinkedList<CTimer> m_ExactTimers;

		NIntrusive::TCAVLTree<&CTimer::m_TreeLink, CTimer::CAVLCompare_NextElapse> m_TimerQueue;

		NTime::CClock m_Clock{true};

#if DMibEnableSafeCheck > 0
		mint m_ProcessingThread = 0;
#endif
		align_cacheline NAtomic::TCAtomic<smint> m_WaitTime;
	};
	
	TCActor<CTimerActor> fg_TimerActor()
	{
		return fg_ConcurrencyManager().f_GetTimerActor();
	}

	void CTimerActor::CInternal::fp_InsertTimerInQueue(CTimer &_Timer)
	{
		_Timer.m_NextElapseLexicographic = fg_FloatToLexicographicInt(_Timer.m_NextElapse);
		m_TimerQueue.f_Insert(_Timer);
	}

	void CTimerActor::CInternal::fp_CallTimerCallback(CTimer &_Timer, CTimerSubscriptionState &_Callback)
	{
		_Callback.m_bOutstanding = true;
		_Callback.m_bMissed = false;

		_Callback.m_fCallback() > [this, pTimer = &_Timer, pCallback = &_Callback, pDestroyed = _Callback.m_pDestroyed](auto &&_Result)
			{
				(void)_Result;
				if (*pDestroyed)
					return;

				pCallback->m_bOutstanding = false;
				if (pCallback->m_bDestroying)
				{
					if (pCallback->m_DestroyPromise.f_IsValid() && !pCallback->m_DestroyPromise.f_IsSet())
						pCallback->m_DestroyPromise.f_SetResult();
					return;
				}

				if (pCallback->m_bMissed && m_Clock.f_GetTime() < (pTimer->m_NextElapse - pTimer->m_Period * 0.1))
					fp_CallTimerCallback(*pTimer, *pCallback);
			}
		;
	}

	void CTimerActor::CInternal::fp_CallTimer(CTimer &_Timer)
	{
		for (auto &Callback : _Timer.m_Callbacks)
		{
			if (Callback.m_bOutstanding || Callback.m_bDestroying)
				continue;
			fp_CallTimerCallback(_Timer, Callback);
		}

		if (_Timer.m_fOneshotCallback)
			_Timer.m_fOneshotCallback() > fg_DiscardResult();
	}

	void CTimerActor::CInternal::fp_ProcessTimers()
	{
#if DMibEnableSafeCheck > 0
		m_ProcessingThread = NSys::fg_Thread_GetCurrentUID();
		auto Cleanup
			= fg_OnScopeExit
			(
				[&]
				{
					m_ProcessingThread = 0;
				}
			)
		;
#endif
		auto *pTimer = m_TimerQueue.f_FindSmallest();
		while (pTimer)
		{
			auto &Timer = *pTimer;

			if (Timer.m_NextElapse > m_Clock.f_GetTime())
				break;

			m_TimerQueue.f_Remove(pTimer);

			auto TimerType = Timer.m_TimerType;

			fp_CallTimer(Timer);

			switch (TimerType)
			{
			case ETimerType_Oneshot:
				m_OneshotTimers.f_Remove(Timer);
				break;
			case ETimerType_Exact:
			case ETimerType_Normal:
				auto Period = Timer.m_Period;
				auto Time = m_Clock.f_GetTime();
				while (Timer.m_NextElapse < Time)
				{
					auto Diff = Time - Timer.m_NextElapse;
					if (Diff > Period)
						Timer.m_NextElapse += Diff - Diff.f_Mod(Period) + Period;
					else
						Timer.m_NextElapse += Period;
					Time = m_Clock.f_GetTime();
				}
				fp_InsertTimerInQueue(Timer);
				break;
			}

			pTimer = m_TimerQueue.f_FindSmallest();
		}

		if (m_pTimerThread)
		{
			auto *pTimer = m_TimerQueue.f_FindSmallest();
			if (pTimer)
			{
				aint WaitTime = ((pTimer->m_NextElapse - m_Clock.f_GetTime()) * 1000000.0).f_ToIntRound();
				if (WaitTime == 0)
					WaitTime = -1;
				m_WaitTime.f_Exchange(WaitTime);
				m_pTimerThread->m_EventWantQuit.f_Signal();
			}
		}
	}

	void CTimerActor::CInternal::fp_StartThread()
	{
		if (!m_pTimerThread)
		{
			m_pTimerThread
				= NThread::CThreadObject::fs_StartThread
				(
					[this, ThisActor = fg_ThisActor(m_pThis)](NThread::CThreadObject *_pThread) -> aint
					{
						aint MicroSecondsOld = 0;
						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
						{
							aint MicroSeconds = m_WaitTime.f_Exchange(0);
							if (MicroSeconds == 0)
								MicroSeconds = MicroSecondsOld;
							if (MicroSeconds)
							{
								MicroSecondsOld = 0;
								// Reset semaphore
								if (MicroSeconds < 0 || _pThread->m_EventWantQuit.f_WaitTimeout(fp64(MicroSeconds) / fp64(1000000.0)))
								{
									g_Dispatch(ThisActor) / [this]
										{
											fp_ProcessTimers();
										}
										> fg_DiscardResult()
									;
								}
								else
								{
									MicroSecondsOld = MicroSeconds;
								}
							}
							else
								_pThread->m_EventWantQuit.f_Wait();
						}
						return 0;
					}
					, "Timer actor timeouts"
				)
			;
		}
		fp_ProcessTimers();
	}

	CTimerActor::CTimerActor()
		: mp_pInternal(fg_Construct(this))
	{
	}

	TCFuture<void> CTimerActor::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		Internal.m_pTimerThread.f_Clear();
#if DMibEnableSafeCheck > 0
		Internal.m_ProcessingThread = NSys::fg_Thread_GetCurrentUID();
		auto Cleanup = g_OnScopeExit / [&]
			{
				Internal.m_ProcessingThread = 0;
			}
		;
#endif
		DMibFastCheck(Internal.m_RegisteredTimers.f_IsEmpty());
		DMibFastCheck(Internal.m_ExactTimers.f_IsEmpty());

		Internal.m_TimerQueue.f_Clear();
		Internal.m_OneshotTimers.f_Clear();

		co_return {};
	}

	CTimerActor::~CTimerActor()
	{
		[[maybe_unused]] auto &Internal = *mp_pInternal;

		// All timers must already be removed
		DMibRequire(Internal.m_RegisteredTimers.f_IsEmpty());
		DMibRequire(Internal.m_OneshotTimers.f_IsEmpty());
		DMibRequire(Internal.m_ExactTimers.f_IsEmpty());
	}

	CTimerActor::CInternal::CInternal(CTimerActor *_pThis)
		: m_pThis(_pThis)
	{
	}
	
	CTimerActor::CInternal::CTimerSubscriptionState::~CTimerSubscriptionState()
	{
		*m_pDestroyed = true;
	}

	CTimerActor::CInternal::CTimer::~CTimer()
	{
		if (m_pDestroyed)
			*m_pDestroyed = true;
	}

	COrdering_Strong CTimerActor::CInternal::CTimer::operator <=> (CTimer const &_Right) const
	{
		if (auto Compare = m_NextElapseLexicographic <=> _Right.m_NextElapseLexicographic; Compare != COrdering_Strong::equivalent)
			return Compare;

		return this <=> &_Right;
	}

	CTimerActor::CInternal::CTimer const &CTimerActor::CInternal::CTimer::CAVLCompare_NextElapse::operator ()(CTimer const &_Node) const
	{
		return _Node;
	}

	void CTimerActor::f_FireAllTimeouts()
	{
		auto &Internal = *mp_pInternal;

		NContainer::TCVector<CInternal::CTimer *> ToDeleteOneshot;

		for (auto &Timer : Internal.m_TimerQueue)
		{
			auto TimerType = Timer.m_TimerType;

			Internal.fp_CallTimer(Timer);

			switch (TimerType)
			{
			case CInternal::ETimerType_Oneshot:
				ToDeleteOneshot.f_Insert(&Timer);
				break;
			case CInternal::ETimerType_Exact:
			case CInternal::ETimerType_Normal:
				break;
			}
		}
		
		for (auto &pToDelete : ToDeleteOneshot)
		{
			Internal.m_TimerQueue.f_Remove(pToDelete);
			Internal.m_OneshotTimers.f_Remove(*pToDelete);
		}
	}

	void CTimerActor::f_FireAtExit()
	{
		auto &Internal = *mp_pInternal;

		NContainer::TCVector<CInternal::CTimer *> ToDeleteOneshot;

		for (auto &Timer : Internal.m_TimerQueue)
		{
			if (!Timer.m_bFireAtExit)
				continue;

			auto TimerType = Timer.m_TimerType;

			Internal.fp_CallTimer(Timer);

			switch (TimerType)
			{
			case CInternal::ETimerType_Oneshot:
				ToDeleteOneshot.f_Insert(&Timer);
				break;
			case CInternal::ETimerType_Exact:
			case CInternal::ETimerType_Normal:
				break;
			}
		}

		for (auto &pToDelete : ToDeleteOneshot)
		{
			Internal.m_TimerQueue.f_Remove(pToDelete);
			Internal.m_OneshotTimers.f_Remove(*pToDelete);
		}
	}

	void CTimerActor::f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_Actor, NFunction::TCFunctionMovable<void ()> &&_fCallback, bool _bFireAtExit)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_OneshotTimers.f_Insert();

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);
		Timer.m_bFireAtExit = _bFireAtExit;

		Internal.fp_InsertTimerInQueue(Timer);

		// This makes it self-referencing, so it will not be deleted until the callback is run. Because the callback handler saves the actor weakly it will not prevent deletion of
		// _Actor unless it's referenced from within _fCallback

		Timer.m_fOneshotCallback = g_ActorFunctorWeak(_Actor) / fg_Move(_fCallback);

		Internal.fp_StartThread();
	}

	CActorSubscription CTimerActor::f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_Actor, NFunction::TCFunctionMovable<void ()> &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_OneshotTimers.f_Insert();

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);

		Internal.fp_InsertTimerInQueue(Timer);

		CInternal::CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		Timer.m_fOneshotCallback = g_ActorFunctorWeak(_Actor) / fg_Move(_fCallback);

		Internal.fp_StartThread();

		return g_ActorSubscription / [this, pTimer, pDestroyed]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;
				if (*pDestroyed)
					co_return {};

				auto ToDestroy = fg_Move(pTimer->m_fOneshotCallback);
				if (pTimer->m_TreeLink.f_IsInTree())
					Internal.m_TimerQueue.f_Remove(pTimer);
				Internal.m_OneshotTimers.f_Remove(*pTimer);

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
	}

	CActorSubscription CTimerActor::f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		auto &Timer = *Internal.m_RegisteredTimers(_Period);

		if (Timer.m_Period == 0.0)
		{
			Timer.m_Period = _Period;
			Timer.m_NextElapse = ((Internal.m_Clock.f_GetTime() + _Period) / _Period).f_Floor() * _Period;
			Timer.m_TimerType = CInternal::ETimerType_Normal;
			Timer.m_pDestroyed = fg_Construct(false);
			Internal.fp_InsertTimerInQueue(Timer);
		}

		auto &Callback = Timer.m_Callbacks.f_Insert();
		Callback.m_fCallback = g_ActorFunctorWeak(_Actor) / fg_Move(_fCallback);

		Internal.fp_StartThread();

		return g_ActorSubscription / [this, pTimer = &Timer, &Callback, pDestroyed = Callback.m_pDestroyed]() -> TCFuture<void>
			{
				if (*pDestroyed)
					co_return {};

				Callback.m_bDestroying = true;

				if (Callback.m_bOutstanding)
				{
					Callback.m_DestroyPromise = {};
					co_await Callback.m_DestroyPromise.f_Future();
				}

				auto &Internal = *mp_pInternal;

				auto ToDestroy = fg_Move(Callback.m_fCallback);

				pTimer->m_Callbacks.f_Remove(Callback);

				if (pTimer->m_Callbacks.f_IsEmpty())
				{
					if (pTimer->m_TreeLink.f_IsInTree())
						Internal.m_TimerQueue.f_Remove(pTimer);
					Internal.m_RegisteredTimers.f_Remove(pTimer);
				}

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
	}

	CActorSubscription CTimerActor::f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_Actor, FUnitVoidFutureFunction &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_ExactTimers.f_Insert();

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Exact;
		Timer.m_pDestroyed = fg_Construct(false);

		Internal.fp_InsertTimerInQueue(Timer);

		auto &Callback = Timer.m_Callbacks.f_Insert();
		Callback.m_fCallback = g_ActorFunctorWeak(_Actor) / fg_Move(_fCallback);

		Internal.fp_StartThread();

		return g_ActorSubscription / [this, pTimer = &Timer, &Callback, pDestroyed = Callback.m_pDestroyed]() -> TCFuture<void>
			{
				if (*pDestroyed)
					co_return {};

				Callback.m_bDestroying = true;
				if (Callback.m_bOutstanding)
				{
					Callback.m_DestroyPromise = {};
					co_await Callback.m_DestroyPromise.f_Future();
				}

				auto &Internal = *mp_pInternal;

				auto ToDestroy = fg_Move(Callback.m_fCallback);

				if (pTimer->m_TreeLink.f_IsInTree())
					Internal.m_TimerQueue.f_Remove(pTimer);
				Internal.m_ExactTimers.f_Remove(*pTimer);

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
	}

	CTimeoutHelper::CTimeoutHelper(fp64 _Period, bool _bFireAtExit)
		: mp_Period(_Period)
		, mp_bFireAtExit(_bFireAtExit)
	{
	}

	CTimeoutHelper &CTimeoutHelper::operator ()(TCActor<CActor> const &_DispatchActor)
	{
		mp_DispatchActor = _DispatchActor;
		return *this;
	}

	void CTimeoutHelper::operator > (NFunction::TCFunctionMovable<void ()> &&_fOnTimeout) const
	{
		fg_TimerActor()
			(
				&CTimerActor::f_OneshotTimer
				, mp_Period
				, mp_DispatchActor ? mp_DispatchActor : fg_CurrentActor()
				, fg_Move(_fOnTimeout)
				, mp_bFireAtExit
			)
			> fg_DiscardResult();
		;
	}

	TCFuture<void> CTimeoutHelper::f_Dispatch()
	{
		TCPromise<void> Promise;
		fg_OneshotTimer
			(
				mp_Period
				, [Promise]
				{
					Promise.f_SetResult();
				}
				, fg_DirectCallActor()
				, mp_bFireAtExit
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFutureAwaiter<void, true, void *> CTimeoutHelper::operator co_await()
	{
		TCPromise<void> Promise;
		fg_TimerActor()
			(
				&CTimerActor::f_OneshotTimer
				, mp_Period
				, fg_CurrentActor()
				, [Promise]
				{
					Promise.f_SetResult();
				}
				, mp_bFireAtExit
			)
			> fg_DiscardResult();
		;
		return Promise.f_MoveFuture().operator co_await();
	}

	CTimeoutHelper fg_Timeout(fp64 _Period, bool _bFireAtExit)
	{
		DMibFastCheck(_Period >= 0.001);

		return CTimeoutHelper(_Period, _bFireAtExit);
	}

	void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMovable<void ()> &&_fCallback, TCActor<CActor> const &_Actor, bool _bFireAtExit)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> Actor = _Actor;
		if (!Actor)
			Actor = fg_CurrentActor();

		fg_TimerActor()
			(
				&CTimerActor::f_OneshotTimer
				, _Period
				, fg_Move(Actor)
				, fg_Move(_fCallback)
				, _bFireAtExit
			)
			> fg_DiscardResult();
		;
	}

	TCFuture<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, NFunction::TCFunctionMovable<void ()> &&_fCallback, TCActor<CActor> const &_Actor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> Actor = _Actor;
		if (!Actor)
			Actor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, Actor = fg_Move(Actor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_OneshotTimerAbortable
							, _Period
							, Actor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCFuture<CActorSubscription> fg_RegisterTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> Actor = _Actor;
		if (!Actor)
			Actor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, Actor = fg_Move(Actor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterTimer
							, _Period
							, Actor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCFuture<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_Actor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> Actor = _Actor;
		if (!Actor)
			Actor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, Actor = fg_Move(Actor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterExactTimer
							, _Period
							, Actor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCFuture<CTimeoutAbortable> fg_TimeoutAbortable(fp64 _Period)
	{
		TCPromise<void> Promise;
		CTimeoutAbortable Return;

		Return.m_Future = Promise.f_Future();

		struct CPromiseHolder
		{
			CPromiseHolder(TCPromise<void> &&_Promise)
				: m_Promise(fg_Move(_Promise))
			{
			}
			CPromiseHolder(CPromiseHolder &&) = default;
			CPromiseHolder(CPromiseHolder const &) = delete;

			~CPromiseHolder()
			{
				if (m_Promise.f_IsValid())
					m_Promise.f_SetException(DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "Timer aborted", false));
			}

			TCPromise<void> m_Promise;
		};

		Return.m_Subscription = co_await fg_OneshotTimerAbortable
			(
				_Period
				, [PromiseHolder = CPromiseHolder(fg_Move(Promise))]() mutable
				{
					PromiseHolder.m_Promise.f_SetResult();
					PromiseHolder.m_Promise = CPromiseConstructEmpty();
				}
			)
		;

		co_return fg_Move(Return);
	}
}
