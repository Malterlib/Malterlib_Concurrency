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
					self / [this]() -> TCAsyncGenerator<int32>
					{
						co_yield 5;
						co_yield 5;
						co_yield co_await f_TestFuture();
						co_yield 5;

						co_return {};
					}
				)
			;

			for co_await (auto Value : Generator)
				co_yield Value;

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

			for co_await (auto Value : Generator)
				co_yield Value;

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
			for co_await(auto Value : f_TestAsyncGenerator())
				Return += Value;
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
						[](int32 const &_Param) -> TCAsyncGenerator<int32>
						{
							co_yield 5;
							co_yield _Param;

							co_return {};
						}
						, _Param
					)
				)
			;

			for co_await (auto Value : Generator)
				co_yield Value;

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
						[](int32 &&_Param) -> TCAsyncGenerator<int32>
						{
							co_yield 5;
							co_yield _Param;

							co_return {};
						}
						, fg_Move(_Param)
					)
				)
			;

			for co_await (auto Value : Generator)
				co_yield Value;

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

			for co_await (auto Value : Generator)
				co_yield Value;

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
				DMibExpectTrue(pState->m_bStartedExecution);
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
						auto CleanupLambda = g_OnScopeExit > [pState]
							{
								if (!pState->m_bDestructed)
									DMibPDebugBreak;  // If lambda is destroyed before coroutine something is wrong
							}
						;

						TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestDestruction = [CleanupLambda = fg_Move(CleanupLambda), pState]() -> TCAsyncGenerator<int32>
							{
								auto Cleanup = g_OnScopeExit > [&]
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
						auto CleanupLambda = g_OnScopeExit > [pState]
							{
								if (!pState->m_bDestructed)
									DMibPDebugBreak;  // If lambda is destroyed before coroutine something is wrong
							}
						;

						TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestDestruction = [CleanupLambda = fg_Move(CleanupLambda), pState]() -> TCAsyncGenerator<int32>
							{
								auto Cleanup = g_OnScopeExit > [&]
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
