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
				CTimerHandleCleanup(NFunction::TCFunction<void ()> const &_fCleanup);
				~CTimerHandleCleanup();

				NFunction::TCFunction<void ()> m_fCleanup;
			};

			struct CTimer
			{
				CTimer(CActor *_pThisActor);
				~CTimer();

				zfp64 m_NextElapse;
				zfp64 m_Period;
				ETimerType m_TimerType;

				DMibListLinkDS_Link(CTimer, m_Link);

				TCActorCallbackManager<void (), true, CTimerHandleCleanup> m_Callbacks;

				NPtr::TCSharedPointer<bool> m_pDestroyed;

				bool operator < (CTimer const &_Right) const
				{
					return m_NextElapse < _Right.m_NextElapse;
				}
			};

			NContainer::TCMap<fp64, CTimer> m_RegisteredTimers;
			NContainer::TCLinkedList<CTimer> m_OneshotTimers;
			NContainer::TCLinkedList<CTimer> m_ExactTimers;
			NAtomic::TCAtomic<smint> m_WaitTime;

			DMibListLinkDS_List(CTimer, m_Link) m_TimerQueue;

			NTime::CClock m_Clock;
			
#if DMibEnableSafeCheck > 0
			mint m_ProcessingThread;
#endif

			void fp_ProcessTimers();
			void fp_StartThread();

			TCContinuation<void> f_Destroy();

		public:

			CTimerActor();
			~CTimerActor();

			void f_OneshotTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void ()> && _fCallback);
			CActorCallback f_OneshotTimerAbortable(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void ()> && _fCallback);
			CActorCallback f_RegisterTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void ()> && _fCallback);
			CActorCallback f_RegisterExactTimer(fp64 _Period, TCActor<CActor> const &_pActor, NFunction::TCFunction<void ()> && _fCallback);
		};
	}
}

