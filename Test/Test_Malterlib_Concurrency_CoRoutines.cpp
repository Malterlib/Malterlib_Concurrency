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
		}

		TCFuture<void> fp_StartApp(NEncoding::CEJSON const &_Params) override
		{
			return fg_Explicit();
		}

		TCFuture<void> fp_StopApp() override
		{
			return fg_Explicit();
		}

		TCFuture<uint32> f_Test()
		{
			co_return 32;
		}

		TCFuture<uint32> f_TestException()
		{
			co_return DMibErrorInstance("Test Exception");
		}

		TCFuture<uint32> f_TestFutureWithAuditor()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % Auditor);
			co_return Value;
		}

		TCFuture<uint32> f_TestFutureWithErrorWithAuditorWith()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % "Extra error" % Auditor);
			co_return Value;
		}

		TCFuture<uint32> f_TestFutureWithAuditorWithError()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % Auditor("Auditor exception"));
			co_return Value;
		}

		TCFuture<uint32> f_TestFutureWithErrorWithAuditorWithError()
		{
			auto Auditor = mp_State.f_Auditor();
			uint32 Value = co_await (f_TestException() % "Extra error" % Auditor("Auditor exception"));
			co_return Value;
		}
	};

	class CTestActor2 : public CActor
	{
	public:

		TCFuture<uint32> f_Test()
		{
			return fg_Explicit(5);
		}
	};

	struct CTestActor : public CActor
	{
		TCFuture<uint32> f_Test()
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

		TCFuture<uint32> f_TestValue(uint32 _Value)
		{
			co_return _Value;
		}

		TCFuture<uint32> f_TestFuture()
		{
			auto Promise = f_Test();
			auto Value = co_await Promise;
			co_return 5 + Value;
		}

		TCFuture<uint32> f_TestForwardAsyncResult()
		{
			auto WrappedValue = co_await f_Test().f_Wrap();
			co_return WrappedValue;
		}

		TCFuture<void> f_TestForwardVoidAsyncResult()
		{
			auto WrappedValue = co_await f_TestVoid().f_Wrap();
			co_return WrappedValue;
		}

		TCFuture<uint32> f_TestReturnPromise()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			TCPromise<uint32> Promise;
			Promise.f_SetResult(Value);
			co_return Promise;
		}

		TCFuture<uint32> f_TestException()
		{
			co_return DMibErrorInstance("Test Exception");
		}

		TCFuture<uint32> f_TestExceptionAfterSuspend()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			(void)Value;
			co_return DMibErrorInstance("Test Exception");
		}

		TCFuture<void> f_TestExceptionAfterSuspendVoid()
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			(void)Value;
			co_return DMibErrorInstance("Test Exception");
		}

		TCFuture<uint32> f_TestRecursiveException()
		{
			uint32 Value = co_await f_TestExceptionAfterSuspend();
			co_return Value;
		}

		TCFuture<uint32> f_TestRecursiveWrappedException()
		{
			auto Value = co_await f_TestExceptionAfterSuspend().f_Wrap();
			if (!Value)
				co_return Value.f_GetException();
			co_return *Value;
		}

		TCFuture<uint32> f_TestRecursiveWrappedExceptionAsyncResult()
		{
			auto Value = co_await f_TestExceptionAfterSuspend().f_Wrap();
			co_return fg_Move(Value);
		}

		TCFuture<uint32> f_TestRecursiveWrappedExceptionAsyncResultVoid()
		{
			auto Value = co_await f_TestExceptionAfterSuspendVoid().f_Wrap();
			co_return Value.f_GetException();
		}

		TCFuture<uint32> f_TestFutureWithError()
		{
			uint32 Value = co_await (f_TestExceptionAfterSuspend() % "Extra error");
			co_return Value;
		}

		TCFuture<uint32> f_TestCleanup()
		{
			auto Cleanup = g_OnScopeExitActor > [this]
				{
					m_WaitForCleanupScopeExit.f_SetResult();
				}
			;

			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			co_return 5 + Value;
		}

		TCFuture<void> f_WaitCleanupScope()
		{
			return m_WaitForCleanupScopeExit;
		}

		TCFutureCaptureExceptions<uint32> f_TestFutureWithErrorWrapped()
		{
			uint32 Value = *(co_await (f_TestExceptionAfterSuspend() % "Extra error").f_Wrap());
			co_return Value;
		}

		TCFuture<uint32> f_TestActorCallWithError()
		{
			uint32 Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error");
			co_return Value;
		}

		TCFutureCaptureExceptions<uint32> f_TestActorCallWithErrorWrapped()
		{
			auto Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error").f_Wrap();
			co_return *Value;
		}

		TCFuture<uint32> f_TestActorCallPackWithError()
		{
			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error");

			auto &[Result0, Result1] = Results;
			co_return Result0 + Result1;
		}

		TCFutureCaptureExceptions<uint32> f_TestActorCallPackWithErrorWrapped()
		{
			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error").f_Wrap();

			auto &[Result0, Result1] = Results;
			co_return *Result0 + *Result1;
		}

		TCFutureCaptureExceptions<uint32> f_TestThrowException()
		{
			DMibError("Test Exception");

			co_return 5;
		}

		TCFutureCaptureExceptions<uint32> f_TestThrowExceptionAfterSuspend()
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;

			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			DMibError("Test Exception");

			co_return Value;
		}

		TCFuture<void> f_TestVoid()
		{
			co_return {};
		}

		TCFuture<uint32> f_TestMultiCall()
		{
			auto Results = co_await (m_TestActor(&CTestActor2::f_Test) + m_TestActor(&CTestActor2::f_Test));
			auto &[Result0, Result1] = Results;

			auto Results1 = co_await (m_TestActor(&CTestActor2::f_Test) + m_TestActor(&CTestActor2::f_Test)).f_Wrap();
			auto &[Result10, Result11] = Results1;
			co_return Result0 + Result1 + *Result10 + *Result11;
		}

		TCFuture<uint32> f_TestActorResultVector()
		{
			TCActorResultVector<uint32> AsyncResults;

			m_TestActor(&CTestActor2::f_Test) > AsyncResults.f_AddResult();
			m_TestActor(&CTestActor2::f_Test) > AsyncResults.f_AddResult();

			uint32 Value = 0;

			for (auto &Result : co_await AsyncResults.f_GetResults())
				Value += *Result;

			co_return Value;
		}

		TCFuture<uint32> f_TestParameter(uint32 _Value)
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestLambdaReferenceRecursive()
		{
			uint32 Value = co_await
				(
					g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
					{
						co_return co_await [=]() -> TCFuture<uint32>
							{
								uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
								co_return Value + Value2;
							}
							()
						;
					}
				)
			;

			co_return Value;
		}

		TCFuture<uint32> f_TestLambdaReferenceRecursiveCorrected()
		{
			uint32 Value = co_await
				(
					g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
					{
						co_return co_await
							(
								g_Dispatch / [=]() -> TCFuture<uint32>
								{
									uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
									co_return Value + Value2;
								}
							)
						;
					}
				)
			;

			co_return Value;
		}

		TCFuture<uint32> f_TestLambdaReference()
		{
			uint32 Value = co_await [=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				()
			;

			co_return Value;
		}

		TCFuture<uint32> f_TestLambdaReferenceCorrected()
		{
			uint32 Value = co_await
				(
					g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
					{
						uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
						co_return Value + Value2;
					}
				)
			;

			co_return Value;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturn()
		{
			return [=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				()
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnCorrected()
		{
			return g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnRecursive()
		{
			return g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
				{
					return [=]() -> TCFuture<uint32>
						{
							uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
							co_return Value + Value2;
						}
						()
					;
				}
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnRecursiveCorrected()
		{
			return g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
				{
					return g_Dispatch / [=]() -> TCFuture<uint32>
						{
							uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
							co_return Value + Value2;
						}
					;
				}
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDispatched()
		{
			TCPromise<uint32> Promise;

			[=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	() > Promise
			;

			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDispatchedCorrected()
		{
			TCPromise<uint32> Promise;

			g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	> Promise
			;

			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoro()
		{
			TCPromise<uint32> Promise;

			[=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	() > Promise
			;

			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroCorrected()
		{
			TCPromise<uint32> Promise;

			g_Dispatch / [=, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			 	> Promise
			;

			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReference(uint32 const &_Value)
		{
			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursive()
		{
			uint32 RefValue = 20;
			uint32 Value = co_await f_TestParameterReference(RefValue);

			co_return Value;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveCorrected()
		{
			uint32 RefValue = 20;
			uint32 Value = co_await self(&CTestActor::f_TestParameterReference, RefValue);

			co_return Value;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursivePack()
		{
			uint32 RefValue = 20;
			auto [Value0, Value1] = co_await (f_TestParameterReference(RefValue) + f_TestParameterReference(RefValue));

			co_return Value0 + Value1;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursivePackCorrected()
		{
			uint32 RefValue = 20;
			auto [Value0, Value1] = co_await (self(&CTestActor::f_TestParameterReference, RefValue) + self(&CTestActor::f_TestParameterReference, RefValue));

			co_return Value0 + Value1;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoro()
		{
			uint32 RefValue = 20;
			TCPromise<uint32> Promise;
			f_TestParameterReference(RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroCorrected()
		{
			uint32 RefValue = 20;
			TCPromise<uint32> Promise;
			self(&CTestActor::f_TestParameterReference, RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturn()
		{
			uint32 RefValue = 20;
			return f_TestParameterReference(RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnCorrected()
		{
			uint32 RefValue = 20;
			return self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return f_TestParameterReference(RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParamsCorrected(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatched()
		{
			uint32 RefValue = 20;
			TCPromise<uint32> Promise;
			f_TestParameterReference(RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatchedCorrected()
		{
			uint32 RefValue = 20;
			TCPromise<uint32> Promise;
			self(&CTestActor::f_TestParameterReference, RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterNonReferenceRecursiveNoCoro()
		{
			uint32 RefValue = 20;
			TCPromise<uint32> Promise;
			f_TestParameter(RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterNonReferenceRecursiveNoCoroDirectReturn()
		{
			uint32 RefValue = 20;
			return f_TestParameter(RefValue);
		}

		TCFuture<uint32> f_TestConcurrentDispatch(uint32 _Value)
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

		TCFuture<void> f_TestCoroutineInResultCall(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]
				{
					return _Value * 2;
				}
				> [this](TCAsyncResult<uint32> _Value) mutable
				{
					g_Dispatch / [=]() mutable -> TCFuture<void>
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

		TCFuture<uint32> f_WaitCleanupResultCall()
		{
			return m_WaitForResultCallToFinish;
		}

		TCFuture<uint32> f_TestActorResultVectorUnwrapped(uint32 _Value)
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

		TCFuture<void> f_TestCoroutineInResultCallUnobservedError(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]
				{
					return _Value * 2;
				}
				> [](TCAsyncResult<uint32> _Value) mutable
				{
					g_Dispatch / [] () -> TCFuture<void>
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
		TCPromise<void> m_WaitForCleanupScopeExit;
		TCPromise<uint32> m_WaitForResultCallToFinish;
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

				DMibExpect(TestActor(&CTestActor::f_TestReturnPromise).f_CallSync(), ==, 5);
				DMibExpect(TestActor(&CTestActor::f_TestFuture).f_CallSync(), ==, 45);

				DMibExpectException(TestActor(&CTestActor::f_TestException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResult).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResultVoid).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestFutureWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
 				DMibExpectException(TestActor(&CTestActor::f_TestFutureWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithError).f_CallSync(), DMibErrorInstance("Extra error: Test Exception\nTest Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithErrorWrapped).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));

#if DMibEnableSafeCheck > 0
				NStr::CStr ReferenceThisError = "bSafeCall 'Unsafe call to coroutine with reference this pointer'";
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReference).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceRecursive).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoro).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturn).f_CallSync(), ReferenceThisError);
				#ifndef DMibInlineEnabled
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturnRecursive).f_CallSync(), ReferenceThisError);
				#endif
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDispatched).f_CallSync(), ReferenceThisError);

				NStr::CStr ReferenceParamError = "bSafeCall 'Unsafe call to coroutine with reference parameters'";
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursive).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoro).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturn).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams, 20).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDispatched).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursivePack).f_CallSync(), ReferenceParamError);
#endif

				DMibExpect(TestActor(&CTestActor::f_TestParameterReference, 25).f_CallSync(), ==, 30);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceRecursiveCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturnCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturnRecursiveCorrected).f_CallSync(), ==, 25);
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
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithAuditor).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithErrorWithAuditorWith).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithErrorWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));

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
