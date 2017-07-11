// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/Actor/Lock>

namespace
{
	const static fp64 g_SecondsToWait = 1.0/1000.0;
	using namespace NMib::NConcurrency;
	using namespace NMib;
	using namespace NMib::NTime;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;

	class CTestActor : public CActor
	{
	public:
		void f_TestValue()
		{

		}
		int f_TestValue1(int _Value)
		{
			return _Value;
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

		TCContinuation<void> f_TestContinuation()
		{
			TCContinuation<void> Continuation;

			self(&CTestActor::f_TestValue)
				> [Continuation](NMib::NConcurrency::TCAsyncResult<void> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;

			return Continuation;
		}
		TCContinuation<int> f_TestContinuationRet()
		{
			TCContinuation<int> Continuation;

			self(&CTestActor::f_TestValue1, 4)
				> [Continuation](NMib::NConcurrency::TCAsyncResult<int> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;


			return Continuation;
		}
	};

	class CLocalActor : public CActor
	{
	public:
		typedef CDispatchingActorHolder CActorHolder;
	};

	class CSeparateThreadActor : public CActor
	{
	public:
		typedef CSeparateThreadActorHolder CActorHolder;
	};

	
	class CConcurrency_Tests : public NMib::NTest::CTest
	{
	public:

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
				DMibTest(DMibExpr(fg_ThrowsException(Exception)) == DMibLExpr(TestException.f_Get()));
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
				
				pActor->f_Destroy() > fg_AnyConcurrentActor() / [&](TCAsyncResult<void> &&)
					{
						Event.f_SetSignaled();
					}
				;
				
				bool bTimedOut = Event.f_WaitTimeout(20.0);
				DMibExpectFalse(bTimedOut);				
			};
		}
		
		void f_GeneralTests()
		{
			DMibTestSuite("General")
			{
				CConcurrencyManager ConcurrencyManager;
				
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
					auto pLocalActor = fg_ConstructActor<CLocalActor>(fg_Construct([&](FActorQueueDispatch &&_Functor){_Functor();}));
					auto pActor = fg_ConstructActor<CTestActor>();
					auto pLockActor = fg_ConstructActor<TCLockActor<int, NThread::CMutual>>(fg_Construct("MalterlibTestSeparateActorThread"), Object, Lock);

					CActorSubscription OneshotAbortableTimer;
					CActorSubscription ExactTimer;
					CActorSubscription NormalTimer;
					NAtomic::TCAtomic<mint> nExpectedHandlers(1);
					NAtomic::TCAtomic<bool> bHandledTimer0;
					NAtomic::TCAtomic<bool> bHandledTimer1;
					NAtomic::TCAtomic<bool> bHandledTimer2;
					NMib::NThread::CEvent HandlersFinished;
					HandlersFinished.f_ResetSignaled();
					TCActorResultVector<int> VectorResults;

					for (int i = 0; i < 128*1024; ++i)
					{
						++nExpectedHandlers;
						pLockActor(&TCLockActor<int, NThread::CMutual>::f_Lock)
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
					fg_TimerActor()
						(
							&CTimerActor::f_OneshotTimer
							, 0.2
							, pLocalActor
							, [&]
							{
								if (--nExpectedHandlers == 0)
									HandlersFinished.f_SetSignaled();
							}
							, true
						)
						> fg_DiscardResult()
					;


					{
						++nExpectedHandlers;
						fg_TimerActor()
							(
								&CTimerActor::f_OneshotTimerAbortable
								, 1000000.0
								, pLocalActor
								, [&]
								{
									if (--nExpectedHandlers == 0)
										HandlersFinished.f_SetSignaled();
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
						fg_TimerActor()
							(
								&CTimerActor::f_OneshotTimerAbortable
								, 0.1
								, pLocalActor
								, [&]
								{
									if (!bHandledTimer0.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}
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
						fg_TimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 1000000.0
								, pLocalActor
								, [&]() -> TCContinuation<void>
								{
									DMibNeverGetHere;
									
									return fg_Explicit();
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
						fg_TimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 0.5
								, pLocalActor
								, [&]() -> TCContinuation<void>
								{
									if (!bHandledTimer1.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}
									
									return fg_Explicit();
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
						fg_TimerActor()
							(
								&CTimerActor::f_RegisterTimer
								, 0.5
								, pLocalActor
								, [&]() -> TCContinuation<void>
								{
									if (!bHandledTimer2.f_Exchange(true))
									{
										if (--nExpectedHandlers == 0)
											HandlersFinished.f_SetSignaled();
									}
									
									return fg_Explicit();
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
					pActor(&CTestActor::f_TestContinuation)
						> pLocalActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<void> &&_Result)
						{
							_Result.f_Get();
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;
					++nExpectedHandlers;
					pActor(&CTestActor::f_TestContinuationRet)
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
						pActor(&CTestActor::f_TestValue8, 1, 1, 1, 1, 1, 1, 1, 1) > VectorResults.f_AddResult();
					}

					++nExpectedHandlers;
					
					VectorResults.f_GetResults()
						> pActor / [&, pActor](NMib::NConcurrency::TCAsyncResult<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<int>>> &&_Result)
						{
							mint Results = 0;
							for (auto Iter = (*_Result).f_GetIterator(); Iter; ++Iter)
							{
								Results += **Iter;
							}
							if (--nExpectedHandlers == 0)
								HandlersFinished.f_SetSignaled();
						}
					;

					mint nWorkObjects = ((fp64(NSys::fg_Thread_GetVirtualCores()) * fp64(5.0)) / g_SecondsToWait).f_ToInt();
					try 
					{			
						NSys::fg_Thread_SetPriority(NSys::fg_Thread_GetCurrent(), EThreadPriority_High);
					}				
					catch(NMib::NException::CException&)
					{
						// Because on linux EThreadPriority_High requires root.
					}

					for (mint i = 0; i < nWorkObjects; ++i)
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
						NSys::fg_Thread_SetPriority(NSys::fg_Thread_GetCurrent(), EThreadPriority_Normal);
					}				
					catch(NMib::NException::CException&)
					{
						// Because fg_Thread_SetPriority can throw.
					}

					if (--nExpectedHandlers == 0)
						HandlersFinished.f_SetSignaled();
					
					bool bTimedOutWatingForTimerHandlers = HandlersFinished.f_WaitTimeout(100.0);
					DMibTest(!DMibExpr(bTimedOutWatingForTimerHandlers));

					pActor->f_DestroyNoResult(DMibPFile, DMibPLine);
					
					DMibLock(TimersLock);
				}

				ConcurrencyManager.f_BlockOnDestroy();

				//DMibTrace("nTestLockThread: {} nTestLockActor: {}\r\n", nTestLockThread.f_Load() << nTestLockActor.f_Load());
			};
		}
		void f_DoTests()
		{
			f_TestAsyncResult();
			f_DestroyTests();
			f_GeneralTests();
		}
	};

	
	DMibTestRegister(CConcurrency_Tests, Malterlib::Concurrency);
}
