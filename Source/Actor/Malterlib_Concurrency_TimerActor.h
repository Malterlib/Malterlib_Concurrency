// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>

namespace NMib::NConcurrency
{
	class CTimerActor : public CActor
	{
		friend class CConcurrencyManager;
		template <typename t_CActor>
		friend class TCActorInternal;

		NStorage::TCUniquePointer<NThread::CThreadObject> m_pTimerThread;

		enum ETimerType
		{
			ETimerType_Oneshot
			, ETimerType_Normal
			, ETimerType_Exact
		};

		struct CTimerSubscriptionState
		{
			CTimerSubscriptionState(NFunction::TCFunctionMutable<void ()> &&_fCleanup);
			~CTimerSubscriptionState();

			NFunction::TCFunctionMutable<void ()> m_fCleanup;
			NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
			bool m_bOutstanding = false;
			bool m_bMissed = false;
		};

		struct CTimer
		{
			CTimer(CActor *_pThisActor);
			~CTimer();

			DMibListLinkDS_Link(CTimer, m_Link);
			TCActorSubscriptionManager<TCContinuation<void> (), true, CTimerSubscriptionState> m_Callbacks;
			TCActorSubscriptionManager<void (), true, CTimerSubscriptionState> m_OneshotCallbacks;
			NStorage::TCSharedPointer<bool> m_pDestroyed;
			zfp64 m_NextElapse;
			zfp64 m_Period;
			ETimerType m_TimerType;
			bool m_bFireAtExit = false;

			bool operator < (CTimer const &_Right) const
			{
				return m_NextElapse < _Right.m_NextElapse;
			}
		};

		NContainer::TCMap<fp64, CTimer> m_RegisteredTimers;
		NContainer::TCLinkedList<CTimer> m_OneshotTimers;
		NContainer::TCLinkedList<CTimer> m_ExactTimers;
		align_cacheline NAtomic::TCAtomic<smint> m_WaitTime;

		DMibListLinkDS_List(CTimer, m_Link) m_TimerQueue;

		NTime::CClock m_Clock;

#if DMibEnableSafeCheck > 0
		mint m_ProcessingThread;
#endif

		void fp_CallTimer(CTimer &_Timer, bool _bOnlyMissed = false);
		void fp_ProcessTimers();
		void fp_StartThread();

		TCContinuation<void> fp_Destroy();

	public:

		CTimerActor();
		~CTimerActor();

		void f_FireAllTimeouts();
		void f_FireAtExit();
		void f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback, bool _bFireAtExit);
		CActorSubscription f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
		CActorSubscription f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCContinuation<void> ()> &&_fCallback);
		CActorSubscription f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCContinuation<void> ()> &&_fCallback);
	};

	struct CTimeoutHelper
	{
		CTimeoutHelper(fp64 _Period, bool _bFireAtExit);
		CTimeoutHelper &operator ()(TCActor<CActor> const &_DispatchActor);

		void operator > (NFunction::TCFunctionMutable<void ()> &&_fOnTimeout) const;

	private:
		fp64 mp_Period;
		TCActor<CActor> mp_DispatchActor;
		bool mp_bFireAtExit = false;
	};

	CTimeoutHelper fg_Timeout(fp64 _Period, bool _bFireAtExit = true);
	void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMutable<void ()> &&_fCallback, TCActor<CActor> const &_pActor = nullptr, bool _bFireAtExit = true);
	TCDispatchedActorCall<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
	TCDispatchedActorCall<CActorSubscription> fg_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCContinuation<void> ()> &&_fCallback);
	TCDispatchedActorCall<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<TCContinuation<void> ()> &&_fCallback);
}

#include "Malterlib_Concurrency_TimerActor.hpp"
