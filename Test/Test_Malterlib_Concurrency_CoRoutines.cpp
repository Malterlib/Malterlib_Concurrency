// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>
#include <Mib/Concurrency/Coroutine>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Test/Exception>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Encoding/JSONShortcuts>
#include <type_traits>

#if 0
	using namespace NMib;
	using namespace NMib::NStorage;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NMemory;

TCContinuation<void> fg_OtherFunction();
TCContinuation<void> f_OtherFunction();


//~~~+incorrect
struct CMyActor : public CActor
{
	TCContinuation<uint32> f_MyFunction(uint32 &_Value) // This will fail to compile with static_assert for reference types
	{
		co_await f_OtherFunction();
		co_return _Value; // Here _Value will have gone out of scope if f_OtherFunction takes more than 1 second
	}
	TCContinuation<void> f_MyFunction2()
	{
		uint32 Value = 5;
		uint32 ReturnValue = co_await f_MyFunction(Value).f_Timeout(1.0, "Timed out");
		co_return {};
	}
};
//~~~

//~~~+incorrect
TCContinuation<uint32> fg_MyFunction(uint32 &_Value) // This will NOT fail to compile due to inability to recognize member function calls in Coroutines TS
{
	co_await fg_OtherFunction();
	co_return _Value; // Here _Value will have gone out of scope if f_OtherFunction takes more than 1 second
}

TCContinuation<void> fg_MyFunction2()
{
	uint32 Value = 5;
	uint32 ReturnValue = co_await fg_MyFunction(Value).f_Timeout(1.0, "Timed out");
	co_return {};
}
//~~~


#endif

#if 1

template <typename T>
struct safe_promise
{
	//...

	template <typename U, std::enable_if_t<!std::is_reference_v<U>> * = nullptr>
	decltype(auto) capture_transform(U &&value)
	{
		return std::forward<U>(value);
	}

	template <typename U, std::enable_if_t<std::is_reference_v<U>> * = nullptr>
	auto capture_transform(U &&value)
	{
		return value;
	}
};

template <typename T>
struct safe_task
{
	std::experimental::suspend_never initial_suspend()
	{
		return {};
	}
	std::experimental::suspend_always final_suspend()
	{
		return {};
	}
	void return_void()
	{
	}

	void unhandled_exception()
	{
	}

	safe_task<T> get_return_object()
	{
		return {};
	}
};


namespace std::experimental
{
	template <typename T, typename ...params>
	struct coroutine_traits<safe_task<T>, params...>
	{
		using promise_type = safe_task<T>;

		template <typename source_type, bool used_after_first_suspension_point>
		struct capture_type
		{
			using type = std::remove_reference_t<source_type>;
			static constexpr bool do_capture = used_after_first_suspension_point || !std::is_reference_v<source_type>;
			static constexpr bool defer_capture = std::is_reference_v<source_type>; // Defers capture until after first suspension point
		};
	};
}

/*

 If initial_suspend returns a type in which await_ready returns true in a constexpr, capture of variables must be deferred

 */

#endif

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

	struct CTestDistributedApp : public CDistributedAppActor
	{

		safe_task<void> f_Testing123()
		{
			co_return;
		}


		CTestDistributedApp()
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings("TestDistApp")
			 	.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() + "/TestDistApp")
			 	.f_SeparateDistributionManager(true)
			 	.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
			 	.f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality_None)
			)
		{

			safe_promise<void> Test;

			auto Test2 = f_Testing123();

			int TestValue = 0;
			auto Value = Test.capture_transform(fg_TempCopy(TestValue));
			++TestValue;
		}

		TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) override
		{
			return fg_Explicit();
		}

		TCContinuation<void> fp_StopApp() override
		{
			return fg_Explicit();
		}

		TCContinuation<uint32> f_Test()
		{
			co_return 32;
		}

		TCContinuation<uint32> f_TestException()
		{
			co_return DMibErrorInstance("Test Exception");
		}

		TCContinuation<uint32> f_TestContinuationWithAuditor()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % Auditor);
			co_return Value;
		}

		TCContinuation<uint32> f_TestContinuationWithErrorWithAuditorWith()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % "Extra error" % Auditor);
			co_return Value;
		}

		TCContinuation<uint32> f_TestContinuationWithAuditorWithError()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % Auditor("Auditor exception"));
			co_return Value;
		}

		TCContinuation<uint32> f_TestContinuationWithErrorWithAuditorWithError()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % "Extra error" % Auditor("Auditor exception"));
			co_return Value;
		}
	};

	class CTestActor2 : public CActor
	{
	public:

		TCContinuation<uint32> f_Test()
		{
			return fg_Explicit(5);
		}
	};

	struct CTestActor : public CActor
	{
		TCContinuation<uint32> f_Test()
		{
			auto ValueResult = co_await m_TestActor(&CTestActor2::f_Test).f_Wrap();
			uint32 Value = *ValueResult;
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			Value += co_await m_TestActor(&CTestActor2::f_Test);
			co_return Value + 5;
		}

		TCContinuation<uint32> f_TestValue(uint32 _Value)
		{
			co_return _Value;
		}

		TCContinuation<uint32> f_TestContinuation()
		{
			auto Continuation = f_Test();
			auto Value = co_await Continuation;
			co_return 5 + Value;
		}

		TCContinuation<uint32> f_TestForwardAsyncResult()
		{
			auto WrappedValue = co_await f_Test().f_Wrap();
			co_return WrappedValue;
		}

		TCContinuation<void> f_TestForwardVoidAsyncResult()
		{
			auto WrappedValue = co_await f_TestVoid().f_Wrap();
			co_return WrappedValue;
		}

		TCContinuation<uint32> f_TestReturnContinuation()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			TCContinuation<uint32> Continuation;
			Continuation.f_SetResult(Value);
			co_return Continuation;
		}

		TCContinuation<uint32> f_TestException()
		{
			co_return DMibErrorInstance("Test Exception");
		}

		TCContinuation<uint32> f_TestExceptionAfterSuspend()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			(void)Value;
			co_return DMibErrorInstance("Test Exception");
		}

		TCContinuation<void> f_TestExceptionAfterSuspendVoid()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			(void)Value;
			co_return DMibErrorInstance("Test Exception");
		}

		TCContinuation<uint32> f_TestRecursiveException()
		{
			uint32 Value = co_await f_TestExceptionAfterSuspend();
			co_return Value;
		}

		TCContinuation<uint32> f_TestRecursiveWrappedException()
		{
			auto Value = co_await f_TestExceptionAfterSuspend().f_Wrap();
			if (!Value)
				co_return Value.f_GetException();
			co_return *Value;
		}

		TCContinuation<uint32> f_TestRecursiveWrappedExceptionAsyncResult()
		{
			auto Value = co_await f_TestExceptionAfterSuspend().f_Wrap();
			co_return fg_Move(Value);
		}

		TCContinuation<uint32> f_TestRecursiveWrappedExceptionAsyncResultVoid()
		{
			auto Value = co_await f_TestExceptionAfterSuspendVoid().f_Wrap();
			co_return Value.f_GetException();
		}

		TCContinuation<uint32> f_TestContinuationWithError()
		{
			uint32 Value = co_await (f_TestExceptionAfterSuspend() % "Extra error");
			co_return Value;
		}

		TCContinuation<uint32> f_TestCleanup()
		{
			auto Cleanup = g_OnScopeExitActor > [this]
				{
					m_WaitForCleanupScopeExit.f_SetResult();
				}
			;

			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			co_return 5 + Value;
		}

		TCContinuation<void> f_WaitCleanupScope()
		{
			return m_WaitForCleanupScopeExit;
		}

		TCContinuationCaptureExceptions<uint32> f_TestContinuationWithErrorWrapped()
		{
			uint32 Value = *(co_await (f_TestExceptionAfterSuspend() % "Extra error").f_Wrap());
			co_return Value;
		}

		TCContinuation<uint32> f_TestActorCallWithError()
		{
			uint32 Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error");
			co_return Value;
		}

		TCContinuationCaptureExceptions<uint32> f_TestActorCallWithErrorWrapped()
		{
			auto Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error").f_Wrap();
			co_return *Value;
		}

		TCContinuation<uint32> f_TestActorCallPackWithError()
		{
			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error");

			auto &[Result0, Result1] = Results;
			co_return Result0 + Result1;
		}

		TCContinuationCaptureExceptions<uint32> f_TestActorCallPackWithErrorWrapped()
		{
			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error").f_Wrap();

			auto &[Result0, Result1] = Results;
			co_return *Result0 + *Result1;
		}

		TCContinuationCaptureExceptions<uint32> f_TestThrowException()
		{
			DMibError("Test Exception");

			co_return 5;
		}

		TCContinuationCaptureExceptions<uint32> f_TestThrowExceptionAfterSuspend()
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;

			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			DMibError("Test Exception");

			co_return Value;
		}

		TCContinuation<void> f_TestVoid()
		{
			co_return {};
		}

		TCContinuation<uint32> f_TestMultiCall()
		{
			auto Results = co_await (m_TestActor(&CTestActor2::f_Test) + m_TestActor(&CTestActor2::f_Test));
			auto &[Result0, Result1] = Results;

			auto Results1 = co_await (m_TestActor(&CTestActor2::f_Test) + m_TestActor(&CTestActor2::f_Test)).f_Wrap();
			auto &[Result10, Result11] = Results1;
			co_return Result0 + Result1 + *Result10 + *Result11;
		}

		TCContinuation<uint32> f_TestActorResultVector()
		{
			TCActorResultVector<uint32> AsyncResults;

			m_TestActor(&CTestActor2::f_Test) > AsyncResults.f_AddResult();
			m_TestActor(&CTestActor2::f_Test) > AsyncResults.f_AddResult();

			uint32 Value = 0;

			for (auto &Result : co_await AsyncResults.f_GetResults())
				Value += *Result;

			co_return Value;
		}

		TCContinuation<uint32> f_TestParameter(uint32 _Value)
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			co_return Value + _Value;
		}

		TCContinuation<uint32> f_TestLambdaReference()
		{
			uint32 Value = co_await [=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				()
			;

			co_return Value;
		}

		TCContinuation<uint32> f_TestLambdaReferenceCorrected()
		{
			uint32 Value = co_await
				(
					g_Dispatch / [=, Value2 = 20]() -> TCContinuation<uint32>
					{
						uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
						co_return Value + Value2;
					}
				)
			;

			co_return Value;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoroDirectReturn()
		{
			return [=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				()
			;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoroDirectReturnCorrected()
		{
			return g_Dispatch / [=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoroDispatched()
		{
			TCContinuation<uint32> Continuation;

			[=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	() > Continuation
			;

			return Continuation;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoroDispatchedCorrected()
		{
			TCContinuation<uint32> Continuation;

			g_Dispatch / [=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	> Continuation
			;

			return Continuation;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoro()
		{
			TCContinuation<uint32> Continuation;

			[=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	() > Continuation
			;

			return Continuation;
		}

		TCContinuation<uint32> f_TestLambdaReferenceNoCoroCorrected()
		{
			TCContinuation<uint32> Continuation;

			g_Dispatch / [=, Value2 = 20]() -> TCContinuation<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	> Continuation
			;

			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterReference(uint32 const &_Value)
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			co_return Value + _Value;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursive()
		{
			uint32 RefValue = 20;
			uint32 Value = co_await f_TestParameterReference(RefValue);

			co_return Value;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveCorrected()
		{
			uint32 RefValue = 20;
			uint32 Value = co_await self(&CTestActor::f_TestParameterReference, RefValue);

			co_return Value;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursivePack()
		{
			uint32 RefValue = 20;
			auto [Value0, Value1] = co_await (f_TestParameterReference(RefValue) + f_TestParameterReference(RefValue));

			co_return Value0 + Value1;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursivePackCorrected()
		{
			uint32 RefValue = 20;
			auto [Value0, Value1] = co_await (self(&CTestActor::f_TestParameterReference, RefValue) + self(&CTestActor::f_TestParameterReference, RefValue));

			co_return Value0 + Value1;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoro()
		{
			uint32 RefValue = 20;
			TCContinuation<uint32> Continuation;
			f_TestParameterReference(RefValue) > Continuation;
			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroCorrected()
		{
			uint32 RefValue = 20;
			TCContinuation<uint32> Continuation;
			self(&CTestActor::f_TestParameterReference, RefValue) > Continuation;
			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturn()
		{
			uint32 RefValue = 20;
			return f_TestParameterReference(RefValue);
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnCorrected()
		{
			uint32 RefValue = 20;
			return self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return f_TestParameterReference(RefValue);
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParamsCorrected(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDispatched()
		{
			uint32 RefValue = 20;
			TCContinuation<uint32> Continuation;
			f_TestParameterReference(RefValue) > Continuation;
			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterReferenceRecursiveNoCoroDispatchedCorrected()
		{
			uint32 RefValue = 20;
			TCContinuation<uint32> Continuation;
			self(&CTestActor::f_TestParameterReference, RefValue) > Continuation;
			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterNonReferenceRecursiveNoCoro()
		{
			uint32 RefValue = 20;
			TCContinuation<uint32> Continuation;
			f_TestParameter(RefValue) > Continuation;
			return Continuation;
		}

		TCContinuation<uint32> f_TestParameterNonReferenceRecursiveNoCoroDirectReturn()
		{
			uint32 RefValue = 20;
			return f_TestParameter(RefValue);
		}

		TCContinuation<uint32> f_TestConcurrentDispatch(uint32 _Value)
		{
			auto [Value0, Value1] = co_await
				(
					g_ConcurrentDispatch / [_Value]
					{
						return _Value * 2;
					}
					+ g_ConcurrentDispatch / [_Value]
					{
						return _Value * 2;
					}
					^ "Error in dispatches"
				)
			;

			co_return Value0 + Value1;
		}

		TCContinuation<void> f_TestCoroutineInResultCall(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]
				{
					return _Value * 2;
				}
				> [this](TCAsyncResult<uint32> _Value) mutable
				{
					g_Dispatch / [=]() mutable -> TCContinuation<void>
						{
							if (!_Value)
							{
								m_WaitForResultCallToFinish.f_SetException(_Value.f_GetException());
								co_return {};
							}

							co_await m_TestActor(&CTestActor2::f_Test);
							m_WaitForResultCallToFinish.f_SetResult(_Value);

							co_return {};
						}
						> fg_DiscardResult()
					;
				}
			;

			co_return {};
		}

		TCContinuation<uint32> f_WaitCleanupResultCall()
		{
			return m_WaitForResultCallToFinish;
		}

		TCContinuation<uint32> f_TestActorResultVectorUnwrapped(uint32 _Value)
		{
			TCActorResultVector<uint32> Results;
			for (mint i = 0; i < 10; ++i)
				m_TestActor(&CTestActor2::f_Test) > Results.f_AddResult();

			auto Results2 = co_await Results.f_GetResults(); /// | g_Unwrap;

			uint32 Value = 0;
			for (auto &Result : Results2)
				Value += *Result;

			co_return Value + _Value;
		}

		TCContinuation<void> f_TestCoroutineInResultCallUnobservedError(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]
				{
					return _Value * 2;
				}
				> [](TCAsyncResult<uint32> _Value) mutable
				{
					g_Dispatch / [] () -> TCContinuation<void>
	 					{
							co_return DMibErrorInstance("Observed");
						}
						> [](TCAsyncResult<void> &&_Result)
						{
						}
					;
				}
			;

			co_return {};
		}

		TCActor<CTestActor2> m_TestActor = fg_Construct();
		TCContinuation<void> m_WaitForCleanupScopeExit;
		TCContinuation<uint32> m_WaitForResultCallToFinish;
	};

	class CCoroutines_Tests : public NMib::NTest::CTest
	{
		void f_TestGeneral()
		{
			DMibTestSuite("General")
			{
				TCActor<CTestActor> TestActor = fg_Construct();

				DMibExpect(TestActor(&CTestActor::f_Test).f_CallSync(), ==, 40);
				DMibExpect(TestActor(&CTestActor::f_TestValue, 55).f_CallSync(), ==, 55);

				DMibExpect(TestActor(&CTestActor::f_TestForwardAsyncResult).f_CallSync(), ==, 40);
				TestActor(&CTestActor::f_TestForwardVoidAsyncResult).f_CallSync();


				DMibExpect(TestActor(&CTestActor::f_TestCleanup).f_CallSync(), ==, 10);
				TestActor(&CTestActor::f_WaitCleanupScope).f_CallSync();

				TestActor(&CTestActor::f_TestVoid).f_CallSync();

				DMibExpect(TestActor(&CTestActor::f_TestReturnContinuation).f_CallSync(), ==, 5);
				DMibExpect(TestActor(&CTestActor::f_TestContinuation).f_CallSync(), ==, 45);

				DMibExpectException(TestActor(&CTestActor::f_TestException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResult).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResultVoid).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestContinuationWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
 				DMibExpectException(TestActor(&CTestActor::f_TestContinuationWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception\nTest Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));

				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReference).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference this pointer'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoro).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference this pointer'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturn).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference this pointer'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDispatched).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference this pointer'");

				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursive).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoro).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturn).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams, 20).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDispatched).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursivePack).f_CallSync(), "bSafeCall 'Unsafe call to coroutine with reference parameters'");

				DMibExpect(TestActor(&CTestActor::f_TestParameterReference, 25).f_CallSync(), ==, 30);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturnCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDispatchedCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursiveCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturnCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParamsCorrected, 20).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDispatchedCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterReferenceRecursivePackCorrected).f_CallSync(), ==, 50);

				DMibExpect(TestActor(&CTestActor::f_TestParameterNonReferenceRecursiveNoCoro).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestParameterNonReferenceRecursiveNoCoroDirectReturn).f_CallSync(), ==, 25);

				DMibExpect(TestActor(&CTestActor::f_TestMultiCall).f_CallSync(), ==, 20);
				DMibExpect(TestActor(&CTestActor::f_TestActorResultVector).f_CallSync(), ==, 10);

				DMibExpect(TestActor(&CTestActor::f_TestConcurrentDispatch, 2).f_CallSync(), ==, 8);

				TestActor(&CTestActor::f_TestCoroutineInResultCall, 2).f_CallSync();
				DMibExpect(TestActor(&CTestActor::f_WaitCleanupResultCall).f_CallSync(), ==, 4);

				DMibExpect(TestActor(&CTestActor::f_TestActorResultVectorUnwrapped, 2).f_CallSync(), ==, 5 * 10 + 2);

				TestActor(&CTestActor::f_TestCoroutineInResultCallUnobservedError, 2).f_CallSync();
			};
		}

		void f_TestAuditor()
		{
			DMibTestSuite("Auditor")
			{
				auto AppActor = fg_ConstructActor<CTestDistributedApp>();
				AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJSON{}, TCActor<>{}, EDistributedAppType_InProcess).f_CallSync();

				DMibExpect(AppActor(&CTestDistributedApp::f_Test).f_CallSync(), ==, 32);
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestContinuationWithAuditor).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestContinuationWithErrorWithAuditorWith).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestContinuationWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestContinuationWithErrorWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));

				AppActor->f_BlockDestroy();
			};
		}

		void f_DoTests()
		{
			f_TestGeneral();
			f_TestAuditor();
		}
	};

	DMibTestRegister(CCoroutines_Tests, Malterlib::Concurrency);
}
