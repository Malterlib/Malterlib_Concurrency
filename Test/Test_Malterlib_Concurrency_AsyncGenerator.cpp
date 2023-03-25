// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/AsyncGenerator>
#include <Mib/Test/Exception>

namespace
{
	fp64 g_Timeout = 60.0 * NMib::NTest::gc_TimeoutMultiplier;

	using namespace NMib;
	using namespace NMib::NStorage;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NMemory;

	struct CTestDestruction
	{
		CTestDestruction() = default;
		CTestDestruction(CTestDestruction const &_Other)
			: m_Test(_Other.m_Test)
		{
			DMibFastCheck(m_Test == 20);
		}
		CTestDestruction(CTestDestruction &&_Other)
			: m_Test(fg_Exchange(_Other.m_Test, 0))
		{
		}
		~CTestDestruction()
		{
			m_Test = 0;
		}

		mint m_Test = 20;
	};

	struct CTestActor : public CActor
	{
		TCFuture<int32> f_TestFuture()
		{
			co_return 6;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorSelf()
		{
			auto Generator = co_await
				(
					self / [this, TestDestruction = CTestDestruction{}]() -> TCAsyncGenerator<int32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						co_yield 5;
						co_yield 5;
						co_yield co_await f_TestFuture();
						co_yield 5;

						co_return {};
					}
				)
			;

			for (auto iValue = co_await fg_Move(Generator).f_GetIterator(); iValue; co_await ++iValue)
				co_yield *iValue;

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorSelfConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await f_TestAsyncGeneratorSelf().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorSelf2()
		{
			auto Generator = co_await self(&CTestActor::f_TestAsyncGenerator);

			for (auto iValue = co_await fg_Move(Generator).f_GetIterator(); iValue; co_await ++iValue)
				co_yield *iValue;

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorSelf2Consumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await f_TestAsyncGeneratorSelf2().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGenerator()
		{
			co_yield 5;
			co_yield 5;
			co_yield co_await f_TestFuture();
			co_yield 5;

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await f_TestAsyncGenerator().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCFuture<int32> f_TestAsyncGeneratorConsumerForCoAwait()
		{
			int32 Return = 0;
			for (auto iValue = co_await f_TestAsyncGenerator().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorException()
		{
			co_return DMibErrorInstance("Test error");
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorExceptionSecond()
		{
			co_yield 1;
			co_return DMibErrorInstance("Test error");
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorExceptionCaptured()
		{
			co_await ECoroutineFlag_CaptureExceptions;
			DMibError("Test error");

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorExceptionSecondConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await f_TestAsyncGeneratorExceptionSecond().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCFuture<int32> f_TestAsyncGeneratorConsumerDefaulted()
		{
			int32 Return = 0;
			for (auto iValue = co_await TCAsyncGenerator<int32>().f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorAccessParam(int32 const &_Param)
		{
			co_yield 5;
			co_yield _Param;

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorAccessParamConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await fg_CallSafe(this, &CTestActor::f_TestAsyncGeneratorAccessParam, 7).f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorAccessParamSelf(int32 const &_Param)
		{
			auto Generator = co_await
				(
					self.f_Invoke
					(
						[TestDestruction = CTestDestruction{}](int32 const &_Param) -> TCAsyncGenerator<int32>
						{
							auto Cleanup = g_OnScopeExit / [&]
								{
									DMibFastCheck(TestDestruction.m_Test == 20);
								}
							;
							co_yield 5;
							co_yield _Param;

							co_return {};
						}
						, _Param
					)
				)
			;

			for (auto iValue = co_await fg_Move(Generator).f_GetIterator(); iValue; co_await ++iValue)
				co_yield *iValue;

			co_return {};

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorAccessParamSelfConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await fg_CallSafe(this, &CTestActor::f_TestAsyncGeneratorAccessParamSelf, 7).f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorAccessParamSelfMove(int32 &&_Param)
		{
			auto Generator = co_await
				(
					self.f_Invoke
					(
						[TestDestruction = CTestDestruction{}](int32 &&_Param) -> TCAsyncGenerator<int32>
						{
							auto Cleanup = g_OnScopeExit / [&]
								{
									DMibFastCheck(TestDestruction.m_Test == 20);
								}
							;
							co_yield 5;
							co_yield _Param;

							co_return {};
						}
						, fg_Move(_Param)
					)
				)
			;

			for (auto iValue = co_await fg_Move(Generator).f_GetIterator(); iValue; co_await ++iValue)
				co_yield *iValue;

			co_return {};

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorAccessParamSelfMoveConsumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await fg_CallSafe(this, &CTestActor::f_TestAsyncGeneratorAccessParamSelfMove, 7).f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}

		TCAsyncGenerator<int32> f_TestAsyncGeneratorAccessParamSelf2(int32 const &_Param)
		{
			auto Generator = co_await self(&CTestActor::f_TestAsyncGeneratorAccessParam, _Param);

			for (auto iValue = co_await fg_Move(Generator).f_GetIterator(); iValue; co_await ++iValue)
				co_yield *iValue;

			co_return {};
		}

		TCFuture<int32> f_TestAsyncGeneratorAccessParamSelf2Consumer()
		{
			int32 Return = 0;
			for (auto iValue = co_await fg_CallSafe(this, &CTestActor::f_TestAsyncGeneratorAccessParamSelf2, 7).f_GetIterator(); iValue; co_await ++iValue)
				Return += *iValue;
			co_return Return;
		}
	};

	class CAsyncGenerator_Tests : public NMib::NTest::CTest
	{
		void f_TestGeneral()
		{
			DMibTestSuite("General")
			{
				TCActor<CTestActor> TestActor = fg_Construct();
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumer).f_CallSync(g_Timeout), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorSelfConsumer).f_CallSync(g_Timeout), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorSelf2Consumer).f_CallSync(g_Timeout), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumerForCoAwait).f_CallSync(g_Timeout), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumerDefaulted).f_CallSync(g_Timeout), ==, 0);
			};
			DMibTestSuite("Access Param")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorAccessParamConsumer).f_CallSync(g_Timeout), ==, 12);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorAccessParamSelfConsumer).f_CallSync(g_Timeout), ==, 12);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorAccessParamSelfMoveConsumer).f_CallSync(g_Timeout), ==, 12);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorAccessParamSelf2Consumer).f_CallSync(g_Timeout), ==, 12);

				auto Generator = TestActor(&CTestActor::f_TestAsyncGeneratorAccessParam, 7).f_CallSync(g_Timeout);
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				DMibExpect(*iIterator, ==, 5);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 7);
			};
			DMibTestSuite("Eager Execution")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				struct CState
				{
					bool m_bStartedExecution = false;
				};
				TCSharedPointer<CState> pState = fg_Construct();

				TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestEager = [pState]() -> TCAsyncGenerator<int32>
					{
						pState->m_bStartedExecution = true;
						co_yield 1;
						co_return {};
					}
				;
				auto Generator = (g_Dispatch(TestActor) / (fg_Move(fTestEager))).f_CallSync(g_Timeout);
				DMibExpectFalse(pState->m_bStartedExecution);
			};
			DMibTestSuite("Eager Execution Iterator") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				struct CState
				{
					bool m_bStartedExecution = false;
				};
				TCSharedPointer<CState> pState = fg_Construct();

				TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestEager = [pState]() -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						pState->m_bStartedExecution = true;
						co_yield 1;
						co_yield 1;
						co_return {};
					}
				;
				auto Generator = co_await (g_Dispatch(TestActor) / (fg_Move(fTestEager))).f_Timeout(g_Timeout, "Timeout");
				auto iIterator = co_await fg_Move(Generator).f_GetIterator();

				co_await (g_Dispatch(TestActor) / []{}).f_Timeout(g_Timeout, "Timeout");

				DMibExpectTrue(pState->m_bStartedExecution);

				co_return {};
			};
			DMibTestSuite("Destruction of locals when destructed early on actor")
			{
				for (mint i = 0; i < 100; ++i)
				{
					using namespace NStr;
					DMibTestPath("{}"_f << i);

					TCActor<> ProcessingActor = fg_Construct();
					CCurrentActorScope ActorScope(ProcessingActor);

					TCActor<CTestActor> TestActor = fg_Construct();

					struct CState
					{
						bool m_bDestructed = false;
						mint m_ThreadID = 0;
						NThread::CEvent m_Event;
					};
					TCSharedPointer<CState> pState = fg_Construct();

					TCFuture<void> DestroyIterator;
					{
						auto CleanupLambda = g_OnScopeExit / [pState]
							{
								if (!pState->m_bDestructed)
									DMibPDebugBreak;  // If lambda is destroyed before coroutine something is wrong
							}
						;

						TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestDestruction = [CleanupLambda = fg_Move(CleanupLambda), pState]() -> TCAsyncGenerator<int32>
							{
								auto Cleanup = g_OnScopeExit / [&]
									{
										pState->m_Event.f_WaitTimeout(g_Timeout);
										pState->m_ThreadID = NSys::fg_Thread_GetCurrentUID();
										pState->m_bDestructed = true;
									}
								;
								co_yield 1;
								co_yield 2;
								co_return {};
							}
						;
						auto Generator = (g_Dispatch(TestActor) / fg_Move(fTestDestruction)).f_CallSync(g_Timeout);
						{
							DMibTestPath("Before");
							DMibExpectFalse(pState->m_bDestructed);
						}
						auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);
						{
							DMibTestPath("After iterator");
							DMibExpectFalse(pState->m_bDestructed);
						}
						DMibExpect(*iIterator, ==, 1);
						DestroyIterator = fg_Move(iIterator).f_Destroy();
					}
					pState->m_Event.f_SetSignaled();
					fg_Move(DestroyIterator).f_CallSync(g_Timeout);
					(g_Dispatch(TestActor) / []{}).f_CallSync(g_Timeout);
					{
						DMibTestPath("After");
						DMibExpectTrue(pState->m_bDestructed);
						DMibExpect(pState->m_ThreadID, !=, NSys::fg_Thread_GetCurrentUID());

					}
				}
			};
			DMibTestSuite("Destruction of locals when destructed early on main")
			{
				for (mint i = 0; i < 100; ++i)
				{
					using namespace NStr;
					DMibTestPath("{}"_f << i);

					TCActor<> ProcessingActor = fg_Construct();
					CCurrentActorScope ActorScope(ProcessingActor);

					TCActor<CTestActor> TestActor = fg_Construct();

					struct CState
					{
						bool m_bDestructed = false;
						NThread::CEvent m_Event;
					};
					TCSharedPointer<CState> pState = fg_Construct();

					{
						auto CleanupLambda = g_OnScopeExit / [pState]
							{
								if (!pState->m_bDestructed)
									DMibPDebugBreak;  // If lambda is destroyed before coroutine something is wrong
							}
						;

						TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestDestruction = [CleanupLambda = fg_Move(CleanupLambda), pState]() -> TCAsyncGenerator<int32>
							{
								auto Cleanup = g_OnScopeExit / [&]
									{
										pState->m_Event.f_SetSignaled();
										pState->m_bDestructed = true;
									}
								;
								co_yield 1;
								co_yield 2;
								co_return {};
							}
						;
						auto Generator = (g_Dispatch(TestActor) / fg_Move(fTestDestruction)).f_CallSync(g_Timeout);
						{
							DMibTestPath("Before");
							DMibExpectFalse(pState->m_bDestructed);
						}
						auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);
						{
							DMibTestPath("After iterator");
							DMibExpectFalse(pState->m_bDestructed);
						}
						DMibExpect(*iIterator, ==, 1);
					}
					pState->m_Event.f_WaitTimeout(60.0);
					(g_Dispatch(TestActor) / []{}).f_CallSync(g_Timeout);
					{
						DMibTestPath("After");
						DMibExpectTrue(pState->m_bDestructed);
					}
				}
			};
			DMibTestSuite("Callsafe param conversion function")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				TCFunctionMovable<TCAsyncGenerator<int32> (int32 const &)> fTestEager = [](int32 const &_Value) -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						co_yield _Value;
						co_return {};
					}
				;
				auto Generator =
					(
						g_Dispatch / [fTestEager = fg_Move(fTestEager)]() mutable
						{
							return fg_CallSafe(fg_Move(fTestEager), int64(5));
						}
					)
					.f_CallSync(g_Timeout)
				;
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				DMibExpect(*iIterator, ==, 1);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 5);
			};
			DMibTestSuite("Callsafe param conversion lambda")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				auto Generator =
					(
						g_Dispatch / []
						{
							return fg_CallSafe
								(
									[](int32 const &_Value) -> TCAsyncGenerator<int32>
									{
										co_yield 1;
										co_yield _Value;
										co_return {};
									}
									, int64(5)
								)
							;
						}
					)
					.f_CallSync(g_Timeout)
				;
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				DMibExpect(*iIterator, ==, 1);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 5);
			};
			DMibTestSuite("Dispatch param conversion function")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				TCFunctionMovable<TCAsyncGenerator<int32> (int32 const &)> fTestEager = [](int32 const &_Value) -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						co_yield _Value;
						co_return {};
					}
				;
				auto Generator = fg_Dispatch(TestActor, fg_Move(fTestEager), int64(5)).f_CallSync(g_Timeout);
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				DMibExpect(*iIterator, ==, 1);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 5);
			};
			DMibTestSuite("Dispatch param conversion lambda")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				auto Generator = fg_Dispatch
					(
						TestActor
						, [](int32 const &_Value) -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							co_yield _Value;
							co_return {};
						}
						, int64(5)
					)
					.f_CallSync(g_Timeout)
				;
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				DMibExpect(*iIterator, ==, 1);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 5);
			};
			DMibTestSuite("Transfer ownership")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CSeparateThreadActor> TestActor(fg_Construct(), "TestActor");
				TCActor<CSeparateThreadActor> TestOtherActor(fg_Construct(), "TestOtherActor");

				auto Generator = fg_Dispatch
					(
						TestActor
						, [TestOtherActor, TestActor]() -> TCAsyncGenerator<int32>
						{
							co_await NConcurrency::fg_ContinueRunningOnActor(TestOtherActor);
							DMibFastCheck(fg_CurrentActor() == TestOtherActor);

							auto RunOnActor1 = TestOtherActor;
							auto RunOnActor2 = TestActor;

							for (mint i = 0; i < 100; ++i)
							{
								if ((i % 10) == 0)
								{
									fg_Swap(RunOnActor1, RunOnActor2);
									co_await NConcurrency::fg_ContinueRunningOnActor(RunOnActor1);
								}

								co_yield i;

								DMibFastCheck(fg_CurrentActor() == RunOnActor1);
							}

							co_return {};
						}
					)
					.f_CallSync(g_Timeout)
				;
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);

				TCVector<mint> ExpectedResults;
				TCVector<mint> Results;
				for (mint i = 0; i < 100; ++i)
				{
					Results.f_Insert(*iIterator);
					ExpectedResults.f_Insert(i);
					(++iIterator).f_CallSync(g_Timeout);
				}
				DMibExpect(Results, ==, ExpectedResults);
			};
			DMibTestSuite("Abort Before Iterator") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, []() -> TCAsyncGenerator<int32>
						{
							for (mint i = 0; i < 300; ++i)
							{
								if (co_await g_bShouldAbort)
								{
									break;
								}
								co_await fg_Timeout(0.1);
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);
				co_await fg_Move(Generator).f_Destroy();

				DMibExpect(Clock.f_GetTime(), <, 5.0);

				co_return {};
			};
			DMibTestSuite("Abort After Iterator") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, []() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							for (mint i = 0; i < 300; ++i)
							{
								if (co_await g_bShouldAbort)
								{
									break;
								}
								co_await fg_Timeout(0.1);
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);

				auto iIterator = co_await fg_Move(Generator).f_GetIterator();

				co_await fg_Move(iIterator).f_Destroy();

				DMibExpect(Clock.f_GetTime(), <, 5.0);

				co_return {};
			};
			DMibTestSuite("Abort After Iterator Wait") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				TCSharedPointer<NAtomic::TCAtomic<bool>> pAborted = fg_Construct(false);
				TCPromise<void> AbortedPromise;

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, [pAborted, AbortedPromise]() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							for (mint i = 0; i < 300; ++i)
							{
								if (co_await g_bShouldAbort)
								{
									pAborted->f_Exchange(true);
									AbortedPromise.f_SetResult();
									break;
								}
								co_await fg_Timeout(0.1);
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);

				auto iIterator = co_await fg_Move(Generator).f_GetIterator();

				co_await fg_Move(iIterator).f_Destroy();
				co_await AbortedPromise.f_Future();

				DMibExpect(Clock.f_GetTime(), <, 5.0);
				DMibExpectTrue(pAborted->f_Load());

				co_return {};
			};
			DMibTestSuite("Abort After Iterator Spin") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, []() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							while (true)
							{
								if (co_await g_bShouldAbort)
								{
									break;
								}
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);

				auto iIterator = co_await fg_Move(Generator).f_GetIterator();

				co_await fg_Move(iIterator).f_Destroy();

				DMibExpect(Clock.f_GetTime(), <, 5.0);

				co_return {};
			};
			DMibTestSuite("Abort After Iterator Spin Wait") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				TCSharedPointer<NAtomic::TCAtomic<bool>> pAborted = fg_Construct(false);
				TCPromise<void> AbortedPromise;

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, [pAborted, AbortedPromise]() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							while (true)
							{
								if (co_await g_bShouldAbort)
								{
									pAborted->f_Exchange(true);
									AbortedPromise.f_SetResult();
									break;
								}
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);

				auto iIterator = co_await fg_Move(Generator).f_GetIterator();

				co_await fg_Move(iIterator).f_Destroy();
				co_await AbortedPromise.f_Future();

				DMibExpect(Clock.f_GetTime(), <, 5.0);
				DMibExpectTrue(pAborted->f_Load());

				co_return {};
			};
			DMibTestSuite("Abort After Iterator Spin Wait Clear") -> TCFuture<void>
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				TCSharedPointer<NAtomic::TCAtomic<bool>> pAborted = fg_Construct(false);
				TCPromise<void> AbortedPromise;

				auto Generator = co_await fg_Dispatch
					(
						TestActor
						, [pAborted, AbortedPromise]() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							while (true)
							{
								if (co_await g_bShouldAbort)
								{
									pAborted->f_Exchange(true);
									AbortedPromise.f_SetResult();
									break;
								}
							}
							co_return {};
						}
					)
				;

				NTime::CClock Clock(true);

				auto iIterator = co_await fg_Move(Generator).f_GetIterator();
				{
					auto Functor = [iIterator = fg_Move(iIterator)]{};
				}
				co_await AbortedPromise.f_Future();

				DMibExpect(Clock.f_GetTime(), <, 5.0);
				DMibExpectTrue(pAborted->f_Load());

				co_return {};
			};
		}

		void f_TestException()
		{
			DMibTestSuite("Exception")
			{
				TCActor<CTestActor> TestActor = fg_Construct();
				{
					TCActor<> ProcessingActor = fg_Construct();
					CCurrentActorScope ActorScope(ProcessingActor);
					DMibExpectException(TestActor(&CTestActor::f_TestAsyncGeneratorException).f_CallSync(g_Timeout).f_GetIterator().f_CallSync(g_Timeout), DMibErrorInstance("Test error"));
					DMibExpectException
						(
							TestActor(&CTestActor::f_TestAsyncGeneratorExceptionCaptured).f_CallSync(g_Timeout).f_GetIterator().f_CallSync(g_Timeout)
							, DMibErrorInstance("Test error")
						)
					;
				}
				DMibExpectException(TestActor(&CTestActor::f_TestAsyncGeneratorExceptionSecondConsumer).f_CallSync(g_Timeout), DMibErrorInstance("Test error"));
			};
		}

		void f_DoTests()
		{
			f_TestGeneral();
			f_TestException();
		}
	};

	DMibTestRegister(CAsyncGenerator_Tests, Malterlib::Concurrency);
}
