// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>

namespace NMib
{
	namespace NConcurrency
	{
		class CTimerActor : public CActor
		{
			friend class CConcurrencyManager;
			template <typename t_CActor>
			friend class TCActorInternal;

			NPtr::TCUniquePointer<NThread::CThreadObject> m_pTimerThread;

			enum ETimerType
			{
				ETimerType_Oneshot
				, ETimerType_Normal
				, ETimerType_Exact
			};

			struct CTimerHandleCleanup
			{
				CTimerHandleCleanup(NFunction::TCFunctionMutable<void ()> &&_fCleanup);
				~CTimerHandleCleanup();

				NFunction::TCFunctionMutable<void ()> m_fCleanup;
			};

			struct CTimer
			{
				CTimer(CActor *_pThisActor);
				~CTimer();

				zfp64 m_NextElapse;
				zfp64 m_Period;
				ETimerType m_TimerType;

				DMibListLinkDS_Link(CTimer, m_Link);

				TCActorSubscriptionManager<void (), true, CTimerHandleCleanup> m_Callbacks;

				NPtr::TCSharedPointer<bool> m_pDestroyed;

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

			void fp_ProcessTimers();
			void fp_StartThread();

			TCContinuation<void> fp_Destroy();

		public:

			CTimerActor();
			~CTimerActor();
			
			void f_FireAllTimeouts();
			void f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
			CActorSubscription f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
			CActorSubscription f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
			CActorSubscription f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
		};

		TCDispatchedActorCall<void> fg_Timeout(fp64 _Period);
		void fg_OneshotTimer(fp64 _Period, NFunction::TCFunctionMutable<void ()> &&_fCallback, TCActor<CActor> const &_pActor = nullptr);
		TCDispatchedActorCall<CActorSubscription> fg_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
		TCDispatchedActorCall<CActorSubscription> fg_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
		TCDispatchedActorCall<CActorSubscription> fg_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunctionMutable<void ()> &&_fCallback);
	}
}

#include "Malterlib_Concurrency_TimerActor.hpp"
