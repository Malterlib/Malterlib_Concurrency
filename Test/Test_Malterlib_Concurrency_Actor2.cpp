// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/Actor/Lock>

namespace NTestActor2
{
	const static fp64 g_SecondsToWait = 1.0/1000.0;
	using namespace NMib::NConcurrency;
	using namespace NMib;
	using namespace NMib::NTime;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;

	fp64 g_Timeout = 60.0 * NMib::NTest::gc_TimeoutMultiplier;

	constinit static NAtomic::TCAtomic<umint> g_nDestroyedActors{0};

	class CTestActor : public CActor
	{
	public:
		void f_TestValue()
		{

		}
		TCFuture<void> f_TestValueFuture()
		{
			co_return {};
		}
		int f_TestValue1(int _Value)
		{
			return _Value;
		}
		TCFuture<int> f_TestValue1Future(int _Value)
		{
			co_return _Value;
		}
		int f_TestValue2(int _Value0, int _Value1)
		{
			return _Value0 + _Value1;
		}
		int f_TestValue3(int _Value0, int _Value1, int _Value2)
		{
			return _Value0 + _Value1 + _Value2;
		}
		int f_TestValue4(int _Value0, int _Value1, int _Value2, int _Value3)
		{
			return _Value0 + _Value1 + _Value2 + _Value3;
		}
		int f_TestValue5(int _Value0, int _Value1, int _Value2, int _Value3, int _Value4)
		{
			return _Value0 + _Value1 + _Value2 + _Value3 + _Value4;
		}
		int f_TestValue6(int _Value0, int _Value1, int _Value2, int _Value3, int _Value4, int _Value5)
		{
			return _Value0 + _Value1 + _Value2 + _Value3 + _Value4 + _Value5;
		}
		int f_TestValue7(int _Value0, int _Value1, int _Value2, int _Value3, int _Value4, int _Value5, int _Value6)
		{
			return _Value0 + _Value1 + _Value2 + _Value3 + _Value4 + _Value5 + _Value6;
		}
		int f_TestValue8(int _Value0, int _Value1, int _Value2, int _Value3, int _Value4, int _Value5, int _Value6, int _Value7)
		{
			return _Value0 + _Value1 + _Value2 + _Value3 + _Value4 + _Value5 + _Value6 + _Value7;
		}

		TCFuture<void> f_TestPromise()
		{
			TCPromise<void> Promise;

			f_TestValueFuture()
				> [Promise](NMib::NConcurrency::TCAsyncResult<void> &&_Result)
				{
					Promise.f_SetResult(fg_Move(_Result));
				}
			;

			co_return co_await Promise.f_MoveFuture();
		}
		TCFuture<int> f_TestPromiseRet()
		{
			TCPromise<int> Promise;

			f_TestValue1Future(4)
				> [Promise](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
				{
					Promise.f_SetResult(fg_Move(_Result));
				}
			;


			co_return co_await Promise.f_MoveFuture();
		}
	};

	class CLocalActor : public CActor
	{
	public:
		using CActorHolder = CDispatchingActorHolder;
	};

	struct CTestActorWithDestroy : public CActor
	{
		~CTestActorWithDestroy()
		{
			++g_nDestroyedActors;
		}

		TCFuture<void> f_Test()
		{
			co_return {};
		}

		TCFuture<void> fp_Destroy() override
		{
			co_return {};
		}
	};

	struct CTestActorWithDestroySeparateThread : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		~CTestActorWithDestroySeparateThread()
		{
			++g_nDestroyedActors;
		}
	};

	class CConcurrency_Tests : public NMib::NTest::CTest
	{
	public:

		void fp_SyncOnAllThreads()
		{
			TCFutureVector<void> Results;

			umint nThreads = NSys::fg_Thread_GetVirtualCores();
			for (umint i = 0; i < nThreads; ++i)
			{
				fg_ConcurrentDispatch
					(
						[]
						{
						}
					)
					> Results
				;
			}

			fg_AllDoneWrapped(Results).f_CallSync(g_Timeout);
		}

		void f_TestAsyncResult()
		{
			DMibTestSuite("Async")
			{
				TCAsyncResult<int> TestValue;
				TestValue.f_SetResult(6);

				DMibTest(DMibExpr(TestValue.f_Get()) == DMibExpr(6));

				auto Exception = DMibImpExceptionInstance(NMib::NException::CException, "Test");
				TCAsyncResult<int> TestException;
				TestException.f_SetException(Exception);

				DMibTest(!DMibExpr(TestException));
				DMibExpectException(TestException.f_Get(), Exception);
			};
		}

		void f_DestroyTests()
		{
			DMibTestSuite("Destroy")
			{
				using namespace NMib::NThread;
				using namespace NMib::NConcurrency;
				auto pActor = fg_ConstructActor<CTestActor>();

				CEvent Event;

				fg_Move(pActor).f_Destroy() > fg_ThisConcurrentActor() / [&](TCAsyncResult<void> &&)
					{
						Event.f_SetSignaled();
					}
				;

				bool bTimedOut = Event.f_WaitTimeout(20.0);
				DMibExpectFalse(bTimedOut);
			};
			DMibTestSuite("Destroy Separate Thread")
			{
				using namespace NMib::NThread;
				using namespace NMib::NConcurrency;
				using namespace NMib::NStorage;

				TCActor<CSeparateThreadActor> Actor(fg_Construct(), "Test Destroy");

				struct CState
				{
					CEvent m_Event1;
					CEvent m_Event2;
				};
				TCSharedPointer<CState> pState = fg_Construct();
				auto Cleanup = g_OnScopeExit / [pState]
					{
						pState->m_Event2.f_WaitTimeout(20.0);
					}
				;

				Actor->f_QueueProcess
					(
						[
							pState
							, Cleanup = fg_Move(Cleanup)
						]
						(CConcurrencyThreadLocal &_ThreadLocal)
						{
							pState->m_Event1.f_SetSignaled();
						}
					)
				;

				bool bTimedOut = pState->m_Event1.f_WaitTimeout(20.0);
				DMibExpectFalse(bTimedOut);
				Actor.f_Clear();
				NSys::fg_Thread_Sleep(1.0);
				pState->m_Event2.f_SetSignaled();
				NSys::fg_Thread_Sleep(1.0);
			};
			DMibTestSuite("Use after free in destruction")
			{
				using namespace NMib::NThread;
				using namespace NMib::NConcurrency;
				using namespace NMib::NStorage;

				umint nDestroyedStart = g_nDestroyedActors.f_Load();

				TCFutureVector<void> Results;

				umint nDestructions = 10;

				for (umint i = 0; i < nDestructions; ++i)
				{
					TCActor<CTestActorWithDestroy> Actor(fg_Construct());
					g_ConcurrentDispatch / [WeakActor = Actor.f_Weak()]() mutable -> TCFuture<void>
						{
							NSys::fg_Thread_Sleep(0.05f);
							auto ActorLocked = WeakActor.f_Lock();
							if (ActorLocked)
							{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
								fg_ConcurrencyThreadLocal().m_bForceWakeUp = true;
#endif
								ActorLocked.f_Bind<&CTestActorWithDestroy::f_Test>().f_DiscardResult();
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
								fg_ConcurrencyThreadLocal().m_bForceWakeUp = false;
#endif
							}
							co_return {};
						}
						> Results
					;

#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
					fg_ConcurrencyThreadLocal().m_nWaits = 1;
#endif
					Actor.f_Clear();
				}


				fg_AllDoneWrapped(Results).f_CallSync(g_Timeout);

				fp_SyncOnAllThreads();

				CStopwatch Stopwatch{true};
				while (g_nDestroyedActors.f_Load() - nDestroyedStart < nDestructions)
				{
					if (Stopwatch.f_GetTime() >= g_Timeout)
						break;
					NSys::fg_Thread_Sleep(0.001f);
				}
				umint nDestroyed = g_nDestroyedActors.f_Load() - nDestroyedStart;

				DMibExpect(nDestroyed, ==, nDestructions);
			};
			DMibTestSuite("Use after free in destruction separate thread actor holder")
			{
				using namespace NMib::NThread;
				using namespace NMib::NConcurrency;
				using namespace NMib::NStorage;

				umint nDestroyedStart = g_nDestroyedActors.f_Load();

				TCFutureVector<void> Results;

				umint nDestructions = 10;

				for (umint i = 0; i < nDestructions; ++i)
				{
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
					auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
					ConcurrencyThreadLocal.m_bForceBusyWait = true;
#endif
					TCActor<CTestActorWithDestroySeparateThread> Actor(fg_Construct(), "Use after free in destruction separate thread actor holder");

					(g_Dispatch(Actor) / []{}).f_CallSync(g_Timeout);

#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
					ConcurrencyThreadLocal.m_bForceBusyWait = false;
					ConcurrencyThreadLocal.m_bForceWakeUp = true;
					ConcurrencyThreadLocal.m_nWaits = 1;
#endif
					Actor.f_Clear();
#if DMibConfig_Tests_Enable && !defined(DTests_PerfTests)
					ConcurrencyThreadLocal.m_bForceWakeUp = false;
#endif
				}

				fg_AllDoneWrapped(Results).f_CallSync(g_Timeout);

				fp_SyncOnAllThreads();

				CStopwatch Stopwatch{true};
				while (g_nDestroyedActors.f_Load() - nDestroyedStart < nDestructions)
				{
					if (Stopwatch.f_GetTime() >= g_Timeout)
						break;
					NSys::fg_Thread_Sleep(0.001f);
				}
				umint nDestroyed = g_nDestroyedActors.f_Load() - nDestroyedStart;

				DMibExpect(nDestroyed, ==, nDestructions);
			};
		}

		void f_GeneralTests()
		{
			DMibTestSuite("General")
			{
				EExecutionPriority ExecutionPriority[EPriority_Max]
#ifdef DPlatformFamily_macOS
					= {EExecutionPriority_BelowNormal, EExecutionPriority_Normal, EExecutionPriority_Normal}
#else
					= {EExecutionPriority_Lowest, EExecutionPriority_Normal, EExecutionPriority_Normal}
#endif
				;

				CConcurrencyManager ConcurrencyManager(ExecutionPriority);
				auto Cleanup = g_OnScopeExit / [&]
					{
						ConcurrencyManager.f_BlockOnDestroy();
					}
				;

				using namespace NMib::NThread;
				using namespace NMib::NConcurrency;

				int Object = 0;
				NThread::CMutual Lock;
				NMib::NAtomic::TCAtomic<smint> TestLockValue;
				NMib::NAtomic::TCAtomic<smint> nTestLockThread;
				NMib::NAtomic::TCAtomic<smint> nTestLockActor;

				NThread::CMutual TimersLock;

				auto pTestLockThread
					= CThreadObject::fs_StartThread
					(
						[&](CThreadObject *_pThread)
						{
							while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
							{
								{
									DMibLock(Lock);
									++nTestLockThread;
									for (int i = 0; i < 1024; ++i)
									{
										if (TestLockValue.f_Exchange(1) != 0)
											DMibPDebugBreak;
										TestLockValue.f_Exchange(0);
									}
								}
							}
							return 0;
						}
						, "Test lock thread"
					)
				;

				{
					auto pLocalActor = fg_DirectCallActor();
					auto pActor = fg_ConstructActor<CTestActor>();
					auto pLockActor = fg_ConstructActor<TCLockActor<int, NThread::CMutual>>(fg_Construct("MalterlibTestSeparateActorThread"), Object, Lock);

					CActorSubscription OneshotAbortableTimer;
					CActorSubscription ExactTimer;
					CActorSubscription NormalTimer;
					NAtomic::TCAtomic<umint> nExpectedHandlers(1);
					NAtomic::TCAtomic<bool> bHandledTimer0;
					NAtomic::TCAtomic<bool> bHandledTimer1;
					NAtomic::TCAtomic<bool> bHandledTimer2;
					NMib::NThread::CEvent HandlersFinished;
					HandlersFinished.f_ResetSignaled();
					TCFutureVector<int> VectorResults;

					for (int i = 0; i < 128*1024; ++i)
					{
						++nExpectedHandlers;
						pLockActor.f_Bind<&TCLockActor<int, NThread::CMutual>::f_Lock>()
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<TCLockActor<int, NThread::CMutual>::CLockReference> &&_Result)
							{
								auto &LockResult = *_Result;
								++(*LockResult);

								++nTestLockActor;

								for (int i = 0; i < 1024; ++i)
								{
									if (TestLockValue.f_Exchange(1) != 0)
										DMibPDebugBreak;
									TestLockValue.f_Exchange(0);
								}

								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;
					}

					++nExpectedHandlers;
					ConcurrencyManager.f_GetTimerActor().f_Bind<&CTimerActor::f_OneshotTimer>
						(
							0.2
							, pLocalActor
							, [&]() -> TCFuture<void>
							{
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();

								co_return {};
							}
							, true
						)
						.f_DiscardResult()
					;


					{
						++nExpectedHandlers;
						ConcurrencyManager.f_GetTimerActor()
							(
								&CTimerActor::f_OneshotTimerAbortable
								, 1000000.0
								, pLocalActor
								, [&]() -> TCFuture<void>
								{
									if (--nExpectedHandlers == 0)
										HandlersFinished.f_SetSignaled();

									co_return {};
								}
							)
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<CActorSubscription> &&_Result)
							{
								DMibLock(TimersLock);
								if (OneshotAbortableTimer.f_IsEmpty())
									OneshotAbortableTimer = fg_Move(*_Result);
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;

						nExpectedHandlers += 2;
						ConcurrencyManager.f_GetTimerActor()
							(
								&CTimerActor::f_OneshotTimerAbortable
								, 0.1
								, pLocalActor
								, [&]() -> TCFuture<void>
								{
									if (!bHandledTimer0.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}

									co_return {};
								}
							)
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<CActorSubscription> &&_Result)
							{
								DMibLock(TimersLock);
								OneshotAbortableTimer = fg_Move(*_Result);
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;

						++nExpectedHandlers;
						ConcurrencyManager.f_GetTimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 1000000.0
								, pLocalActor
								, [&]() -> TCFuture<void>
								{
									DMibNeverGetHere;

									co_return {};
								}
							)
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<CActorSubscription> &&_Result)
							{
								DMibLock(TimersLock);
								if (!NormalTimer.f_IsEmpty())
									NormalTimer = fg_Move(*_Result);
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;

						nExpectedHandlers += 2;
						ConcurrencyManager.f_GetTimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 0.5
								, pLocalActor
								, [&]() -> TCFuture<void>
								{
									if (!bHandledTimer1.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}

									co_return {};
								}
							)
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<CActorSubscription> &&_Result)
							{
								DMibLock(TimersLock);
								NormalTimer = fg_Move(*_Result);
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;

						nExpectedHandlers += 2;
						ConcurrencyManager.f_GetTimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 0.5
								, pLocalActor
								, [&]() -> TCFuture<void>
								{
									if (!bHandledTimer2.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}

									co_return {};
								}
							)
							> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<CActorSubscription> &&_Result)
							{
								DMibLock(TimersLock);
								ExactTimer = fg_Move(*_Result);
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
						;
					}

					++nExpectedHandlers;
					pLockActor(&TCLockActor<int, NThread::CMutual>::f_Lock)
						> pLocalActor / [&](NMib::NConcurrency::TCAsyncResult<TCLockActor<int, NThread::CMutual>::CLockReference> &&_Result)
						{
							auto &Lock = *_Result;
							++(*Lock);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue)
						> pLocalActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<void> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestPromise)
						> pLocalActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<void> &&_Result)
						{
							_Result.f_Get();
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestPromiseRet)
						> pLocalActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result0, NMib::NConcurrency::TCAsyncResult<int> &&_Result1)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						> pActor / [&, pActor]
						(
							NMib::NConcurrency::TCAsyncResult<int> &&_Result0
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
						)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						+ pActor(&CTestActor::f_TestValue4, 1, 2, 3, 4)
						> pActor / [&, pActor]
						(
							NMib::NConcurrency::TCAsyncResult<int> &&_Result0
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result3
						)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							DMibCheck(_Result3);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						+ pActor(&CTestActor::f_TestValue4, 1, 2, 3, 4)
						+ pActor(&CTestActor::f_TestValue5, 1, 2, 3, 4, 5)
						> pActor / [&, pActor]
						(
							NMib::NConcurrency::TCAsyncResult<int> &&_Result0
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result3
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result4
						)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							DMibCheck(_Result3);
							DMibCheck(_Result4);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						+ pActor(&CTestActor::f_TestValue4, 1, 2, 3, 4)
						+ pActor(&CTestActor::f_TestValue5, 1, 2, 3, 4, 5)
						+ pActor(&CTestActor::f_TestValue6, 1, 2, 3, 4, 5, 6)
						> pActor / [&, pActor]
						(
							NMib::NConcurrency::TCAsyncResult<int> &&_Result0
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result3
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result4
							, NMib::NConcurrency::TCAsyncResult<int> &&_Result5
						)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							DMibCheck(_Result3);
							DMibCheck(_Result4);
							DMibCheck(_Result5);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						+ pActor(&CTestActor::f_TestValue4, 1, 2, 3, 4)
						+ pActor(&CTestActor::f_TestValue5, 1, 2, 3, 4, 5)
						+ pActor(&CTestActor::f_TestValue6, 1, 2, 3, 4, 5, 6)
						+ pActor(&CTestActor::f_TestValue7, 1, 2, 3, 4, 5, 6, 7)
						> pActor / [&, pActor]
							(
								NMib::NConcurrency::TCAsyncResult<int> &&_Result0
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result3
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result4
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result5
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result6
							)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							DMibCheck(_Result3);
							DMibCheck(_Result4);
							DMibCheck(_Result5);
							DMibCheck(_Result6);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue1, 1)
						+ pActor(&CTestActor::f_TestValue2, 1, 2)
						+ pActor(&CTestActor::f_TestValue3, 1, 2, 3)
						+ pActor(&CTestActor::f_TestValue4, 1, 2, 3, 4)
						+ pActor(&CTestActor::f_TestValue5, 1, 2, 3, 4, 5)
						+ pActor(&CTestActor::f_TestValue6, 1, 2, 3, 4, 5, 6)
						+ pActor(&CTestActor::f_TestValue7, 1, 2, 3, 4, 5, 6, 7)
						+ pActor(&CTestActor::f_TestValue8, 1, 2, 3, 4, 5, 6, 7, 8)
						> pActor / [&, pActor]
							(
								NMib::NConcurrency::TCAsyncResult<int> &&_Result0
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result1
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result2
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result3
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result4
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result5
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result6
								, NMib::NConcurrency::TCAsyncResult<int> &&_Result7
							)
						{
							DMibCheck(_Result0);
							DMibCheck(_Result1);
							DMibCheck(_Result2);
							DMibCheck(_Result3);
							DMibCheck(_Result4);
							DMibCheck(_Result5);
							DMibCheck(_Result6);
							DMibCheck(_Result7);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;

					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue2, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue3, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue4, 1, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue5, 1, 1, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue6, 1, 1, 1, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue7, 1, 1, 1, 1, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestValue8, 1, 1, 1, 1, 1, 1, 1, 1)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
						{
							DMibCheck(_Result);
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;

					for (int i = 0; i < 512; ++i)
					{
						pActor(&CTestActor::f_TestValue8, 1, 1, 1, 1, 1, 1, 1, 1) > VectorResults;
					}

					++nExpectedHandlers;

					fg_AllDoneWrapped(VectorResults)
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<int>>> &&_Result)
						{
							[[maybe_unused]] umint Results = 0;
							for (auto Iter = (*_Result).f_GetIterator(); Iter; ++Iter)
							{
								Results += **Iter;
							}
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;

					umint nWorkObjects = ((fp64(NSys::fg_Thread_GetVirtualCores()) * fp64(5.0)) / g_SecondsToWait).f_ToInt();
					try
					{
						NSys::fg_Thread_SetPriority(NSys::fg_Thread_GetCurrent(), EExecutionPriority_High);
					}
					catch(NMib::NException::CException&)
					{
						// Because on linux EExecutionPriority_High requires root.
					}

					for (umint i = 0; i < nWorkObjects; ++i)
					{
						fg_ConcurrentActor()
							(
								&CActor::f_Dispatch
								, []
								{
									int64 CyclesToWait = (NTime::CSystem_Time::fs_CyclesFrequencyFp() * g_SecondsToWait).f_ToInt();

									int64 StartCycles = NTime::NPlatform::fg_Timer_Cycles();
									while (NTime::NPlatform::fg_Timer_Cycles() - StartCycles < CyclesToWait)
										;
								}
							)
							> fg_ConcurrentActor() / [](TCAsyncResult<void> &&_Result)
							{
							}
						;
					}
					try
					{
						NSys::fg_Thread_SetPriority(NSys::fg_Thread_GetCurrent(), EExecutionPriority_Normal);
					}
					catch(NMib::NException::CException&)
					{
						// Because fg_Thread_SetPriority can throw.
					}

					if (--nExpectedHandlers == 0)
						HandlersFinished.f_SetSignaled();

					bool bTimedOutWatingForTimerHandlers = HandlersFinished.f_WaitTimeout(g_Timeout * 3.0);
					DMibTest(!DMibExpr(bTimedOutWatingForTimerHandlers));

					pActor->f_BlockDestroy();
					pLockActor->f_BlockDestroy();

					DMibLock(TimersLock);
				}

				//DMibTrace("nTestLockThread: {} nTestLockActor: {}\r\n", nTestLockThread.f_Load(), nTestLockActor.f_Load());
			};
		}

		template <umint t_Align>
		struct TCOverAlignResult
		{
			uint32 m_Value0;
			alignas(t_Align) uint32 m_Value1;
		};

		struct COverAlignActor : public CActor
		{
#if DMibPPtrBits < 64
			TCFuture<TCOverAlignResult<4>> f_Get4()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}
#endif
			TCFuture<TCOverAlignResult<8>> f_Get8()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<16>> f_Get16()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<32>> f_Get32()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<64>> f_Get64()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<128>> f_Get128()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<256>> f_Get256()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

			TCFuture<TCOverAlignResult<512>> f_Get512()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}

#if DMibPPtrBits > 32
			TCFuture<TCOverAlignResult<1024>> f_Get1024()
			{
				co_return {.m_Value0 = 5, .m_Value1 = 15};
			}
#endif
		};

		void f_OverAlignTests()
		{
			DMibTestSuite("OverAlign") -> TCFuture<void>
			{
				TCActor<COverAlignActor> Actor{fg_Construct()};
#if DMibPPtrBits < 64
				DMibExpect((co_await Actor(&COverAlignActor::f_Get4)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get4)).m_Value1, ==, 15);
#endif
				DMibExpect((co_await Actor(&COverAlignActor::f_Get8)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get8)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get16)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get16)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get32)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get32)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get64)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get64)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get128)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get128)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get256)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get256)).m_Value1, ==, 15);

				DMibExpect((co_await Actor(&COverAlignActor::f_Get512)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get512)).m_Value1, ==, 15);

#if DMibPPtrBits > 32
				DMibExpect((co_await Actor(&COverAlignActor::f_Get1024)).m_Value0, ==, 5);
				DMibExpect((co_await Actor(&COverAlignActor::f_Get1024)).m_Value1, ==, 15);
#endif
				co_return {};
			};
		}

		void f_DoTests()
		{
			f_TestAsyncResult();
			f_DestroyTests();
			f_GeneralTests();
			f_OverAlignTests();
		}
	};


	DMibTestRegister(CConcurrency_Tests, Malterlib::Concurrency);
}
