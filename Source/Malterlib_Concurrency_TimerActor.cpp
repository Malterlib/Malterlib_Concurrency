// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		TCActor<CTimerActor> fg_TimerActor()
		{
			return fg_ConcurrencyManager().f_GetTimerActor();
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

				Timer.m_Callbacks();

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

		TCContinuation<void> CTimerActor::f_Destroy()
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

			return TCContinuation<void>::fs_Finished();
		}

		CTimerActor::~CTimerActor()
		{
			// All timers must already be removed
			DMibRequire(m_RegisteredTimers.f_IsEmpty());
			DMibRequire(m_OneshotTimers.f_IsEmpty());
			DMibRequire(m_ExactTimers.f_IsEmpty());
		}

		CTimerActor::CTimerHandleCleanup::CTimerHandleCleanup(NFunction::TCFunction<void (NFunction::CThisTag &)> const &_fCleanup)
			: m_fCleanup(_fCleanup)
		{
		}
		
		CTimerActor::CTimerHandleCleanup::~CTimerHandleCleanup()
		{
			if (m_fCleanup)
				m_fCleanup();
		}

		CTimerActor::CTimer::CTimer(CActor *_pThisActor)
			: m_Callbacks(_pThisActor, false)
		{
		}

		CTimerActor::CTimer::~CTimer()
		{
			if (m_pDestroyed)
				*m_pDestroyed = true;
		}

		void CTimerActor::f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
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

			// This makes it self-referencing, so it will not be deleted until the callback is run. Because the callback handler saves the actor weakly it will not prevent deletion of
			// _pActor unless it's referenced from within _fCallback
			NPtr::TCSharedPointer<NPtr::TCSharedPointer<CActorCallback>> pCallbackHandle = fg_Construct(fg_Construct());

			**pCallbackHandle = Timer.m_Callbacks.f_Register
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

		CActorCallback CTimerActor::f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
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

			auto pCallbackHandle = Timer.m_Callbacks.f_Register
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

		CActorCallback CTimerActor::f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
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

		CActorCallback CTimerActor::f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
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
		
		TCDispatchedActorCall<void> fg_Timeout(fp64 _Period)
		{
			return fg_ConcurrentDispatch
				(
					[_Period]()
					{
						TCContinuation<void> Continuation;
						fg_TimerActor()
							(
								&CTimerActor::f_OneshotTimer
								, _Period
								, fg_AnyConcurrentActor()
								, [Continuation]
								{
									Continuation.f_SetResult();
								}
							)
							> fg_DiscardResult();
						;
						return Continuation;
					}
				)
			;
		}
		
		void fg_OneshotTimer(fp64 _Period, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback, TCActor<CActor> const &_pActor)
		{
			TCActor<CActor> pActor = _pActor;
			if (!pActor)
				pActor = fg_ConcurrencyManager().f_CurrentActor();
			fg_TimerActor()
				(
					&CTimerActor::f_OneshotTimer
					, _Period
					, fg_Move(pActor)
					, fg_Move(_fCallback)
				)
				> fg_DiscardResult();
			;
		}
		
		TCDispatchedActorCall<CActorCallback> fg_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
		{
			return fg_ConcurrentDispatch
				(
					[_Period, _pActor, fCallback = fg_LambdaMove(_fCallback)]()
					{
						TCContinuation<CActorCallback> Continuation;
						fg_TimerActor()
							(
								&CTimerActor::f_OneshotTimerAbortable
								, _Period
								, _pActor
								, [fCallback]() mutable
								{
									(*fCallback)();
								}
							)
							> Continuation;
						;
						return Continuation;
					}
				)
			;
		}

		TCDispatchedActorCall<CActorCallback> fg_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
		{
			return fg_ConcurrentDispatch
				(
					[_Period, _pActor, fCallback = fg_LambdaMove(_fCallback)]()
					{
						TCContinuation<CActorCallback> Continuation;
						fg_TimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, _Period
								, _pActor
								, [fCallback]() mutable
								{
									(*fCallback)();
								}
							)
							> Continuation;
						;
						return Continuation;
					}
				)
			;
		}
		
		TCDispatchedActorCall<CActorCallback> fg_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void (NFunction::CThisTag &)> && _fCallback)
		{
			return fg_ConcurrentDispatch
				(
					[_Period, _pActor, fCallback = fg_LambdaMove(_fCallback)]()
					{
						TCContinuation<CActorCallback> Continuation;
						fg_TimerActor()
							(
								&CTimerActor::f_RegisterExactTimer
								, _Period
								, _pActor
								, [fCallback]() mutable
								{
									(*fCallback)();
								}
							)
							> Continuation;
						;
						return Continuation;
					}
				)
			;
		}
	}
}

