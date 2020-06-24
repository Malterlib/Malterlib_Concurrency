// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/AsyncGenerator>
#include <Mib/Test/Exception>

namespace
{
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
	};

	class CAsyncGenerator_Tests : public NMib::NTest::CTest
	{
		void f_TestGeneral()
		{
			DMibTestSuite("General")
			{
				TCActor<CTestActor> TestActor = fg_Construct();
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumer).f_CallSync(60.0), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumerForCoAwait).f_CallSync(60.0), ==, 21);
				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorConsumerDefaulted).f_CallSync(60.0), ==, 0);
			};
			DMibTestSuite("Access Param")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				DMibExpect(TestActor(&CTestActor::f_TestAsyncGeneratorAccessParamConsumer).f_CallSync(60.0), ==, 12);

				auto Generator = TestActor(&CTestActor::f_TestAsyncGeneratorAccessParam, 7).f_CallSync(60.0);
				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(60.0);

				DMibExpect(*iIterator, ==, 5);
				(++iIterator).f_CallSync(60.0);
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
				auto Generator = fg_CallSafe(fg_Move(fTestEager));
				DMibExpectTrue(pState->m_bStartedExecution);
			};
			DMibTestSuite("Destruction of locals when destructed early")
			{
				TCActor<> ProcessingActor = fg_Construct();
				CCurrentActorScope ActorScope(ProcessingActor);

				TCActor<CTestActor> TestActor = fg_Construct();

				struct CState
				{
					bool m_bDestructed = false;
				};
				TCSharedPointer<CState> pState = fg_Construct();

				TCFunctionMovable<TCAsyncGenerator<int32> ()> fTestDestruction = [pState]() -> TCAsyncGenerator<int32>
					{
						auto Cleanup = g_OnScopeExit > [&]
							{
								pState->m_bDestructed = true;
							}
						;
						co_yield 1;
						co_yield 2;
						co_return {};
					}
				;
				{
					auto Generator = fg_CallSafe(fg_Move(fTestDestruction));
					{
						DMibTestPath("Before");
						DMibExpectFalse(pState->m_bDestructed);
					}
					auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(60.0);
					{
						DMibTestPath("After iterator");
						DMibExpectFalse(pState->m_bDestructed);
					}
					DMibExpect(*iIterator, ==, 1);
				}
				(g_Dispatch(TestActor) / []{}).f_CallSync(60.0);
				{
					DMibTestPath("After");
					DMibExpectTrue(pState->m_bDestructed);
				}
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
					DMibExpectException(TestActor(&CTestActor::f_TestAsyncGeneratorException).f_CallSync(60.0).f_GetIterator().f_CallSync(60.0), DMibErrorInstance("Test error"));
				}
				DMibExpectException(TestActor(&CTestActor::f_TestAsyncGeneratorExceptionSecondConsumer).f_CallSync(60.0), DMibErrorInstance("Test error"));
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
