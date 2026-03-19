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
#include <Mib/Encoding/JsonShortcuts>

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
	using namespace NMib::NStr;

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

		TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) override
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

	struct CTestClass
	{
	};

	class CTestActor2 : public CActor
	{
	public:

		using CActorHolder = CDelegatedActorHolder;

		TCFuture<uint32> f_Test()
		{
			co_return 5;
		}

		TCFuture<void> f_TestVoid()
		{
			co_return {};
		}

		static TCFuture<void> fs_TestParams(CTestClass *_pTest, int32 _Value)
		{
			co_await g_Yield;
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

		umint m_Test = 0;
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

		umint m_Test = 20;
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
			co_return co_await m_WaitForCleanupScopeExit.f_Future();
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
			TCFutureVector<uint32> AsyncResults;

			m_TestActor(&CTestActor2::f_Test) > AsyncResults;
			m_TestActor(&CTestActor2::f_Test) > AsyncResults;

			uint32 Value = 0;

			for (auto &Result : co_await fg_AllDoneWrapped(AsyncResults))
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
					self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						co_return co_await [=, this]() -> TCFuture<uint32>
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
					self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						co_return co_await
							(
								self / [=, this]() -> TCFuture<uint32>
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
			uint32 Value = co_await [=, this, Value2 = 20]() -> TCFuture<uint32>
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
					self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
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
					g_Dispatch / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
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
			co_return co_await [=, this, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				()
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnCorrected()
		{
			co_return co_await
				(
					self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
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

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnRecursive()
		{
			co_return co_await
				(
					self / [=, this, Value2 = 20]() -> TCFuture<uint32>
					{
						co_return co_await
							(
								[=, this]() -> TCFuture<uint32>
								{
									uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
									co_return Value + Value2;
								}
								()
							)
						;
					}
				)
			;
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDirectReturnRecursiveCorrected()
		{
			co_return co_await
				(
					self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
					{
						auto Cleanup = g_OnScopeExit / [&]
							{
								DMibFastCheck(TestDestruction.m_Test == 20);
							}
						;
						co_return co_await
							(
								self / [=, this]() -> TCFuture<uint32>
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
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDispatched()
		{
			TCPromise<uint32> Promise;

			[=, this, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				() > fg_TempCopy(Promise)
			;

			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroDispatchedCorrected()
		{
			TCPromise<uint32> Promise;

			self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
				{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				> fg_TempCopy(Promise)
			;

			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoro()
		{
			TCPromise<uint32> Promise;

			[=, this, Value2 = 20]() -> TCFuture<uint32>
				{
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				() > fg_TempCopy(Promise)
			;

			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestLambdaReferenceNoCoroCorrected()
		{
			TCPromise<uint32> Promise;

			self / [=, this, Value2 = 20, TestDestruction = CTestDestruction{}]() -> TCFuture<uint32>
				{
					auto Cleanup = g_OnScopeExit / [&]
						{
							DMibFastCheck(TestDestruction.m_Test == 20);
						}
					;
					uint32 Value = co_await m_TestActor(&CTestActor2::f_Test);
					co_return Value + Value2;
				}
				> fg_TempCopy(Promise)
			;

			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReference(uint32 _Value)
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
			uint32 Value = co_await f_TestParameterReference(RefValue);

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
			auto [Value0, Value1] = co_await (f_TestParameterReference(RefValue) + f_TestParameterReference(RefValue));

			co_return Value0 + Value1;
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoro()
		{
			TCPromise<uint32> Promise;
			uint32 RefValue = 20;
			f_TestParameterReference(RefValue) > fg_TempCopy(Promise);
			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroCorrected()
		{
			TCPromise<uint32> Promise;
			{
				uint32 RefValue = 20;
				f_TestParameterReference(RefValue) > fg_TempCopy(Promise);
			}
			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturn()
		{
			TCFuture<uint32> Future;
			{
				uint32 RefValue = 20;
				Future = f_TestParameterReference(RefValue);
			}
			co_return co_await fg_Move(Future);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnCorrected()
		{
			TCFuture<uint32> Future;
			{
				uint32 RefValue = 20;
				Future = f_TestParameterReference(RefValue);
			}
			co_return co_await fg_Move(Future);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParams(uint32 _Param)
		{
			TCFuture<uint32> Future;
			{
				uint32 RefValue = 20;
				Future = f_TestParameterReference(RefValue);
			}
			co_return co_await fg_Move(Future);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDirectReturnSameParamsCorrected(uint32 _Param)
		{
			TCFuture<uint32> Future;
			{
				uint32 RefValue = 20;
				Future = f_TestParameterReference(RefValue);
			}
			co_return co_await fg_Move(Future);
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatched()
		{
			TCPromise<uint32> Promise;
			{
				uint32 RefValue = 20;
				f_TestParameterReference(RefValue) > fg_TempCopy(Promise);
			}
			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterReferenceRecursiveNoCoroDispatchedCorrected()
		{
			TCPromise<uint32> Promise;
			{
				uint32 RefValue = 20;
				f_TestParameterReference(RefValue) > fg_TempCopy(Promise);
			}
			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterNonReferenceRecursiveNoCoro()
		{
			TCPromise<uint32> Promise;
			{
				uint32 RefValue = 20;
				f_TestParameter(RefValue) > fg_TempCopy(Promise);
			}
			co_return co_await Promise.f_MoveFuture();
		}

		TCFuture<uint32> f_TestParameterNonReferenceRecursiveNoCoroDirectReturn()
		{
			uint32 RefValue = 20;
			co_return co_await f_TestParameter(RefValue);
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
					self / [=, this, Value = fg_Move(_Value), TestDestruction = CTestDestruction{}]() mutable -> TCFuture<void>
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
						> g_DiscardResult
					;
				}
			;

			co_return {};
		}

		TCFuture<uint32> f_WaitCleanupResultCall()
		{
			co_return co_await m_WaitForResultCallToFinish.f_Future();
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
					TCFutureVector<uint32> Results;
					for (umint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_Test) > Results;
					return Results;
				}
			;

			uint32 Value = 0;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | g_Unwrap))
				Value += Result;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | (g_Unwrap % "Test error")))
				Value += Result;

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestActorResultVectorVoidUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCFutureVector<void> Results;
					for (umint i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_TestVoid) > Results;
					return Results;
				}
			;

			co_await (co_await fg_AllDoneWrapped(fResultContainer()) | g_Unwrap);
			co_await (co_await fg_AllDoneWrapped(fResultContainer()) | (g_Unwrap % "Test error"));

			co_return _Value;
		}

		TCFuture<uint32> f_TestActorResultMapUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCFutureMap<int, uint32> Results;
					for (int i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_Test) > Results[i];
					return Results;
				}
			;

			uint32 Value = 0;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | g_Unwrap))
				Value += Result;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | (g_Unwrap % "Test error")))
				Value += Result;

			co_return Value + _Value;
		}

		TCFuture<uint32> f_TestActorResultMapVoidUnwrapped(uint32 _Value)
		{
			auto fResultContainer = [&]
				{
					TCFutureMap<int, void> Results;
					for (int i = 0; i < 10; ++i)
						m_TestActor(&CTestActor2::f_TestVoid) > Results[i];
					return Results;
				}
			;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | g_Unwrap))
				;

			for (auto &Result : co_await (co_await fg_AllDoneWrapped(fResultContainer()) | (g_Unwrap % "Test error")))
				;

			co_return _Value;
		}

		TCFuture<void> f_TestCoroutineInResultCallUnobservedError(uint32 _Value)
		{
			g_ConcurrentDispatch / [_Value]() -> TCFuture<uint32>
				{
					co_return _Value * 2;
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
			co_await g_Yield;

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

		// Tests for fg_OnResume returning TCAsyncResult<T>
		TCFuture<int32> f_TestOnResumeValue(bool _bSetValueInitial)
		{
			bool bSetValue = _bSetValueInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bSetValue)
							Result.f_SetResult(42);
						return Result;
					}
				)
			;
			bSetValue = !_bSetValueInitial;
			co_await g_Yield;

			co_return 99;
		}

		TCFuture<int32> f_TestOnResumeValueNoSuspend(bool _bSetValueInitial)
		{
			bool bSetValue = _bSetValueInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bSetValue)
							Result.f_SetResult(42);
						return Result;
					}
				)
			;
			bSetValue = !_bSetValueInitial;
			TCPromise<void> Promise;
			Promise.f_SetResult();
			co_await Promise.f_Future();

			co_return 99;
		}

		TCFuture<int32> f_TestOnResumeResultException(bool _bSetExceptionInitial)
		{
			bool bSetException = _bSetExceptionInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bSetException)
							Result.f_SetException(DMibErrorInstance("OnResumeException"));
						return Result;
					}
				)
			;
			bSetException = !_bSetExceptionInitial;
			co_await g_Yield;

			co_return 99;
		}

		TCFuture<void> f_TestOnResumeVoidValue(bool _bSetValueInitial)
		{
			bool bSetValue = _bSetValueInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<void>
					{
						TCAsyncResult<void> Result;
						if (bSetValue)
							Result.f_SetResult();
						return Result;
					}
				)
			;
			bSetValue = !_bSetValueInitial;
			co_await g_Yield;

			co_return {};
		}

		TCFuture<void> f_TestOnResumeVoidException(bool _bSetExceptionInitial)
		{
			bool bSetException = _bSetExceptionInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<void>
					{
						TCAsyncResult<void> Result;
						if (bSetException)
							Result.f_SetException(DMibErrorInstance("OnResumeVoidException"));
						return Result;
					}
				)
			;
			bSetException = !_bSetExceptionInitial;
			co_await g_Yield;

			co_return {};
		}

		// Tests for fg_OnResume with g_CaptureExceptions
		TCFuture<int32> f_TestOnResumeValueWithCaptureExceptions(bool _bSetValueInitial)
		{
			auto CaptureScope = co_await g_CaptureExceptions;

			bool bSetValue = _bSetValueInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bSetValue)
							Result.f_SetResult(42);
						return Result;
					}
				)
			;
			bSetValue = !_bSetValueInitial;
			co_await g_Yield;

			co_return 99;
		}

		TCFuture<int32> f_TestOnResumeExceptionWithCaptureExceptions(bool _bSetExceptionInitial)
		{
			auto CaptureScope = co_await g_CaptureExceptions;

			bool bSetException = _bSetExceptionInitial;
			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bSetException)
							Result.f_SetException(DMibErrorInstance("OnResumeWithCapture"));
						return Result;
					}
				)
			;
			bSetException = !_bSetExceptionInitial;
			co_await g_Yield;

			co_return 99;
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

			co_await g_Yield;
			DMibCustomException("Test Custom");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % fg_LogError("Test", "Test"));

			co_await g_Yield;
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));

			co_await g_Yield;
			DMibError("Test Log");

			co_return {};
		}

		TCFuture<void> f_TestCaptureLogErrorUserMessageNestedAfterSupend()
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				co_await g_Yield;
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
				co_await g_Yield;
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

			co_await g_Yield;
			DMibError("Test Log2");

			co_return {};
		}

		// Test: fg_OnResume returning value with g_CaptureExceptions inside try/catch
		// Without outer capture scope
		TCFuture<int32> f_TestOnResumeValueWithCaptureInsideTryCatch(bool _bReturnValueInitially)
		{
			bool bReturnValue = _bReturnValueInitially;
			try
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				DMibError("Test Log");
			}
			catch (...)
			{
			}

			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bReturnValue)
							Result.f_SetResult(42);
						return Result;
					}
				)
			;
			bReturnValue = !_bReturnValueInitially;
			co_await g_Yield;

			co_return 99;
		}

		// Test: fg_OnResume returning value with g_CaptureExceptions inside try/catch
		// With outer capture scope
		TCFuture<int32> f_TestOnResumeValueWithCaptureInsideTryCatchOuter(bool _bReturnValueInitially)
		{
			auto CaptureExceptions = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message"));
			bool bReturnValue = _bReturnValueInitially;
			try
			{
				auto CaptureExceptionsNested = co_await (g_CaptureExceptions % (fg_LogError("Test", "Test") % "User Message Nested"));
				DMibError("Test Log");
			}
			catch (...)
			{
			}

			auto OnResume = co_await fg_OnResume
				(
					[&]() -> TCAsyncResult<int32>
					{
						TCAsyncResult<int32> Result;
						if (bReturnValue)
							Result.f_SetResult(42);
						return Result;
					}
				)
			;
			bReturnValue = !_bReturnValueInitially;
			co_await g_Yield;

			co_return 99;
		}

		TCFuture<void> fp_Destroy() override
		{
			co_await fg_Move(m_TestActor).f_Destroy();

			co_return {};
		}

		TCActor<CTestActor2> m_TestActor{fg_Construct(), fg_ThisActor(this)};
		TCPromise<void> m_WaitForCleanupScopeExit;
		TCPromise<uint32> m_WaitForResultCallToFinish;
	};

	struct CTestDestroyActor : public CActor
	{
		TCFuture<void> f_TestDestroy(TCFuture<void> _ToWaitFor)
		{
			co_await fg_Move(_ToWaitFor);

			co_return {};
		}
	};

	struct CTestDestroyDelegatedActor : public CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		TCFuture<void> f_TestDestroy(TCFuture<void> _ToWaitFor)
		{
			co_await fg_Move(_ToWaitFor);

			co_return {};
		}
	};

	class CBaseActor : public CActor
	{
	public:
		virtual uint32 f_GetValue()
		{
			return 1;
		}

		virtual uint32 f_GetSpecificValue(uint32 _Value)
		{
			return 0;
		}

		virtual TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> _pValue) = 0;
		virtual TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> _pValue) = 0;
	};

	class CDerivedActor : public CBaseActor
	{
	public:
		virtual uint32 f_GetValue() override
		{
			return 2;
		}

		virtual uint32 f_GetSpecificValue(uint32 _Value) override
		{
			return _Value;
		}

		TCFuture<uint32> f_GetSpecificValueFuture(uint32 _Value)
		{
			co_return _Value;
		}

		TCFuture<uint32> f_GetSpecificValueCoro(uint32 _Value)
		{
			co_return _Value;
		}

		virtual TCFuture<void> fp_Destroy() override
		{
			co_return {};
		}

		TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> _pValue) override
		{
			co_return _pValue->f_ToInt();
		}

		TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> _pValue) override
		{
			co_return _pValue->f_ToInt();
		}

		umint m_DummyMember0 = 0;
		umint m_DummyMember1 = 1;
	};

	class CCoroutines_Tests : public NMib::NTest::CTest
	{
		TCFuture<void> fp_BlockOnAllThreads()
		{
			umint nThreads = NSys::fg_Thread_GetVirtualCores();

			TCFutureVector<void> Results;

			for (umint i = 0; i < nThreads; ++i)
				fg_ConcurrentDispatch([]{}) > Results;

			co_await fg_AllDone(Results);

			co_return {};
		}

		void f_TestGeneral()
		{
			DMibTestSuite("General")
			{
				TCActor<CTestActor> TestActor(fg_Construct());
				auto Destroy = g_OnScopeExit / [&]
					{
						fg_Move(TestActor).f_Destroy().f_CallSync();
					}
				;

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
				AppActor(&CDistributedAppActor::f_StartApp, NEncoding::CEJsonSorted{}, TCActor<CDistiributedAppLogActor>{}, EDistributedAppType_InProcess).f_CallSync();

				DMibExpect(AppActor(&CTestDistributedApp::f_Test).f_CallSync(), ==, 32);
				DMibExpectException(AppActor(&CTestDistributedApp::f_TestFutureWithAuditor).f_CallSync(), DMibErrorInstance("Test Exception"));
				DMibExpectException
					(
						AppActor(&CTestDistributedApp::f_TestFutureWithErrorWithAuditorWith).f_CallSync()
						, DMibErrorInstanceWrapped("Extra error: Test Exception", DMibErrorInstance("Test Exception").f_ExceptionPointer())
					)
				;
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

						DestroyFuture = TestDestroyActor(&CTestDestroyActor::f_TestDestroy, DestroyPromise.f_Future());
						fg_Move(TestDestroyActor).f_Destroy().f_CallSync();
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));
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

							DestroyFuture = TestDestroyActor(&CTestDestroyDelegatedActor::f_TestDestroy, DestroyPromise.f_Future());
							fg_Move(TestDestroyActor).f_Destroy().f_CallSync();
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));
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

							DestroyFuture = TestDestroyActor.f_Bind<&CTestDestroyDelegatedActor::f_TestDestroy>(DestroyPromise.f_Future());
							fg_Move(MainActor).f_Destroy().f_CallSync();
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));
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

								DestroyFuture = TestDestroyActor(&CTestDestroyDelegatedActor::f_TestDestroy, DestroyPromise.f_Future());
								fg_Move(MainActor).f_Destroy().f_CallSync();
							}
						}
					}
					DestroyPromise.f_SetResult();
				}

				DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));
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
				co_await g_Yield;
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

			TCFuture<void> fp_Destroy() override
			{
				++m_pTestState->m_nStartDestroy;
				co_await g_Yield;
				++m_pTestState->m_nDestroyed;
				co_return {};
			}

			TCFuture<void> f_TestDestroy(TCFuture<void> _ToWaitFor)
			{
				TCActor<CAsyncDestroyActor> Variable = fg_Construct(m_pTestState);

				auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

				co_await fg_Move(_ToWaitFor);

				co_return {};
			}

			TCFuture<void> f_TestDestroyOrder(TCFuture<void> _ToWaitFor0, TCFuture<void> _ToWaitFor1, TCPromise<void> _DestroyStartedPromise)
			{
				auto AsyncDestroy = co_await fg_AsyncDestroy
					(
						[ToWaitFor1 = fg_Move(_ToWaitFor1), DestroyStartedPromise = fg_Move(_DestroyStartedPromise)]() mutable -> TCFuture<void>
						{
							DestroyStartedPromise.f_SetResult();
							co_await fg_Move(ToWaitFor1);
							co_return {};
						}
					)
				;

				co_await fg_Move(_ToWaitFor0);

				co_return {};
			}

			TCFuture<void> f_TestDestroyAtOnce(TCFuture<void> _ToWaitFor0, TCPromise<void> _DestroyStartedPromise)
			{
				auto AsyncDestroy = co_await fg_AsyncDestroy
					(
						[ToWaitFor0 = fg_Move(_ToWaitFor0), DestroyStartedPromise = fg_Move(_DestroyStartedPromise)]() mutable -> TCFuture<void>
						{
							DestroyStartedPromise.f_SetResult();
							co_await fg_Move(ToWaitFor0);
							co_return {};
						}
					)
				;

				co_return {};
			}

			TCFuture<void> f_TestDestroyAtOnceError(TCPromise<void> _DestroyStartedPromise)
			{
				auto AsyncDestroy = co_await fg_AsyncDestroy
					(
						[DestroyStartedPromise = fg_Move(_DestroyStartedPromise)]() mutable -> TCFuture<void>
						{
							DestroyStartedPromise.f_SetResult();

							co_return DMibErrorInstance("Testing");
						}
					)
				;

				co_return {};
			}

			TCFuture<void> f_TestDestroyAtOnceNoDestory(TCPromise<void> _DestroyStartedPromise)
			{
				_DestroyStartedPromise.f_SetResult();
				co_return {};
			}

			TCSharedPointer<CAsyncDestroyTestState> m_pTestState;
		};

		struct CImmediateAsyncDestroyActor : public CAsyncDestroyActor
		{
			CImmediateAsyncDestroyActor(TCSharedPointer<CAsyncDestroyTestState> const &_pTestState)
				: CAsyncDestroyActor(_pTestState)
			{
			}

			TCFuture<void> fp_Destroy() override
			{
				++m_pTestState->m_nStartDestroy;
				++m_pTestState->m_nDestroyed;
				co_return {};
			}
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

								co_await g_Yield;

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

								co_await g_Yield;

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

				DMibTestCategory("Actor Abort Suspended") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCFuture<void> DestroyFuture;
					{
						TCPromise<void> DestroyPromise;
						{
							TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);

							DestroyFuture = TestDestroyActor(&CAsyncDestroyActor::f_TestDestroy, DestroyPromise.f_Future());
							fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

							DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 2);
							DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 2);
						}
						DestroyPromise.f_SetResult();
					}

					DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));

					co_return {};
				};

				DMibTestCategory("Actor Abort Suspended Order") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCFuture<void> DestroyFuture;
					{
						TCPromise<void> DestroyPromise0;
						TCPromise<void> DestroyPromise1;
						TCPromise<void> DestroyStartedPromise;
						{
							TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);

							DestroyFuture = TestDestroyActor
								(
									&CAsyncDestroyActor::f_TestDestroyOrder
									, DestroyPromise0.f_Future()
									, DestroyPromise1.f_Future()
									, DestroyStartedPromise
								)
							;

							auto DestroyActorFuture = fg_Move(TestDestroyActor).f_Destroy();
							co_await DestroyStartedPromise.f_Future();
							DestroyPromise0.f_SetResult();
							DestroyPromise1.f_SetResult();

							fg_Move(DestroyActorFuture).f_CallSync();

							DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
							DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);
						}
					}

					DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibImpErrorInstance(CExceptionActorDeleted, "Actor called has been deleted"));

					co_return {};
				};

				DMibTestCategory("Actor Async Destroy At Once") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);
					TCFuture<void> DestroyFuture;
					{
						TCPromise<void> DestroyPromise0;
						TCPromise<void> DestroyStartedPromise;
						{

							DestroyFuture = TestDestroyActor
								(
									&CAsyncDestroyActor::f_TestDestroyAtOnce
									, DestroyPromise0.f_Future()
									, DestroyStartedPromise
								)
							;

							co_await DestroyStartedPromise.f_Future();
							DestroyPromise0.f_SetResult();
						}
					}

					fg_Move(DestroyFuture).f_CallSync();
					fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

					co_return {};
				};

				DMibTestCategory("Actor Async Destroy At Once Error") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);
					TCFuture<void> DestroyFuture;
					{
						TCPromise<void> DestroyStartedPromise;
						{

							DestroyFuture = TestDestroyActor
								(
									&CAsyncDestroyActor::f_TestDestroyAtOnceError
									, DestroyStartedPromise
								)
							;

							co_await DestroyStartedPromise.f_Future();
						}
					}

					DMibExpectException(fg_Move(DestroyFuture).f_CallSync(), DMibErrorInstance("Testing"));
					fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

					co_return {};
				};

				DMibTestCategory("Actor Async Destroy At Once Discard") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);
					{
						TCPromise<void> DestroyPromise0;
						TCPromise<void> DestroyStartedPromise;
						{

							{
								TestDestroyActor
									(
										&CAsyncDestroyActor::f_TestDestroyAtOnce
										, DestroyPromise0.f_Future()
										, DestroyStartedPromise
									)
									.f_DiscardResult()
								;
							}

							co_await DestroyStartedPromise.f_Future();
							DestroyPromise0.f_SetResult();
						}
					}
					fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

					co_return {};
				};

				DMibTestCategory("Actor Async Destroy At Once Error Discard") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);
					{
						TCPromise<void> DestroyStartedPromise;
						{

							{
								TestDestroyActor
									(
										&CAsyncDestroyActor::f_TestDestroyAtOnceError
										, DestroyStartedPromise
									)
									.f_DiscardResult()
								;
							}

							co_await DestroyStartedPromise.f_Future();
						}
					}
					fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

					co_return {};
				};

				DMibTestCategory("Actor Async Destroy At Once No Destroy") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					TCActor<CAsyncDestroyActor> TestDestroyActor = fg_Construct(pTestState);
					{
						TCPromise<void> DestroyStartedPromise;
						{

							{
								TestDestroyActor.f_Bind<&CAsyncDestroyActor::f_TestDestroyAtOnceNoDestory, EVirtualCall::mc_NotVirtual>
									(
										DestroyStartedPromise
									)
									.f_DiscardResult()
								;
							}

							co_await DestroyStartedPromise.f_Future();
						}
					}
					fg_Move(TestDestroyActor).f_Destroy().f_CallSync();

					co_return {};
				};

				DMibTestCategory("Async Generator") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_return {};
							}
						)
					;

					auto Iterator = co_await fg_Move(Generator).f_GetPipelinedIterator(1);
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibAssertTrue(Iterator);
					DMibExpect(*Iterator, ==, 1);

					co_await ++Iterator;
					DMibExpectFalse(Iterator);

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Aborted Async Generator") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));

					co_await fg_Move(Generator).f_Destroy();

					// The body of the coroutine dosen't start running until the iterator is gotten
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));

					co_return {};
				};

				DMibTestCategory("Aborted Async Generator Pipelined") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					auto Iterator = co_await fg_Move(Generator).f_GetPipelinedIterator(1);
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibAssertTrue(Iterator);
					DMibExpect(*Iterator, ==, 1);

					co_await ++Iterator;
					DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));

					co_await fg_Move(Iterator).f_Destroy();

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Aborted Async Generator Simple") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					auto Iterator = co_await fg_Move(Generator).f_GetSimpleIterator();
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibAssertTrue(Iterator);
					DMibExpect(*Iterator, ==, 1);

					co_await ++Iterator;
					DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));

					co_await fg_Move(Iterator).f_Destroy();

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Abandoned Async Generator Pipelined") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CImmediateAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					{
						auto Iterator = co_await fg_Move(Generator).f_GetPipelinedIterator(1);
						DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
						DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
						DMibAssertTrue(Iterator);
						DMibExpect(*Iterator, ==, 1);

						co_await ++Iterator;
						DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

						DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
						DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					}

					co_await fp_BlockOnAllThreads();

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Abandoned Async Generator Simple") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CImmediateAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_yield 1;

								DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 0);
								DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 0);

								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					{
						auto Iterator = co_await fg_Move(Generator).f_GetSimpleIterator();
						DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
						DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
						DMibAssertTrue(Iterator);
						DMibExpect(*Iterator, ==, 1);

						co_await ++Iterator;
						DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

						DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
						DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					}

					co_await fp_BlockOnAllThreads();

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Aborted Changed Owner Async Generator Pipelined") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_ContinueRunningOnActor(fg_ThisConcurrentActor());

								co_yield 1;
								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					auto Iterator = co_await fg_Move(Generator).f_GetPipelinedIterator(1);
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibAssertTrue(Iterator);
					DMibExpect(*Iterator, ==, 1);

					co_await ++Iterator;
					DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));

					co_await fg_Move(Iterator).f_Destroy();

					DMibExpect(pTestState->m_nDestroyed.f_Load(), ==, 1);
					DMibExpect(pTestState->m_nDestructed.f_Load(), ==, 1);

					co_return {};
				};

				DMibTestCategory("Aborted Changed Owner Async Generator Simple") -> TCFuture<void>
				{
					TCSharedPointer<CAsyncDestroyTestState> pTestState = fg_Construct();

					auto Generator = fg_CallSafe
						(
							[pTestState]() -> TCAsyncGenerator<uint32>
							{
								TCActor<CAsyncDestroyActor> Variable = fg_Construct(pTestState);

								auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);

								co_await fg_ContinueRunningOnActor(fg_ThisConcurrentActor());

								co_yield 1;
								co_yield 1;
								co_yield 1;

								co_return {};
							}
						)
					;

					auto Iterator = co_await fg_Move(Generator).f_GetSimpleIterator();
					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("2"));
					DMibAssertTrue(Iterator);
					DMibExpect(*Iterator, ==, 1);

					co_await ++Iterator;
					DMibTest(!!DMibExpr(Iterator) && DMibExpr("2"));

					DMibTest(DMibExpr(pTestState->m_nDestroyed.f_Load()) == DMibExpr(0) && DMibExpr("3"));
					DMibTest(DMibExpr(pTestState->m_nDestructed.f_Load()) == DMibExpr(0) && DMibExpr("3"));

					co_await fg_Move(Iterator).f_Destroy();

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

								co_await g_Yield;

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

								co_await g_Yield;

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
											co_await g_Yield;
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
											co_await g_Yield;
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
											co_await g_Yield;
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

								co_await g_Yield;

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
								for (umint i = 0; i < 10; ++i)
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
								for (umint i = 0; i < 10; ++i)
								{
									{
										CToAsyncDestroy Variable(pTestState);
										auto AsyncDestroy = co_await fg_AsyncDestroy(Variable);
									}
									co_await g_AsyncDestroy;
									for (auto &Result : co_await g_AsyncDestroy.f_Wrap())
									{
										if (!Result)
											DMibConOut("Error: {}\n", Result.f_GetExceptionStr());
									}
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

								co_await g_Yield;

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

								co_await g_Yield;

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
					auto Destroy = g_OnScopeExit / [&]
						{
							fg_Move(TestActor).f_Destroy().f_CallSync();
						}
					;

					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeException, false).f_CallSync(), DMibErrorInstance("Aborted"));
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeException, true).f_CallSync(), DMibErrorInstance("Aborted"));

					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionNoSuspend, false).f_CallSync());
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionNoSuspend, true).f_CallSync(), DMibErrorInstance("Aborted"));

					// TCAsyncResult<int32> - return value on initial call (immediate return)
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValue, true).f_CallSync(), ==, 42);
					// TCAsyncResult<int32> - return value after suspend (first call empty, second call returns value)
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValue, false).f_CallSync(), ==, 42);
					// TCAsyncResult<int32> - no suspend, return value on initial call
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValueNoSuspend, true).f_CallSync(), ==, 42);
					// TCAsyncResult<int32> - no suspend, empty result initially (no suspend means functor not called again, so continues to natural return)
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValueNoSuspend, false).f_CallSync(), ==, 99);
					// TCAsyncResult<int32> - exception on initial call
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeResultException, true).f_CallSync(), DMibErrorInstance("OnResumeException"));
					// TCAsyncResult<int32> - exception after suspend
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeResultException, false).f_CallSync(), DMibErrorInstance("OnResumeException"));

					// TCAsyncResult<void> - return void value on initial call
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeVoidValue, true).f_CallSync());
					// TCAsyncResult<void> - return void value after suspend
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeVoidValue, false).f_CallSync());
					// TCAsyncResult<void> - exception on initial call
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeVoidException, true).f_CallSync(), DMibErrorInstance("OnResumeVoidException"));
					// TCAsyncResult<void> - exception after suspend
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeVoidException, false).f_CallSync(), DMibErrorInstance("OnResumeVoidException"));

					// fg_OnResume with g_CaptureExceptions - return value initially
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureExceptions, true).f_CallSync(), ==, 42);
					// fg_OnResume with g_CaptureExceptions - return value after suspend
					DMibExpect(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureExceptions, false).f_CallSync(), ==, 42);
					// fg_OnResume with g_CaptureExceptions - exception initially
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionWithCaptureExceptions, true).f_CallSync(), DMibErrorInstance("OnResumeWithCapture"));
					// fg_OnResume with g_CaptureExceptions - exception after suspend
					DMibExpectException(TestActor(&CTestActor::f_TestOnResumeExceptionWithCaptureExceptions, false).f_CallSync(), DMibErrorInstance("OnResumeWithCapture"));
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
					auto Destroy = g_OnScopeExit / [&]
						{
							fg_Move(TestActor).f_Destroy().f_CallSync();
						}
					;

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

					// fg_OnResume returning value with capture inside try/catch - without outer capture scope
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureInsideTryCatch, true).f_CallSync(), CaptureInsideTryCatchError);
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureInsideTryCatch, false).f_CallSync(), CaptureInsideTryCatchError);

					// fg_OnResume returning value with capture inside try/catch - with outer capture scope
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureInsideTryCatchOuter, true).f_CallSync(), CaptureInsideTryCatchError);
					DMibExpectViolatesSafeCheck(TestActor(&CTestActor::f_TestOnResumeValueWithCaptureInsideTryCatchOuter, false).f_CallSync(), CaptureInsideTryCatchError);
#endif
				};
#if 0
				// Throwing from unhandled_exception is not supported on MSVC
				// Clang doesn't call the correct destructors when throwing from unhandled_exception

				DMibTestCategory("NotCaught")
				{
					if (fg_MemoryManagerFeatures() & EMemoryManagerFeatureFlag_TraceLeaks)
						return; // Clang doesn't call the correct destructors when throwing from unhandled_exception

					CActorRunLoopTestHelper RunLoopHelper;

					auto fTest = [&]() -> TCFuture<void>
						{
							auto CaptureExceptions = co_await (g_CaptureExceptions.f_Specific<CCustomException>() % (fg_LogError("Test", "Test") % "User Message Custom"));
							DMibError("Test Custom");
							co_return {};
						}
					;
					DMibExpectException(fTest().f_CallSync(RunLoopHelper.m_pRunLoop), DMibErrorInstance("Test Custom"));
				};
#endif
			};
		}

		void f_TestParamDetection()
		{
			DMibTestSuite("ParamDetection") -> TCFuture<void>
			{
				CTestClass Testing;

				co_await CTestActor2::fs_TestParams(&Testing, 5);

				co_return {};
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
			f_TestParamDetection();
		}
	};

	DMibTestRegister(CCoroutines_Tests, Malterlib::Concurrency);
}
