// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Coroutine>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Concurrency/LogError>
#include <Mib/Test/Exception>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NConcurrency::NTest
{
	using namespace NMib;
	using namespace NMib::NStorage;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NMemory;

	DMibImpErrorClassDefine(CCustomException, NMib::NException::CException);
	DMibImpErrorClassImplement(CCustomException);

	DMibImpErrorClassDefine(CCustomException2, NMib::NException::CException);
	DMibImpErrorClassImplement(CCustomException2);

#	define DMibCustomException(_Description) DMibImpError(CCustomException, _Description)
#	define DMibCustomExceptionInstance(_Description) DMibImpErrorInstance(CCustomException, _Description)

#	define DMibCustomException2(_Description) DMibImpError(CCustomException2, _Description)
#	define DMibCustomException2Instance(_Description) DMibImpErrorInstance(CCustomException2, _Description)

	struct CTestDistributedApp : public CDistributedAppActor
	{
		CTestDistributedApp(NStr::CStr const &_Name)
			: CDistributedAppActor
			(
				CDistributedAppActor_Settings(_Name)
				.f_RootDirectory(NFile::CFile::fs_GetProgramDirectory() / _Name)
				.f_SeparateDistributionManager(true)
				.f_KeySetting(NConcurrency::CDistributedActorTestKeySettings{})
				.f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality_None)
			)
		{
			fg_TestAddCleanupPath(mp_Settings.m_RootDirectory);
		}

		TCFuture<void> fp_StartApp(NEncoding::CEJSONSorted const &_Params) override
		{
			co_return {};
		}

		TCFuture<void> fp_StopApp() override
		{
			co_return {};
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
			co_return 5;
		}

		TCFuture<void> f_TestVoid()
		{
			co_return {};
		}
	};

	struct CTestReturn
	{
		CTestReturn()
		{
		}

		CTestReturn(CTestReturn const &)
		{
		}

		CTestReturn(CTestReturn &&)
		{
		}

		~CTestReturn()
		{
		}

		mint m_Test = 0;
	};

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
			if (m_Test)
			{
				m_Test = 0;
			}
		}

		mint m_Test = 20;
	};

	struct CTestActor : public CActor
	{
		TCFuture<CTestReturn> f_TestReturnInner()
		{
			CTestReturn Value;
			Value.m_Test = 5;

			co_return fg_Move(Value);
		}

		TCFuture<void> f_TestReturn()
		{
			CTestReturn Value = co_await f_TestReturnInner();

			co_return {};
		}

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
			auto Future = f_Test();
			auto Value = co_await fg_Move(Future);
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
			co_return fg_Move(Promise);
		}

		TCFuture<void> f_TestReturnVoidPromise()
		{
			co_await m_TestActor(&CTestActor2::f_Test);

			TCPromise<void> Promise;
			Promise.f_SetResult();
			co_return fg_Move(Promise);
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
			auto Cleanup = g_OnScopeExitActor / [this]
				{
					m_WaitForCleanupScopeExit.f_SetResult();
				}
			;

			uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
			co_return 5 + Value;
		}

		TCFuture<void> f_WaitCleanupScope()
		{
			return m_WaitForCleanupScopeExit.f_Future();
		}

		TCFuture<uint32> f_TestFutureWithErrorWrapped()
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			uint32 Value = *(co_await (f_TestExceptionAfterSuspend() % "Extra error").f_Wrap());
			co_return Value;
		}

		TCFuture<uint32> f_TestActorCallWithError()
		{
			uint32 Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error");
			co_return Value;
		}

		TCFuture<uint32> f_TestActorCallWithErrorWrapped()
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			auto Value = co_await (fg_ThisActor(this)(&CTestActor::f_TestExceptionAfterSuspend) % "Extra error").f_Wrap();
			co_return *Value;
		}

		TCFuture<uint32> f_TestActorCallPackWithError()
		{
			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error");

			auto &[Result0, Result1] = Results;
			co_return Result0 + Result1;
		}

		TCFuture<uint32> f_TestActorCallPackWithErrorWrapped()
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			auto Results = co_await (f_TestExceptionAfterSuspend() + f_TestExceptionAfterSuspend() ^ "Extra error").f_Wrap();

			auto &[Result0, Result1] = Results;
			co_return *Result0 + *Result1;
		}

		TCFuture<uint32> f_TestThrowException()
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			DMibError("Test Exception");

			co_return 5;
		}

		TCFuture<uint32> f_TestThrowExceptionCaptureScope()
		{
			{
				auto CaptureScope = co_await g_CaptureExceptions;
				DMibError("Test Exception");
			}

			co_return 5;
		}

		TCFuture<uint32> f_TestThrowExceptionCaptureScopeDouble()
		{
			auto CaptureScope = co_await g_CaptureExceptions;
			{
				auto CaptureScope = co_await g_CaptureExceptions;
				DMibError("Test Exception");
			}

			co_return 5;
		}

		TCFuture<uint32> f_TestThrowExceptionCaptureScopeOuterScope()
		{
			auto CaptureScope = co_await g_CaptureExceptions;
			DMibError("Test Exception");

			co_return 5;
		}

		TCFuture<void> f_TestThrowExceptionCaptureScopeNoException()
		{
			auto CaptureScope = co_await g_CaptureExceptions;

			co_return {};
		}

		TCFuture<void> f_TestThrowExceptionCaptureScopeNoExceptionCaught()
		{
			auto CaptureScope = co_await g_CaptureExceptions;
			try
			{
				DMibError("Test Exception");
			}
			catch (...)
			{
			}

			co_return {};
		}

		TCFuture<void> f_TestThrowExceptionCaptureScopeNoExceptionInsideTry()
		{
			try
			{
				auto CaptureScope = co_await g_CaptureExceptions;
				DMibError("Test Exception");
			}
			catch (...)
			{
			}

			co_return {};
		}

		TCFuture<uint32> f_TestThrowExceptionAfterSuspend()
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			NException::CDisableExceptionTraceScope DisableExceptionTrace;

			[[maybe_unused]] uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);

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
					self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
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
					self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						co_return co_await
							(
								self / [=]() -> TCFuture<uint32>
								{
									auto Cleanup = g_OnScopeExit / [&]
										{
											DMibFastCheck(TestDestruction.m_Test == 20);
										}
									;
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
					self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
						co_return Value + Value2;
					}
				)
			;

			co_return Value;
		}

		TCFuture<uint32> f_TestLambdaReferenceCorrectedDispatched()
		{
			uint32 Value = co_await
				(
					g_Dispatch / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
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
			return g_Future <<= self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
				{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnRecursive()
		{
			return g_Future <<= self / [=, Value2 = 20]() -> TCFuture<uint32>
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
			return g_Future <<= self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
			{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
					return g_Future <<= self / [=]() -> TCFuture<uint32>
						{
							auto Cleanup = g_OnScopeExit / [&]
								{
									DMibFastCheck(TestDestruction.m_Test == 20);
								}
							;
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

			self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
				{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
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

			self / [=, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
				{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
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
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
			f_TestParameterReference(RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroCorrected()
		{
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
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
			return g_Future <<= self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return f_TestParameterReference(RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParamsCorrected(uint32 const &_Param)
		{
			uint32 RefValue = 20;
			return g_Future <<= self(&CTestActor::f_TestParameterReference, RefValue);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatched()
		{
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
			f_TestParameterReference(RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatchedCorrected()
		{
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
			self(&CTestActor::f_TestParameterReference, RefValue) > Promise;
			return Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterNonReferenceRecursiveNoCoro()
		{
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
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
					self / [=, Value = fg_Move(_Value), TestDestruction = CTestDestruction{}]() mutable -> TCFuture<void>
						{
							auto Cleanup = g_OnScopeExit / [&]
								{
									DMibFastCheck(TestDestruction.m_Test == 20);
								}
							;
							if (!Value)
							{
								m_WaitForResultCallToFinish.f_SetException(Value.f_GetException());
								co_return {};
							}

							co_await m_TestActor(&CTestActor2::f_Test);
							m_WaitForResultCallToFinish.f_SetResult(fg_Move(Value));

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
			return m_WaitForResultCallToFinish.f_Future();
		}

		TCFuture<uint32> f_TestActorResultUnwrapped()
		{
			auto Result = co_await (co_await m_TestActor(&CTestActor2::f_Test).f_Wrap() | g_Unwrap);
			auto Result2 = co_await (co_await m_TestActor(&CTestActor2::f_Test).f_Wrap() | (g_Unwrap % "Test error"));

			co_return Result + Result2;
		}

		TCFuture<uint32> f_TestActorResultVoidUnwrapped()
		{
			co_await (co_await m_TestActor(&CTestActor2::f_TestVoid).f_Wrap() | g_Unwrap);
			co_await (co_await m_TestActor(&CTestActor2::f_TestVoid).f_Wrap() | (g_Unwrap % "Test error"));

			co_return 0;
		}

		TCFuture<uint32> f_TestActorResultVectorUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCActorResultVector<uint32> Results;
					for (mint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_Test) > Results.f_AddResult();
					return Results;
				}
			;

			uint32 Value = 0;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | g_Unwrap))
				Value += Result;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | (g_Unwrap % "Test error")))
				Value += Result;

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestActorResultVectorVoidUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCActorResultVector<void> Results;
					for (mint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_TestVoid) > Results.f_AddResult();
					return Results;
				}
			;

			co_await (co_await fResultContainer().f_GetResults() | g_Unwrap);
			co_await (co_await fResultContainer().f_GetResults() | (g_Unwrap % "Test error"));

			co_return _Value;
		}

		TCFuture<uint32> f_TestActorResultMapUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCActorResultMap<int, uint32> Results;
					for (mint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_Test) > Results.f_AddResult(i);
					return Results;
				}
			;

			uint32 Value = 0;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | g_Unwrap))
				Value += Result;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | (g_Unwrap % "Test error")))
				Value += Result;

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestActorResultMapVoidUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCActorResultMap<int, void> Results;
					for (mint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_TestVoid) > Results.f_AddResult(i);
					return Results;
				}
			;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | g_Unwrap))
				;

			for (auto &Result : co_await (co_await fResultContainer().f_GetResults() | (g_Unwrap % "Test error")))
				;

			co_return _Value;
		}

		TCFuture<void> f_TestCoroutineInResultCallUnobservedError(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]
				{
					return _Value * 2;
				}
				> [this](TCAsyncResult<uint32> _Value) mutable
				{
					self / [TestDestruction = CTestDestruction{}] () -> TCFuture<void>
						{
							auto Cleanup = g_OnScopeExit / [&]
								{
									DMibFastCheck(TestDestruction.m_Test == 20);
								}
							;
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

		TCFuture<void> f_TestOnResumeException(bool _bThrowInitial)
		{
			bool bThrow = _bThrowInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> NException::CExceptionPointer
					{
						if (bThrow)
							return DMibErrorInstance("Aborted");
						return {};
					}
				)
			;
			bThrow = !_bThrowInitial;
			co_await fg_Timeout(0.001);

			co_return {};
		}

		TCFuture<void> f_TestOnResumeExceptionNoSuspend(bool _bThrowInitial)
		{
			bool bThrow = _bThrowInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> NException::CExceptionPointer
					{
						if (bThrow)
							return DMibErrorInstance("Aborted");
						return {};
					}
				)
			;
			bThrow = !_bThrowInitial;
			TCPromise<void> Promise;
			Promise.f_SetResult();
			co_await Promise.f_Future();

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecific()
		{
			auto CaptureExceptions = co_await g_CaptureExceptions.f_Specific<CCustomException>();

			DMibCustomException("Test Custom");

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecificUserMessage()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions.f_Specific<CCustomException>() % (fg_LogError("Test", "Test") % "User Message Custom"));

			DMibCustomException("Test Custom");

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecific2()
		{
			auto CaptureExceptions = co_await g_CaptureExceptions.f_Specific<CCustomException, CCustomException2>();

			DMibCustomException2("Test Custom");

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecificInner()
		{
			auto CaptureExceptions = co_await g_CaptureExceptions;
			{
				auto CaptureExceptions = co_await g_CaptureExceptions.f_Specific<CCustomException, CCustomException2>();
				DMibCustomException2("Test Custom");
			}

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecificInnerNormal()
		{
			auto CaptureExceptions = co_await g_CaptureExceptions;
			{
				auto CaptureExceptions = co_await g_CaptureExceptions.f_Specific<CCustomException, CCustomException2>();
				DMibError("Test Normal");
			}

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecificOuter()
		{
			auto CaptureExceptions = co_await g_CaptureExceptions.f_Specific<CCustomException, CCustomException2>();
			{
				auto CaptureExceptions = co_await g_CaptureExceptions;
				DMibError("Test Normal");
			}

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogError()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % fg_LogError("Test", "Test"));
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessage()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageNested()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				DMibError("Test Log");
			}

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageLeaked()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			try
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				DMibError("Test Log");
			}
			catch (...)
			{
			}
			DMibError("Test Log2");

			co_return {};
		}

		TCFuture<void> f_TestCaptureSpecificAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions.f_Specific<CCustomException>() % (fg_LogError("Test", "Test") % "User Message Custom"));

			co_await fg_Timeout(0.001);
			DMibCustomException("Test Custom");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % fg_LogError("Test", "Test"));

			co_await fg_Timeout(0.001);
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));

			co_await fg_Timeout(0.001);
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageNestedAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				co_await fg_Timeout(0.001);
				DMibError("Test Log");
			}

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageLeakedAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			try
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				co_await fg_Timeout(0.001);
				DMibError("Test Log");
			}
			catch (...)
			{
			}
			DMibError("Test Log2");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageLeakedAfterSupendOuter()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			try
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				DMibError("Test Log");
			}
			catch (...)
			{
			}

			co_await fg_Timeout(0.001);
			DMibError("Test Log2");

			co_return {};
		}

		TCActor<CTestActor2> m_TestActor = fg_Construct();
		TCPromise<void> m_WaitForCleanupScopeExit;
		TCPromise<uint32> m_WaitForResultCallToFinish;
	};

	struct CTestDestroyActor : public CActor
	{
		TCFuture<void> f_TestDestroy(TCFuture<void> &&_ToWaitFor)
		{
			co_await fg_Move(_ToWaitFor);

			co_return {};
		}
	};

	struct CTestDestroyDelegatedActor : public CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		TCFuture<void> f_TestDestroy(TCFuture<void> &&_ToWaitFor)
		{
			co_await fg_Move(_ToWaitFor);

			co_return {};
		}
	};

	class CCoroutines_Tests : public NMib::NTest::CTest
	{
		void f_TestGeneral()
		{
			DMibTestSuite("General")
			{
				TCActor<CTestActor> TestActor(fg_Construct());

#if DMibEnableSafeCheck > 0
				NStr::CStr ReferenceThisError = "false 'Unsafe call to coroutine with reference this pointer'";
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReference).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceRecursive).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoro).f_CallSync(), ReferenceThisError);
#if DMibConcurrencySupportCoroutineFreeFunctionDebug
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturn).f_CallSync(), ReferenceThisError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDirectReturnRecursive).f_CallSync(), ReferenceThisError);
#endif
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestLambdaReferenceNoCoroDispatched).f_CallSync(), ReferenceThisError);

				NStr::CStr ReferenceParamError = "false 'Unsafe call to coroutine with reference parameters'";
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursive).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoro).f_CallSync(), ReferenceParamError);
#if DMibConcurrencySupportCoroutineFreeFunctionDebug
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturn).f_CallSync(), ReferenceParamError);
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams, 20).f_CallSync(), ReferenceParamError);
#endif
				DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestParameterReferenceRecursiveNoCoroDispatched).f_CallSync(), ReferenceParamError);
				DMibExpectException(TestActor(&CTestActor::f_TestParameterReferenceRecursivePack).f_CallSync(), DMibErrorInstanceExceptionVector(ReferenceParamError + " - x2", {})	);
#endif

				TestActor(&CTestActor::f_TestReturn).f_CallSync();

				DMibExpect(TestActor(&CTestActor::f_Test).f_CallSync(), ==, 40);
				DMibExpect(TestActor(&CTestActor::f_TestValue, 55).f_CallSync(), ==, 55);

				DMibExpect(TestActor(&CTestActor::f_TestForwardAsyncResult).f_CallSync(), ==, 40);
				TestActor(&CTestActor::f_TestForwardVoidAsyncResult).f_CallSync();


				DMibExpect(TestActor(&CTestActor::f_TestCleanup).f_CallSync(), ==, 10);
				TestActor(&CTestActor::f_WaitCleanupScope).f_CallSync();

				TestActor(&CTestActor::f_TestVoid).f_CallSync();

				DMibExpect(TestActor(&CTestActor::f_TestReturnPromise).f_CallSync(), ==, 5);
				TestActor(&CTestActor::f_TestReturnVoidPromise).f_CallSync();
				DMibExpect(TestActor(&CTestActor::f_TestFuture).f_CallSync(), ==, 45);

				DMibExpectException(TestActor(&CTestActor::f_TestException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionAfterSuspend).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedException).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResult).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(TestActor(&CTestActor::f_TestRecursiveWrappedExceptionAsyncResultVoid).f_CallSync(), DMibErrorInstance("Test Exception"));

				NException::CExceptionPointer pDummy;

				DMibExpectException(TestActor(&CTestActor::f_TestFutureWithError).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception", pDummy));
				DMibExpectException(TestActor(&CTestActor::f_TestFutureWithErrorWrapped).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception", pDummy));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithError).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception", pDummy));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallWithErrorWrapped).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception", pDummy));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithError).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception - x2", pDummy));
				DMibExpectException(TestActor(&CTestActor::f_TestActorCallPackWithErrorWrapped).f_CallSync(), DMibErrorInstanceWrapped("Extra error: Test Exception", pDummy));

				DMibExpect(TestActor(&CTestActor::f_TestParameterReference, 25).f_CallSync(), ==, 30);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceCorrected).f_CallSync(), ==, 25);
				DMibExpect(TestActor(&CTestActor::f_TestLambdaReferenceCorrectedDispatched).f_CallSync(), ==, 25);
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

				DMibExpect(TestActor(&CTestActor::f_TestActorResultVectorUnwrapped, 2).f_CallSync(), ==, 5 * 10 * 2 + 2);
				DMibExpect(TestActor(&CTestActor::f_TestActorResultMapUnwrapped, 2).f_CallSync(), ==, 5 * 10 * 2 + 2);
				DMibExpect(TestActor(&CTestActor::f_TestActorResultUnwrapped).f_CallSync(), ==, 5 * 2);

				DMibExpect(TestActor(&CTestActor::f_TestActorResultVectorVoidUnwrapped, 2).f_CallSync(), ==, 2);
				DMibExpect(TestActor(&CTestActor::f_TestActorResultMapVoidUnwrapped, 2).f_CallSync(), ==, 2);
				DMibExpect(TestActor(&CTestActor::f_TestActorResultVoidUnwrapped).f_CallSync(), ==, 0);

				TestActor(&CTestActor::f_TestCoroutineInResultCallUnobservedError, 2).f_CallSync();
			};
		}

		void f_TestAuditor()
		{
			DMibTestSuite("Auditor")
			{
				auto AppActor = fg_ConstructActor<CTestDistributedApp>("TestDistAppAuditor");
				AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJSONSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess).f_CallSync();

				DMibExpect(AppActor(&CTestDistributedApp::f_Test).f_CallSync(), ==, 32);
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithAuditor).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithErrorWithAuditorWith).f_CallSync(), DMibErrorInstance("Extra error: Test Exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithErrorWithAuditorWithError).f_CallSync(), DMibErrorInstance("Auditor exception"));

				AppActor->f_BlockDestroy();
			};
		}

		void f_TestDestroy()
		{
			DMibTestSuite("Destroy")
			{
				TCFuture<void> DestroyFuture;
				{
					TCPromise<void> DestroyPromise;
					{
						TCActor<CTestDestroyActor> TestDestroyActor = fg_Construct();

						DestroyFuture = g_Future <<= TestDestroyActor(&CTestDestroyActor::f_TestDestroy, DestroyPromise.f_Future());
						TestDestroyActor.f_Destroy().f_CallSync();
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorResultWasNotSet, "Result was not set"));
			};
			DMibTestSuite("Destroy Delegated")
			{
				TCFuture<void> DestroyFuture;
				{
					TCPromise<void> DestroyPromise;
					{
						TCActor<CActor> MainActor = fg_Construct();
						{
							TCActor<CTestDestroyDelegatedActor> TestDestroyActor{fg_Construct(), MainActor};

							DestroyFuture = g_Future <<= TestDestroyActor(&CTestDestroyDelegatedActor::f_TestDestroy, DestroyPromise.f_Future());
							TestDestroyActor.f_Destroy().f_CallSync();
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorResultWasNotSet, "Result was not set"));
			};
			DMibTestSuite("Destroy Delegated 2")
			{
				TCFuture<void> DestroyFuture;
				{
					TCPromise<void> DestroyPromise;
					{
						TCActor<CActor> MainActor = fg_Construct();
						{
							TCActor<CTestDestroyDelegatedActor> TestDestroyActor{fg_Construct(), MainActor};

							DestroyFuture = g_Future <<= TestDestroyActor(&CTestDestroyDelegatedActor::f_TestDestroy, DestroyPromise.f_Future());
							MainActor.f_Destroy().f_CallSync();
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorResultWasNotSet, "Result was not set"));
			};
			DMibTestSuite("Destroy Delegated 3")
			{
				TCFuture<void> DestroyFuture;
				{
					TCPromise<void> DestroyPromise;
					{
						TCActor<CActor> MainActor = fg_Construct();
						{
							TCActor<CTestDestroyDelegatedActor> Delegated{fg_Construct(), MainActor};
							{
								TCActor<CTestDestroyDelegatedActor> TestDestroyActor{fg_Construct(), Delegated};

								DestroyFuture = g_Future <<= TestDestroyActor(&CTestDestroyDelegatedActor::f_TestDestroy, DestroyPromise.f_Future());
								MainActor.f_Destroy().f_CallSync();
							}
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorResultWasNotSet, "Result was not set"));
			};
		}

		struct CAsyncDestroyTestState
		{
			NAtomic::TCAtomic<int32> m_Sequence = 0;
			int32 m_ExitSequence = -1;
			int32 m_DestroyCalledSequence = -1;

			NAtomic::TCAtomic<int32> m_nDestructed = 0;
			NAtomic::TCAtomic<int32> m_nDestroyed = 0;
			NAtomic::TCAtomic<int32> m_nStartDestroy = 0;
		};

		struct CToAsyncDestroy : public CAllowUnsafeThis
		{
			CToAsyncDestroy(TCSharedPointer<CAsyncDestroyTestState> const &_pTestState)
				: m_pTestState(_pTestState)
			{
			}
			CToAsyncDestroy(CToAsyncDestroy &&) = default;

			~CToAsyncDestroy()
			{
				if (m_pTestState)
					++m_pTestState->m_nDestructed;
			}

			TCFuture<void> f_Destroy()
			{
				++m_pTestState->m_nStartDestroy;
				co_await fg_Timeout(0.001);
				++m_pTestState->m_nDestroyed;
				co_return {};
			}

			TCSharedPointer<CAsyncDestroyTestState> m_pTestState;
		};

		struct CAsyncDestroyActor : public CActor
		{
			CAsyncDestroyActor(TCSharedPointer<CAsyncDestroyTestState> const &_pTestState)
				: m_pTestState(_pTestState)
			{
			}

			~CAsyncDestroyActor()
			{
				++m_pTestState->m_nDestructed;
			}

			TCFuture<void> fp_Destroy()
			{
				++m_pTestState->m_nStartDestroy;
				co_await fg_Timeout(0.001);
				++m_pTestState->m_nDestroyed;
				co_return {};
			}

			TCSharedPointer<CAsyncDestroyTestState> m_pTestState;
		};

		void f_TestAsyncDestroy()
		{
			DMibTestSuite("Async Destroy")
			{
				DMibTestCategory("Sequence") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&, pTestState]() -> TCFuture<void>
										{
											auto ToDestroy = fg_Move(Variable);
											pTestState->m_DestroyCalledSequence = pTestState->m_Sequence.f_FetchAdd(1);

											co_await ToDestroy.f_Destroy();

											co_return {};
										}
									)
								;

								co_await fg_Timeout(0.001);

								pTestState->m_ExitSequence = pTestState->m_Sequence.f_FetchAdd(1);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						)
					;

					DMibExpect(pTestState->m_ExitSequence, ==, 0);
					DMibExpect(pTestState->m_DestroyCalledSequence, ==, 1);
					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};
				DMibTestCategory("Actor") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_Timeout(0.001);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						)
					;

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Object") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_Timeout(0.001);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						)
					;

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_Timeout(0.001);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								DMibError("Test");

								co_return {};
							}
						)
						.f_Wrap()
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Awaited Exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&, pTestState]() -> TCFuture<void>
										{
											auto ToDestroy = fg_Move(Variable);
											co_await fg_Timeout(0.001);
											co_await ToDestroy.f_Destroy();
											co_return {};
										}
									)
								;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_await fg_CallSafe
									(
										[]() -> TCFuture<void>
										{
											co_return DMibErrorInstance("Test");
										}
									)
								;
								co_return {};
							}
						)
						.f_Wrap()
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Resume Exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&, pTestState]() -> TCFuture<void>
										{
											auto ToDestroy = fg_Move(Variable);
											co_await fg_Timeout(0.001);
											co_await ToDestroy.f_Destroy();
											co_return {};
										}
									)
								;

								bool bThrowResume = false;

								auto OnResume = co_await fg_OnResume
									(
										[&]() -> NException::CExceptionPointer
										{
											if (bThrowResume)
												return DMibErrorInstance("Test");

											return {};
										}
									)
								;

								bThrowResume = true;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_await g_Yield;

								co_return {};
							}
						)
						.f_Wrap()
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Initial Resume Exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&, pTestState]() -> TCFuture<void>
										{
											auto ToDestroy = fg_Move(Variable);
											co_await fg_Timeout(0.001);
											co_await ToDestroy.f_Destroy();
											co_return {};
										}
									)
								;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								auto OnResume = co_await fg_OnResume
									(
										[&]() -> NException::CExceptionPointer
										{
											return DMibErrorInstance("Test");

											return {};
										}
									)
								;

								co_return {};
							}
						)
						.f_Wrap()
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					co_return {};
				};
				DMibTestCategory("Explicit") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_Timeout(0.001);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_await fg_Move(AsyncDestroy);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

								co_return {};
							}
						)
					;

					{
						DMibTestPath("After");
						DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
						DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					}
					co_return {};
				};
				DMibTestCategory("For Loop") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								for (mint i = 0; i < 10; ++i)
								{
									CToAsyncDestroy Variable(pTestState);
									auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);
								}

								DMibExpect(pTestState->m_nStartDestroy.f_Load(), ==, 10);
								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						)
					;

					{
						DMibTestPath("After");
						DMibExpect(pTestState->m_nStartDestroy.f_Load(), ==, 10);
						DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 10);
						DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 10);
					}

					co_return {};
				};
				DMibTestCategory("For Loop Wait") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								for (mint i = 0; i < 10; ++i)
								{
									{
										CToAsyncDestroy Variable(pTestState);
										auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);
									}
									co_await g_AsyncDestroy;
								}

								DMibExpect(pTestState->m_nStartDestroy.f_Load(), ==, 10);
								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 10);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 10);

								co_return {};
							}
						)
					;

					{
						DMibTestPath("After");
						DMibExpect(pTestState->m_nStartDestroy.f_Load(), ==, 10);
						DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 10);
						DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 10);
					}

					co_return {};
				};
				DMibTestCategory("Destroy exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								CToAsyncDestroy Variable(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&, pTestState]() -> TCFuture<void>
										{
											auto ToDestroy = fg_Move(Variable);

											co_await ToDestroy.f_Destroy();

											co_return DMibErrorInstance("Test");
										}
									)
								;

								co_await fg_Timeout(0.001);

								pTestState->m_ExitSequence = pTestState->m_Sequence.f_FetchAdd(1);

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						).f_Wrap()
					;

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					co_return {};
				};
				DMibTestCategory("Destroy exception direct") -> TCFuture<void>
				{
					auto Result = co_await fg_CallSafe
						(
							[]() -> TCFuture<void>
							{
								auto AsyncDestroy = co_await fg_AsyncDestroy
									(
										[&]() -> TCFuture<void>
										{
											co_return DMibErrorInstance("Test");
										}
									)
								;

								co_await fg_Timeout(0.001);

								co_return {};
							}
						).f_Wrap()
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstance("Test"));

					co_return {};
				};
#if DMibEnableSafeCheck > 0
				DMibTestCategory("Require co_await") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureExceptions;

								CToAsyncDestroy Variable(pTestState);
								auto AsyncDestroy = fg_AsyncDestroy(Variable);

								co_return {};
							}
						)
						.f_Wrap();
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstanceSafeCheck("fg_Exchange(mp_bAwaited, true) 'You forgot to co_await the async destroy'"));
					co_return {};
				};
				DMibTestCategory("Require co_await double exception") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Result = co_await fg_CallSafe
						(
							[pTestState]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureExceptions;

								CToAsyncDestroy Variable(pTestState);
								auto AsyncDestroy = fg_AsyncDestroy(Variable);

								co_return DMibErrorInstance("Test");

								co_return {};
							}
						)
						.f_Wrap();
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstanceExceptionVector("Test\nfg_Exchange(mp_bAwaited, true) 'You forgot to co_await the async destroy'", {}));
					co_return {};
				};
#endif
			};
		}

		void f_TestOnResume()
		{
			DMibTestSuite("OnResume")
			{
				DMibTestCategory("General")
				{
					TCActor<CTestActor> TestActor(fg_Construct());

					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeException, false).f_CallSync(), DMibErrorInstance("Aborted"));
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeException, true).f_CallSync(), DMibErrorInstance("Aborted"));

					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionNoSuspend, false).f_CallSync());
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionNoSuspend, true).f_CallSync(), DMibErrorInstance("Aborted"));
				};
#if DMibEnableSafeCheck > 0
				DMibTestCategory("Require co_await") -> TCFuture<void>
				{
					auto Result = co_await fg_CallSafe
						(
							[]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureExceptions;

								auto OnResume = fg_OnResume
									(
										[&]() -> NException::CExceptionPointer
										{
											return {};
										}
									)
								;

								co_return {};
							}
						)
						.f_Wrap();
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstanceSafeCheck("fg_Exchange(mp_bAwaited, true) 'You forgot to co_await the on resume'"));
					co_return {};
				};
				DMibTestCategory("Require co_await double exception") -> TCFuture<void>
				{
					auto Result = co_await fg_CallSafe
						(
							[]() -> TCFuture<void>
							{
								co_await ECoroutineFlag_CaptureExceptions;

								auto OnResume = fg_OnResume
									(
										[&]() -> NException::CExceptionPointer
										{
											return {};
										}
									)
								;

								co_return DMibErrorInstance("Test");

								co_return {};
							}
						)
						.f_Wrap();
					;

					DMibExpectException(Result.f_Access(), DMibErrorInstanceExceptionVector("Test\nfg_Exchange(mp_bAwaited, true) 'You forgot to co_await the on resume'", {}));
					co_return {};
				};
#endif
			};
		}

		void f_TestCapture()
		{
			DMibTestSuite("Capture")
			{
				DMibTestCategory("General")
				{
					NStr::CStr CaptureInsideTryCatchError = "false 'It's not supported to capture exceptions inside a try/catch statement'";

					TCActor<CTestActor> TestActor(fg_Construct());

					DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionCaptureScope).f_CallSync(), DMibErrorInstance("Test Exception"));
					DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionCaptureScopeDouble).f_CallSync(), DMibErrorInstance("Test Exception"));
					DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionCaptureScopeOuterScope).f_CallSync(), DMibErrorInstance("Test Exception"));
					DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionCaptureScopeNoException).f_CallSync());
					DMibExpectException(TestActor(&CTestActor::f_TestThrowExceptionCaptureScopeNoExceptionCaught).f_CallSync());

					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecific).f_CallSync(), DMibCustomExceptionInstance("Test Custom"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecificUserMessage).f_CallSync(), DMibErrorInstance("User Message Custom"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecific2).f_CallSync(), DMibCustomException2Instance("Test Custom"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecificInner).f_CallSync(), DMibCustomException2Instance("Test Custom"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecificInnerNormal).f_CallSync(), DMibErrorInstance("Test Normal"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecificOuter).f_CallSync(), DMibErrorInstance("Test Normal"));

					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogError).f_CallSync(), DMibErrorInstance("Test Log"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessage).f_CallSync(), DMibErrorInstance("User Message"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageNested).f_CallSync(), DMibErrorInstance("User Message Nested"));

					DMibExpectException(TestActor(&CTestActor::f_TestCaptureSpecificAfterSupend).f_CallSync(), DMibErrorInstance("User Message Custom"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogErrorAfterSupend).f_CallSync(), DMibErrorInstance("Test Log"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageAfterSupend).f_CallSync(), DMibErrorInstance("User Message"));
					DMibExpectException(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageNestedAfterSupend).f_CallSync(), DMibErrorInstance("User Message Nested"));
#if DMibEnableSafeCheck > 0
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestThrowExceptionCaptureScopeNoExceptionInsideTry).f_CallSync(), CaptureInsideTryCatchError);
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageLeaked).f_CallSync(), CaptureInsideTryCatchError)(ETest_ExpectFail);
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageLeakedAfterSupend).f_CallSync(), CaptureInsideTryCatchError)(ETest_ExpectFail);
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestCaptureLogErrorUserMessageLeakedAfterSupendOuter).f_CallSync(), CaptureInsideTryCatchError);
#endif
				};
#ifndef DCompiler_MSVC_Workaround
				// throwing from unhandled_exception is not supported
				DMibTestCategory("NotCaught")
				{
					TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
					auto CleanupRunLoop = g_OnScopeExit / [&]
						{
							while (pRunLoop->m_RefCount.f_Get() > 0)
								pRunLoop->f_WaitOnceTimeout(0.1);
						}
					;
					TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
					auto CleanupHelperActor = g_OnScopeExit / [&]
						{
							HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
						}
					;
					CCurrentlyProcessingActorScope CurrentActor{HelperActor};

					auto fTest = [&]() -> TCFuture<void>
						{
							auto CaptureExceptions = co_await (g_CaptureExceptions.f_Specific<CCustomException>() % (fg_LogError("Test", "Test") % "User Message Custom"));
							DMibError("Test Custom");
							co_return {};
						}
					;
					DMibExpectException(fTest().f_CallSync(pRunLoop), DMibErrorInstance("Test Custom"));
				};
#endif
			};
		}

		void f_DoTests()
		{
#if DMibEnableSafeCheck > 0
			auto bDebugCoroutineSafeCheck = CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck;
			CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck = true;
			auto Cleanup = g_OnScopeExit / [&]
				{
					CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck = bDebugCoroutineSafeCheck;
				}
			;
#endif
			f_TestGeneral();
			f_TestAuditor();
			f_TestDestroy();
			f_TestAsyncDestroy();
			f_TestOnResume();
			f_TestCapture();
		}
	};

	DMibTestRegister(CCoroutines_Tests, Malterlib::Concurrency);
}
