// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/Coroutine>

namespace NMib::NConcurrency
{
	TCActor<CTimerActor> fg_TimerActor()
	{
		return fg_ConcurrencyManager().f_GetTimerActor();
	}

	void CTimerActor::fp_CallTimer(CTimer &_Timer, bool _bOnlyMissed)
	{
		_Timer.m_Callbacks.f_CallEach
			(
				[&](TCDispatchedActorCall<void> &&_ActorCall, CTimerSubscriptionState &_HandleInfo)
				{
					if (_bOnlyMissed)
					{
						if (!_HandleInfo.m_bMissed || _HandleInfo.m_bOutstanding)
						{
							_ActorCall.f_Clear();
							return;
						}
					}
					else
					{
						if (_HandleInfo.m_bOutstanding)
						{
							_HandleInfo.m_bMissed = true;
							_ActorCall.f_Clear();
							return;
						}
					}
					_HandleInfo.m_bOutstanding = true;
					_ActorCall > [this, pTimer = &_Timer, pHandleInfo = &_HandleInfo, pDestroyed = _HandleInfo.m_pDestroyed](auto &&)
						{
							if (*pDestroyed)
								return;

							pHandleInfo->m_bOutstanding = false;

							if (pHandleInfo->m_bMissed)
								fp_CallTimer(*pTimer, true);
						}
					;
				}
			)
		;

		if (_bOnlyMissed)
			return;

		_Timer.m_OneshotCallbacks() > fg_DiscardResult();
	}

	void CTimerActor::fp_ProcessTimers()
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
		auto *pTimer = m_TimerQueue.f_GetFirst();
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
				while (Timer.m_NextElapse < m_Clock.f_GetTime())
					Timer.m_NextElapse.f_Get() += Timer.m_Period;
				m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);
				break;
			}

			pTimer = m_TimerQueue.f_GetFirst();
		}

		if (m_pTimerThread)
		{
			auto *pTimer = m_TimerQueue.f_GetFirst();
			if (pTimer)
			{
				aint WaitTime = ((pTimer->m_NextElapse.f_Get() - m_Clock.f_GetTime()) * 1000000.0).f_ToIntRound();
				if (WaitTime == 0)
					WaitTime = -1;
				m_WaitTime.f_Exchange(WaitTime);
				m_pTimerThread->m_EventWantQuit.f_Signal();
			}
		}
	}

	void CTimerActor::fp_StartThread()
	{
		if (!m_pTimerThread)
		{
			m_pTimerThread
				= NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
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
									fg_ThisActor(this)(&CTimerActor::fp_ProcessTimers) > fg_DiscardResult();
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
					, "Concurrent timer"
				)
			;
		}
		fp_ProcessTimers();
	}

	CTimerActor::CTimerActor()
#if DMibEnableSafeCheck > 0
		: m_ProcessingThread(0)
#endif
	{
		m_Clock.f_Start();
	}

	TCFuture<void> CTimerActor::fp_Destroy()
	{
		m_pTimerThread.f_Clear();
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
		DMibFastCheck(m_RegisteredTimers.f_IsEmpty());
		DMibFastCheck(m_ExactTimers.f_IsEmpty());

		m_OneshotTimers.f_Clear();

		return fg_Explicit();
	}

	CTimerActor::~CTimerActor()
	{
		// All timers must already be removed
		DMibRequire(m_RegisteredTimers.f_IsEmpty());
		DMibRequire(m_OneshotTimers.f_IsEmpty());
		DMibRequire(m_ExactTimers.f_IsEmpty());
	}

	CTimerActor::CTimerSubscriptionState::CTimerSubscriptionState(NFunction::TCFunctionMutable<void ()> &&_fCleanup)
		: m_fCleanup(fg_Move(_fCleanup))
	{
	}

	CTimerActor::CTimerSubscriptionState::~CTimerSubscriptionState()
	{
		*m_pDestroyed = true;

		if (m_fCleanup)
			m_fCleanup();
	}

	CTimerActor::CTimer::CTimer(CActor *_pThisActor)
		: m_Callbacks(_pThisActor, false)
		, m_OneshotCallbacks(_pThisActor, false)
	{
	}

	CTimerActor::CTimer::~CTimer()
	{
		if (m_pDestroyed)
			*m_pDestroyed = true;
	}

	void CTimerActor::f_FireAllTimeouts()
	{
		for (auto iTimer = m_TimerQueue.f_GetIterator(); iTimer; ++iTimer)
		{
			auto &Timer = *iTimer;
			++iTimer;

			auto TimerType = Timer.m_TimerType;

			fp_CallTimer(Timer);

			switch (TimerType)
			{
			case ETimerType_Oneshot:
				m_TimerQueue.f_Remove(&Timer);
				m_OneshotTimers.f_Remove(Timer);
				break;
			case ETimerType_Exact:
			case ETimerType_Normal:
				break;
			}
		}
	}

	void CTimerActor::f_FireAtExit()
	{
		for (auto iTimer = m_TimerQueue.f_GetIterator(); iTimer; ++iTimer)
		{
			auto &Timer = *iTimer;
			++iTimer;

			if (!Timer.m_bFireAtExit)
				continue;

			auto TimerType = Timer.m_TimerType;

			fp_CallTimer(Timer);

			switch (TimerType)
			{
			case ETimerType_Oneshot:
				m_TimerQueue.f_Remove(&Timer);
				m_OneshotTimers.f_Remove(Timer);
				break;
			case ETimerType_Exact:
			case ETimerType_Normal:
				break;
			}
		}
	}

	void CTimerActor::f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback, bool _bFireAtExit)
	{
		DMibRequire(_Period > 0.0);

		CTimer &Timer = m_OneshotTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);
		Timer.m_bFireAtExit = _bFireAtExit;

		m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		// This makes it self-referencing, so it will not be deleted until the callback is run. Because the callback handler saves the actor weakly it will not prevent deletion of
		// _pActor unless it's referenced from within _fCallback
		NStorage::TCSharedPointer<NStorage::TCSharedPointer<CActorSubscription>> pCallbackHandle = fg_Construct(fg_Construct());

		**pCallbackHandle = Timer.m_OneshotCallbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed, pCallbackHandle]
				{
					if (!*pDestroyed)
						m_OneshotTimers.f_Remove(*pTimer);
				}
			)
		;

		fp_StartThread();
	}

	CActorSubscription CTimerActor::f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback)
	{
		DMibRequire(_Period > 0.0);
		CTimer &Timer = m_OneshotTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);

		m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_OneshotCallbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					if (!*pDestroyed)
						m_OneshotTimers.f_Remove(*pTimer);
				}
			)
		;

		fp_StartThread();

		return fg_Move(pCallbackHandle);
	}

	CActorSubscription CTimerActor::f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCFuture<void> ()> &&_fCallback)
	{
		DMibRequire(_Period > 0.0);
		auto &Timer = *m_RegisteredTimers(_Period, this);

		if (Timer.m_Period == 0.0)
		{
			Timer.m_Period = _Period;
			Timer.m_NextElapse = ((m_Clock.f_GetTime() + _Period) / _Period).f_Floor() * _Period;
			Timer.m_TimerType = ETimerType_Normal;
			Timer.m_pDestroyed = fg_Construct(false);
			m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);
		}

		CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_Callbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					if (!*pDestroyed)
					{
						if (pTimer->m_Callbacks.f_IsEmpty())
							m_RegisteredTimers.f_Remove(pTimer);
					}
				}
			)
		;

		fp_StartThread();

		return fg_Move(pCallbackHandle);
	}

	CActorSubscription CTimerActor::f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCFuture<void> ()> &&_fCallback)
	{
		DMibRequire(_Period > 0.0);
		CTimer &Timer = m_ExactTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = ETimerType_Exact;
		Timer.m_pDestroyed = fg_Construct(false);

		m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_Callbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					if (!*pDestroyed)
					{
						m_ExactTimers.f_Remove(*pTimer);
					}
				}
			)
		;

		fp_StartThread();

		return fg_Move(pCallbackHandle);
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

	void CTimeoutHelper::operator > (NFunction::TCFunctionMutable<void ()> &&_fOnTimeout) const
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
		return Promise.operator co_await();
	}

	CTimeoutHelper fg_Timeout(fp64 _Period, bool _bFireAtExit)
	{
		return CTimeoutHelper(_Period, _bFireAtExit);
	}

	void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMutable<void ()> &&_fCallback, TCActor<CActor> const &_pActor, bool _bFireAtExit)
	{
		TCActor<CActor> pActor = _pActor;
		if (!pActor)
			pActor = fg_CurrentActor();
		fg_TimerActor()
			(
				&CTimerActor::f_OneshotTimer
				, _Period
				, fg_Move(pActor)
				, fg_Move(_fCallback)
				, _bFireAtExit
			)
			> fg_DiscardResult();
		;
	}

	TCDispatchedActorCall<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback)
	{
		return fg_DirectDispatch
			(
				[_Period, _pActor, fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_OneshotTimerAbortable
							, _Period
							, _pActor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCDispatchedActorCall<CActorSubscription> fg_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCFuture<void> ()> &&_fCallback)
	{
		return fg_DirectDispatch
			(
				[_Period, _pActor, fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterTimer
							, _Period
							, _pActor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCDispatchedActorCall<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCFuture<void> ()> &&_fCallback)
	{
		return fg_DirectDispatch
			(
				[_Period, _pActor, fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterExactTimer
							, _Period
							, _pActor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}
}
