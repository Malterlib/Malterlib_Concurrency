// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Coroutine>

namespace NMib::NConcurrency
{
	DMibImpErrorClassImplement(CExceptionAsyncTimeout);

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
			CTimerSubscriptionState(NFunction::TCFunctionMovable<void ()> &&_fCleanup);
			~CTimerSubscriptionState();

			NFunction::TCFunctionMovable<void ()> m_fCleanup;
			NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
			bool m_bOutstanding = false;
			bool m_bMissed = false;
		};

		struct CTimer
		{
			CTimer(CActor *_pThisActor);
			~CTimer();

			DMibListLinkDS_Link(CTimer, m_Link);
			TCActorSubscriptionManager<TCFuture<void> (), true, CTimerSubscriptionState> m_Callbacks;
			TCActorSubscriptionManager<void (), true, CTimerSubscriptionState> m_OneshotCallbacks;
			NStorage::TCSharedPointer<bool> m_pDestroyed;
			fp64 m_NextElapse = 0.0;
			fp64 m_Period = 0.0;
			ETimerType m_TimerType;
			bool m_bFireAtExit = false;

			COrdering_Partial operator <=> (CTimer const &_Right) const
			{
				return m_NextElapse <=> _Right.m_NextElapse;
			}
		};

		CInternal(CTimerActor *_pThis)
			: m_pThis(_pThis)
		{
		}

		void fp_CallTimerCallback(CTimer &_Timer, CTimerSubscriptionState &_HandleInfo);
		void fp_CallTimer(CTimer &_Timer);
		void fp_ProcessTimers();
		void fp_StartThread();

		CTimerActor *m_pThis;
		NStorage::TCUniquePointer<NThread::CThreadObject> m_pTimerThread;
		NContainer::TCMap<fp64, CTimer> m_RegisteredTimers;
		NContainer::TCLinkedList<CTimer> m_OneshotTimers;
		NContainer::TCLinkedList<CTimer> m_ExactTimers;
		align_cacheline NAtomic::TCAtomic<smint> m_WaitTime;

		DMibListLinkDS_List(CTimer, m_Link) m_TimerQueue;

		NTime::CClock m_Clock{true};

#if DMibEnableSafeCheck > 0
		mint m_ProcessingThread = 0;
#endif
	};
	
	TCActor<CTimerActor> fg_TimerActor()
	{
		return fg_ConcurrencyManager().f_GetTimerActor();
	}

	void CTimerActor::CInternal::fp_CallTimerCallback(CTimer &_Timer, CTimerSubscriptionState &_HandleInfo)
	{
		_Timer.m_Callbacks.f_CallSpecific
			(
				[&](auto &&_fActorCall, CTimerSubscriptionState &_HandleInfo)
				{
					_HandleInfo.m_bOutstanding = true;
					_HandleInfo.m_bMissed = false;
					_fActorCall() > [this, pTimer = &_Timer, pHandleInfo = &_HandleInfo, pDestroyed = _HandleInfo.m_pDestroyed](auto &&_Result)
						{
							(void)_Result;
							if (*pDestroyed)
								return;

							pHandleInfo->m_bOutstanding = false;

							if (pHandleInfo->m_bMissed && m_Clock.f_GetTime() < (pTimer->m_NextElapse - pTimer->m_Period * 0.1) )
								fp_CallTimerCallback(*pTimer, *pHandleInfo);
						}
					;
				}
			 	, _HandleInfo
			)
		;
	}

	void CTimerActor::CInternal::fp_CallTimer(CTimer &_Timer)
	{
		_Timer.m_Callbacks.f_CallEach
			(
				[&](auto &&_fActorCall, CTimerSubscriptionState &_HandleInfo)
				{
					if (_HandleInfo.m_bOutstanding)
					{
						_HandleInfo.m_bMissed = true;
						return;
					}

					_HandleInfo.m_bOutstanding = true;
					_HandleInfo.m_bMissed = false;
					_fActorCall() > [this, pTimer = &_Timer, pHandleInfo = &_HandleInfo, pDestroyed = _HandleInfo.m_pDestroyed](auto &&_Result)
						{
							(void)_Result;
							if (*pDestroyed)
								return;

							pHandleInfo->m_bOutstanding = false;

							if (pHandleInfo->m_bMissed && m_Clock.f_GetTime() < (pTimer->m_NextElapse - pTimer->m_Period * 0.1) )
								fp_CallTimerCallback(*pTimer, *pHandleInfo);
						}
					;
				}
			)
		;

		_Timer.m_OneshotCallbacks() > fg_DiscardResult();
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

	CTimerActor::CInternal::CTimerSubscriptionState::CTimerSubscriptionState(NFunction::TCFunctionMovable<void ()> &&_fCleanup)
		: m_fCleanup(fg_Move(_fCleanup))
	{
	}

	CTimerActor::CInternal::CTimerSubscriptionState::~CTimerSubscriptionState()
	{
		*m_pDestroyed = true;

		if (m_fCleanup)
			m_fCleanup();
	}

	CTimerActor::CInternal::CTimer::CTimer(CActor *_pThisActor)
		: m_Callbacks(_pThisActor, false)
		, m_OneshotCallbacks(_pThisActor, false)
	{
	}

	CTimerActor::CInternal::CTimer::~CTimer()
	{
		if (m_pDestroyed)
			*m_pDestroyed = true;
	}

	void CTimerActor::f_FireAllTimeouts()
	{
		auto &Internal = *mp_pInternal;

		for (auto iTimer = Internal.m_TimerQueue.f_GetIterator(); iTimer; ++iTimer)
		{
			auto &Timer = *iTimer;
			++iTimer;

			auto TimerType = Timer.m_TimerType;

			Internal.fp_CallTimer(Timer);

			switch (TimerType)
			{
			case CInternal::ETimerType_Oneshot:
				Internal.m_TimerQueue.f_Remove(&Timer);
				Internal.m_OneshotTimers.f_Remove(Timer);
				break;
			case CInternal::ETimerType_Exact:
			case CInternal::ETimerType_Normal:
				break;
			}
		}
	}

	void CTimerActor::f_FireAtExit()
	{
		auto &Internal = *mp_pInternal;

		for (auto iTimer = Internal.m_TimerQueue.f_GetIterator(); iTimer; ++iTimer)
		{
			auto &Timer = *iTimer;
			++iTimer;

			if (!Timer.m_bFireAtExit)
				continue;

			auto TimerType = Timer.m_TimerType;

			Internal.fp_CallTimer(Timer);

			switch (TimerType)
			{
			case CInternal::ETimerType_Oneshot:
				Internal.m_TimerQueue.f_Remove(&Timer);
				Internal.m_OneshotTimers.f_Remove(Timer);
				break;
			case CInternal::ETimerType_Exact:
			case CInternal::ETimerType_Normal:
				break;
			}
		}
	}

	void CTimerActor::f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMovable<void ()> &&_fCallback, bool _bFireAtExit)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_OneshotTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);
		Timer.m_bFireAtExit = _bFireAtExit;

		Internal.m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CInternal::CTimer *pTimer = &Timer;
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
					auto &Internal = *mp_pInternal;
					if (!*pDestroyed)
						Internal.m_OneshotTimers.f_Remove(*pTimer);
				}
			)
		;

		Internal.fp_StartThread();
	}

	CActorSubscription CTimerActor::f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMovable<void ()> &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_OneshotTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Oneshot;
		Timer.m_pDestroyed = fg_Construct(false);

		Internal.m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CInternal::CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_OneshotCallbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					auto &Internal = *mp_pInternal;
					if (!*pDestroyed)
						Internal.m_OneshotTimers.f_Remove(*pTimer);
				}
			)
		;

		Internal.fp_StartThread();

		return fg_Move(pCallbackHandle);
	}

	CActorSubscription CTimerActor::f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, FUnitVoidFutureFunction &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		auto &Timer = *Internal.m_RegisteredTimers(_Period, this);

		if (Timer.m_Period == 0.0)
		{
			Timer.m_Period = _Period;
			Timer.m_NextElapse = ((Internal.m_Clock.f_GetTime() + _Period) / _Period).f_Floor() * _Period;
			Timer.m_TimerType = CInternal::ETimerType_Normal;
			Timer.m_pDestroyed = fg_Construct(false);
			Internal.m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);
		}

		CInternal::CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_Callbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					auto &Internal = *mp_pInternal;
					if (!*pDestroyed)
					{
						if (pTimer->m_Callbacks.f_IsEmpty())
							Internal.m_RegisteredTimers.f_Remove(pTimer);
					}
				}
			)
		;

		Internal.fp_StartThread();

		return fg_Move(pCallbackHandle);
	}

	CActorSubscription CTimerActor::f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, FUnitVoidFutureFunction &&_fCallback)
	{
		DMibFastCheck(_Period >= 0.001);

		auto &Internal = *mp_pInternal;

		CInternal::CTimer &Timer = Internal.m_ExactTimers.f_Insert(fg_Construct(this));

		Timer.m_Period = _Period;
		Timer.m_NextElapse = Internal.m_Clock.f_GetTime() + _Period;
		Timer.m_TimerType = CInternal::ETimerType_Exact;
		Timer.m_pDestroyed = fg_Construct(false);

		Internal.m_TimerQueue.f_InsertSorted<CCompare_Default>(Timer);

		CInternal::CTimer *pTimer = &Timer;
		auto pDestroyed = Timer.m_pDestroyed;

		auto pCallbackHandle = Timer.m_Callbacks.f_Register
			(
				_pActor
				, fg_Move(_fCallback)
				, [this, pTimer, pDestroyed]
				{
					auto &Internal = *mp_pInternal;
					if (!*pDestroyed)
					{
						Internal.m_ExactTimers.f_Remove(*pTimer);
					}
				}
			)
		;

		Internal.fp_StartThread();

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

	void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMovable<void ()> &&_fCallback, TCActor<CActor> const &_pActor, bool _bFireAtExit)
	{
		DMibFastCheck(_Period >= 0.001);

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

	TCFuture<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, NFunction::TCFunctionMovable<void ()> &&_fCallback, TCActor<CActor> const &_pActor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> pActor = _pActor;
		if (!pActor)
			pActor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, pActor = fg_Move(pActor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_OneshotTimerAbortable
							, _Period
							, pActor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCFuture<CActorSubscription> fg_RegisterTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_pActor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> pActor = _pActor;
		if (!pActor)
			pActor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, pActor = fg_Move(pActor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterTimer
							, _Period
							, pActor
							, fg_Move(fCallback)
						)
						> Promise;
					;
					return Promise.f_MoveFuture();
				}
			)
		;
	}

	TCFuture<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, FUnitVoidFutureFunction &&_fCallback, TCActor<CActor> const &_pActor)
	{
		DMibFastCheck(_Period >= 0.001);

		TCActor<CActor> pActor = _pActor;
		if (!pActor)
			pActor = fg_CurrentActor();

		return g_Future <<= fg_UnsafeDirectDispatch
			(
				[_Period, pActor = fg_Move(pActor), fCallback = fg_Move(_fCallback)]() mutable -> TCFuture<CActorSubscription>
				{
					TCPromise<CActorSubscription> Promise;
					fg_TimerActor()
						(
							&CTimerActor::f_RegisterExactTimer
							, _Period
							, pActor
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
