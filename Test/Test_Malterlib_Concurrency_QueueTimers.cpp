// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;

	TCFuture<umint> fg_TimerWorkerThread()
	{
		co_return NSys::fg_Thread_GetCurrentUID();
	}

	struct CQueueTimerTarget : CActor
	{
		enum class EKind : uint8
		{
			mc_Oneshot
			, mc_OneshotUnabortable
			, mc_Periodic
			, mc_Exact
		};

		struct CWork
		{
			NAtomic::TCAtomic<bool> m_bExpired = false;
			NTime::CStopwatch m_Elapsed{true};
			TCPromise<bool> m_Done;
		};

		struct CCallbackState
		{
			TCPromise<void> m_Fired;
			NAtomic::TCAtomic<umint> m_nCalls = 0;
		};

		TCFuture<CActorSubscription> f_Subscribe(EKind _Kind, TCSharedPointer<CCallbackState> _pState)
		{
			auto fCallback = [_pState]() -> TCFuture<void>
				{
					_pState->m_nCalls.f_FetchAdd(1);
					if (!_pState->m_Fired.f_IsSet())
						_pState->m_Fired.f_SetResult();
					co_return {};
				}
			;

			if (_Kind == EKind::mc_Oneshot)
				co_return co_await fg_OneshotTimerAbortable(0.01, fg_Move(fCallback));
			if (_Kind == EKind::mc_Periodic)
				co_return co_await fg_RegisterTimer(0.01, fg_Move(fCallback));
			co_return co_await fg_RegisterExactTimer(0.01, fg_Move(fCallback));
		}

		umint f_Thread()
		{
			return NSys::fg_Thread_GetCurrentUID();
		}

		TCActor<CTimerActor> f_TimerOwner()
		{
			return fg_TimerActor();
		}

		TCFuture<bool> f_DelayedRegistration(EKind _Kind)
		{
			auto pFired = TCSharedPointer<NAtomic::TCAtomic<bool>>(fg_Construct(false));
			auto fCallback = [pFired]() -> TCFuture<void>
				{
					pFired->f_Store(true);
					co_return {};
				}
			;

			TCFuture<CActorSubscription> Registration;
			switch (_Kind)
			{
			case EKind::mc_Oneshot:
				Registration = fg_OneshotTimerAbortable(0.01, fg_Move(fCallback), fg_DirectCallActor());
				break;
			case EKind::mc_OneshotUnabortable:
				fg_OneshotTimer(0.01, fg_Move(fCallback), fg_DirectCallActor(), false);
				break;
			case EKind::mc_Periodic:
				Registration = fg_RegisterTimer(0.01, fg_Move(fCallback), fg_DirectCallActor());
				break;
			case EKind::mc_Exact:
				Registration = fg_RegisterExactTimer(0.01, fg_Move(fCallback), fg_DirectCallActor());
				break;
			}

			// Hold the owner queue past the deadline before it can handle registration.
			NSys::fg_Thread_Sleep(0.03f);

			CActorSubscription Subscription;
			if (_Kind == EKind::mc_OneshotUnabortable)
				co_await fg_Dispatch(fg_TimerActor(), [] {});
			else
				Subscription = co_await fg_Move(Registration);

			auto bFired = pFired->f_Load();
			if (Subscription)
				co_await Subscription->f_Destroy();

			co_return bFired;
		}

		TCFuture<umint> f_DrainBeforePendingTimerWork()
		{
			auto pCount = TCSharedPointer<NAtomic::TCAtomic<umint>>(fg_Construct(umint(0)));
			for (umint i = 0; i < 256; ++i)
				fg_Dispatch(fg_ThisActor(this), [pCount] { pCount->f_FetchAdd(1); }).f_DiscardResult();

			TCPromise<umint> Done;
			fg_Dispatch(fg_TimerActor(), [pCount, Done] { Done.f_SetResult(pCount->f_Load()); }).f_DiscardResult();

			co_return co_await Done.f_Future();
		}

		TCFuture<bool> f_TimerRunsBeforeQueuedWork()
		{
			auto pRan = TCSharedPointer<NAtomic::TCAtomic<bool>>(fg_Construct(false));
			TCPromise<bool> Done;
			f_ConcurrencyManager().f_DispatchOnCurrentThreadOrConcurrent
				(
					EPriority_Normal
					, [pRan, Done](CConcurrencyThreadLocal &)
					{
						Done.f_SetResult(pRan->f_Load());
					}
				)
			;
			fg_Dispatch(fg_TimerActor(), [pRan] { pRan->f_Store(true); }).f_DiscardResult();

			co_return co_await Done.f_Future();
		}

		TCFuture<bool> f_ExpireDuringContinuousWork()
		{
			auto pWork = TCSharedPointer<CWork>(fg_Construct());
			auto Subscription = co_await fg_OneshotTimerAbortable
				(
					0.01
					, [pWork]() -> TCFuture<void>
					{
						pWork->m_bExpired.f_Store(true);
						co_return {};
					}
					, fg_DirectCallActor()
				)
			;
			f_Work(pWork);
			co_return co_await pWork->m_Done.f_Future();
		}

		void f_Work(TCSharedPointer<CWork> const &_pWork)
		{
			if (_pWork->m_bExpired.f_Load() || _pWork->m_Elapsed.f_GetTime() > 2.0)
			{
				_pWork->m_Done.f_SetResult(_pWork->m_bExpired.f_Load());
				return;
			}

			fg_ThisActor(this).f_Bind<&CQueueTimerTarget::f_Work>(_pWork).f_DiscardResult();
		}
	};

	struct CLowQueueTimerTarget : CQueueTimerTarget
	{
		static constexpr EPriority mc_Priority = EPriority_Low;
	};

	struct CQueueTimers_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("LateSubscriptionRelease")
			{
				CActorSubscription Subscription;
				NAtomic::TCAtomic<bool> bFired = false;
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities);
					Subscription = Manager.f_GetTimerActor().f_Bind<&CTimerActor::f_OneshotTimerAbortable>
						(
							fp64(1000000.0)
							, NTime::CSystem_Time::fs_GetTimerValue()
							, Manager.f_GetDirectCallActor()
							, [&bFired]() -> TCFuture<void>
							{
								bFired.f_Store(true);
								co_return {};
							}
						).f_CallSync()
					;
					Manager.f_BlockOnDestroy();
				}

				Subscription.f_Clear();
				DMibExpectFalse(bFired.f_Load());
			};

			DMibTestSuite("ConcurrentActorShutdown")
			{
				NAtomic::TCAtomic<bool> bCleaned = false;
				TCPromise<void> Started;
				EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
				CConcurrencyManager Manager(Priorities);
				Manager.f_EnableShutdownLogging(true);
				fg_Dispatch
					(
						Manager.f_GetConcurrentActor()
						, [Started, pCleaned = &bCleaned]() -> TCFuture<void>
						{
							auto Cleanup = co_await fg_AsyncDestroy
								(
									[pCleaned]() -> TCFuture<void>
									{
										auto *pResult = pCleaned;
										co_await fg_Timeout(0.01).f_Dispatch();
										pResult->f_Store(true);
										co_return {};
									}
								)
							;
							Started.f_SetResult();
							co_return {};
						}
					).f_DiscardResult()
				;
				Started.f_Future().f_CallSync();
				Manager.f_BlockOnDestroy();
				DMibExpectTrue(bCleaned.f_Load());
			};

			DMibTestSuite("BlockingActorShutdown")
			{
				EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
				NAtomic::TCAtomic<bool> bFired = false;
				CConcurrencyManager Manager(Priorities);
				Manager.f_GetConcurrentActor();
				auto Checkout = Manager.f_GetBlockingActor();
				DMibExpectTrue(&Checkout.f_Actor()->f_ConcurrencyManager() == &Manager);
				auto Timer = Manager.f_GetTimerActor();
				Timer.f_Bind<&CTimerActor::f_OneshotTimer>
					(
						fp64(0.1)
						, NTime::CSystem_Time::fs_GetTimerValue()
						, Manager.f_GetDirectCallActor()
						, [Checkout = fg_Move(Checkout), &bFired]() mutable -> TCFuture<void>
						{
							bFired.f_Store(true);
							Checkout.f_Clear();
							co_return {};
						}
						, false
					).f_CallSync()
				;

				Manager.f_BlockOnDestroy();
				DMibExpectTrue(bFired.f_Load());
			};

			DMibTestSuite("PlacementAndProgress") -> TCFuture<void>
			{
				auto Actor = fg_ConstructActor<CQueueTimerTarget>();
				auto DestroyActor = co_await fg_AsyncDestroy(Actor);
				Actor->f_SetFixedQueue(0);
				auto Timer = fg_TimerActor(Actor);
				DMibExpectTrue(Timer->f_IsAlwaysAlive());

				auto ActorThread = co_await Actor.f_Bind<&CQueueTimerTarget::f_Thread>();
				auto TimerThread = co_await Timer.f_Bind<fg_TimerWorkerThread>();
				DMibExpect(ActorThread, ==, TimerThread);
				auto FromActor = co_await Actor.f_Bind<&CQueueTimerTarget::f_TimerOwner>();
				DMibExpectTrue(FromActor.f_Get() == Timer.f_Get());

#if !DMibConfig_Concurrency_FairScheduling
				DMibExpect(co_await Actor.f_Bind<&CQueueTimerTarget::f_DrainBeforePendingTimerWork>(), ==, umint(256));
#endif
				DMibExpectTrue(co_await Actor.f_Bind<&CQueueTimerTarget::f_TimerRunsBeforeQueuedWork>());
				DMibExpectTrue(co_await Actor.f_Bind<&CQueueTimerTarget::f_ExpireDuringContinuousWork>());

				using EKind = CQueueTimerTarget::EKind;
				for (auto Kind : {EKind::mc_Oneshot, EKind::mc_Periodic, EKind::mc_Exact})
				{
					DMibTestPath(Kind == EKind::mc_Oneshot ? "Oneshot" : Kind == EKind::mc_Periodic ? "Periodic" : "Exact");
					auto pState = TCSharedPointer<CQueueTimerTarget::CCallbackState>(fg_Construct());
					auto Subscription = co_await Actor.f_Bind<&CQueueTimerTarget::f_Subscribe>(Kind, pState);
					co_await pState->m_Fired.f_Future();
					co_await Subscription->f_Destroy();
					auto nCalls = pState->m_nCalls.f_Load();
					DMibExpect(nCalls, >=, umint(1));

					co_await fg_Timeout(0.03);
					DMibExpect(pState->m_nCalls.f_Load(), ==, nCalls);
					DMibExpectTrue(fg_TimerActor(Actor).f_Get() == Timer.f_Get());
				}
				co_return {};
			};

			DMibTestSuite("DelayedRegistration") -> TCFuture<void>
			{
				auto Actor = fg_ConstructActor<CQueueTimerTarget>();
				auto DestroyActor = co_await fg_AsyncDestroy(Actor);
				Actor->f_SetFixedQueue(0);

				using EKind = CQueueTimerTarget::EKind;
				for (auto Kind : {EKind::mc_Oneshot, EKind::mc_OneshotUnabortable, EKind::mc_Periodic, EKind::mc_Exact})
				{
					DMibTestPath("Kind {}"_f << umint(Kind));
					DMibExpectTrue(co_await Actor.f_Bind<&CQueueTimerTarget::f_DelayedRegistration>(Kind));
				}

				co_return {};
			};

			DMibTestSuite("ConcurrentFirstSelection") -> TCFuture<void>
			{
				auto Actor = fg_ConstructActor<CQueueTimerTarget>();
				auto DestroyActor = co_await fg_AsyncDestroy(Actor);
				TCFutureVector<TCActor<CTimerActor>> Results;
				for (umint i = 0; i < 64; ++i)
				{
					fg_Dispatch(fg_ConcurrentActor(), [Actor] { return fg_TimerActor(Actor); }) > Results;
				}

				auto Owners = co_await fg_AllDone(Results);
				auto Timer = fg_TimerActor(Actor);
				for (umint i = 0; i < Owners.f_GetLen(); ++i)
				{
					DMibTestPath("Registration {}"_f << i);
					DMibExpectTrue(Owners[i].f_Get() == Timer.f_Get());
				}

				auto FromActor = co_await Actor.f_Bind<&CQueueTimerTarget::f_TimerOwner>();
				DMibExpectTrue(FromActor.f_Get() == Timer.f_Get());

				auto LowActor = fg_ConstructActor<CLowQueueTimerTarget>();
				auto DestroyLowActor = co_await fg_AsyncDestroy(LowActor);
				LowActor->f_SetFixedQueue(0);
				auto *pExpected = fg_TimerActor(LowActor).f_Get();
				auto &Manager = LowActor->f_ConcurrencyManager();
				TCPromise<bool> Matches;
				Manager.f_DispatchToQueue
					(
						EPriority_Low, 0
						, [Matches, pExpected, pManager = &Manager](CConcurrencyThreadLocal &_ThreadLocal)
						{
							Matches.f_SetResult
								(
									!_ThreadLocal.m_pCurrentlyProcessingActorHolder
									&& fg_TimerActor().f_Get() == pExpected
									&& pManager->f_GetTimerActor().f_Get() == pExpected
								)
							;
						}
					)
				;
				DMibExpectTrue(co_await Matches.f_Future());
				co_return {};
			};
		}
	};

	DMibTestRegister(CQueueTimers_Tests, Malterlib::Concurrency);
}
