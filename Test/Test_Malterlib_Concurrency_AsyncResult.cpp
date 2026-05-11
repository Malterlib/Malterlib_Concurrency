// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

#include <Mib/Concurrency/AsyncResult>
#include <Mib/Concurrency/ConcurrencyManager>

namespace
{
	using namespace NMib;
	using namespace NMib::NConcurrency;
	using namespace NMib::NException;

	class CAsyncResultTestActor : public CActor
	{
	public:
		TCFuture<int32> f_TestAwaitAsyncResultValue()
		{
			TCAsyncResult<int32> Result;
			Result.f_SetResult(41);

			int32 Value = co_await fg_Move(Result);
			co_return Value + 1;
		}

		TCFuture<int32> f_TestAwaitAsyncResultException()
		{
			TCAsyncResult<int32> Result;
			Result.f_SetException(DMibErrorInstance("AsyncResult exception"));

			int32 Value = co_await fg_Move(Result);
			co_return Value;
		}

		TCFuture<void> f_TestAwaitAsyncResultVoidException()
		{
			TCAsyncResult<void> Result;
			Result.f_SetException(DMibErrorInstance("AsyncResult void exception"));

			co_await fg_Move(Result);
			co_return {};
		}

		TCFuture<int32> f_TestReturnWrappedValue()
		{
			co_return TCWrapped<int32>(42);
		}

		TCFuture<int32> f_TestReturnWrappedException()
		{
			co_return TCWrapped<int32>(DMibErrorInstance("Wrapped exception"));
		}

		TCFuture<void> f_TestReturnWrappedVoid()
		{
			co_return TCWrapped<void>();
		}

		TCFuture<void> f_TestReturnWrappedVoidException()
		{
			co_return TCWrapped<void>(DMibErrorInstance("Wrapped void exception"));
		}

		TCFuture<int64> f_TestReturnWrappedConvertedValue()
		{
			TCWrapped<int32> Result(42);
			co_return fg_Move(Result);
		}

		TCFuture<int64> f_TestReturnWrappedConvertedException()
		{
			TCWrapped<int32> Result(DMibErrorInstance("Wrapped converted exception"));
			co_return fg_Move(Result);
		}

		TCFuture<int64> f_TestReturnAsyncResultConvertedValue()
		{
			TCAsyncResult<int32> Result;
			Result.f_SetResult(42);
			co_return fg_Move(Result);
		}

		TCFuture<int64> f_TestReturnAsyncResultConvertedException()
		{
			TCAsyncResult<int32> Result;
			Result.f_SetException(DMibErrorInstance("AsyncResult converted exception"));
			co_return fg_Move(Result);
		}

		TCFuture<int32> f_TestWrappedErrorTransform()
		{
			int32 Value = co_await (TCWrapped<int32>(DMibErrorInstance("Wrapped transform exception")) % "Failed doing X");
			co_return Value;
		}
	};

	TCWrapped<int32> fg_WrappedCoroutineReturnValue()
	{
		co_return 42;
	}

	TCWrapped<int32> fg_WrappedCoroutineReturnException()
	{
		co_return DMibErrorInstance("Wrapped coroutine exception");
	}

	TCWrapped<void> fg_WrappedCoroutineReturnVoid()
	{
		co_return {};
	}

	TCWrapped<void> fg_WrappedCoroutineReturnVoidException()
	{
		co_return DMibErrorInstance("Wrapped coroutine void exception");
	}

	TCWrapped<int32> fg_WrappedCoroutineAwaitWrappedValue()
	{
		int32 Value = co_await TCWrapped<int32>(10);
		co_return Value + 32;
	}

	TCWrapped<int32> fg_WrappedCoroutineAwaitWrappedException()
	{
		int32 Value = co_await TCWrapped<int32>(DMibErrorInstance("Inner wrapped exception"));
		co_return Value + 1;
	}

	TCWrapped<int32> fg_WrappedCoroutineAwaitAsyncResultValue()
	{
		TCAsyncResult<int32> Inner;
		Inner.f_SetResult(7);
		int32 Value = co_await fg_Move(Inner);
		co_return Value * 6;
	}

	TCWrapped<int32> fg_WrappedCoroutineAwaitAsyncResultException()
	{
		TCAsyncResult<int32> Inner;
		Inner.f_SetException(DMibErrorInstance("Inner async exception"));
		int32 Value = co_await fg_Move(Inner);
		co_return Value;
	}

	TCWrapped<void> fg_WrappedCoroutineAwaitVoidException()
	{
		TCAsyncResult<void> Inner;
		Inner.f_SetException(DMibErrorInstance("Inner async void exception"));
		co_await fg_Move(Inner);
		co_return {};
	}

	TCWrapped<int64> fg_WrappedCoroutineForwardWrapped()
	{
		co_return TCWrapped<int32>(99);
	}

	TCWrapped<int64> fg_WrappedCoroutineForwardWrappedException()
	{
		co_return TCWrapped<int32>(DMibErrorInstance("Forwarded wrapped exception"));
	}

	TCWrapped<int32> fg_WrappedFromSameTypeAsyncResultValue()
	{
		TCAsyncResult<int32> Result;
		Result.f_SetResult(123);

		return TCWrapped<int32>(fg_Move(Result));
	}

	TCWrapped<int32> fg_WrappedFromSameTypeAsyncResultException()
	{
		TCAsyncResult<int32> Result;
		Result.f_SetException(DMibErrorInstance("Same-type async result exception"));

		return TCWrapped<int32>(fg_Move(Result));
	}

	TCWrapped<void> fg_WrappedFromSameTypeAsyncResultVoidValue()
	{
		TCAsyncResult<void> Result;
		Result.f_SetResult();

		return TCWrapped<void>(fg_Move(Result));
	}

	TCWrapped<void> fg_WrappedFromSameTypeAsyncResultVoidException()
	{
		TCAsyncResult<void> Result;
		Result.f_SetException(DMibErrorInstance("Same-type async result void exception"));

		return TCWrapped<void>(fg_Move(Result));
	}

	class CAsyncResult_Tests : public NMib::NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("ExceptionTests")
			{
				TCAsyncResult<void> Result;
				Result.f_SetException(DMibImpExceptionInstance(CExceptionMemory, "Test exception"));

				DMibExpectTrue(Result.f_HasExceptionType<CExceptionMemory>());
				DMibExpectTrue(Result.f_HasExceptionType<CException>());

				DMibExpectException(Result.f_Access(), DMibImpExceptionInstance(CExceptionMemory, "Test exception"));
			};

			DMibTestSuite("CoroutineTests")
			{
				TCActor<CAsyncResultTestActor> TestActor(fg_Construct());

				DMibExpect(TestActor(&CAsyncResultTestActor::f_TestAwaitAsyncResultValue).f_CallSync(), ==, 42);
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestAwaitAsyncResultException).f_CallSync(), DMibErrorInstance("AsyncResult exception"));
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestAwaitAsyncResultVoidException).f_CallSync(), DMibErrorInstance("AsyncResult void exception"));

				DMibExpect(TestActor(&CAsyncResultTestActor::f_TestReturnWrappedValue).f_CallSync(), ==, 42);
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestReturnWrappedException).f_CallSync(), DMibErrorInstance("Wrapped exception"));
				TestActor(&CAsyncResultTestActor::f_TestReturnWrappedVoid).f_CallSync();
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestReturnWrappedVoidException).f_CallSync(), DMibErrorInstance("Wrapped void exception"));
				DMibExpect(TestActor(&CAsyncResultTestActor::f_TestReturnWrappedConvertedValue).f_CallSync(), ==, 42);
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestReturnWrappedConvertedException).f_CallSync(), DMibErrorInstance("Wrapped converted exception"));
				DMibExpect(TestActor(&CAsyncResultTestActor::f_TestReturnAsyncResultConvertedValue).f_CallSync(), ==, 42);
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestReturnAsyncResultConvertedException).f_CallSync(), DMibErrorInstance("AsyncResult converted exception"));

				NException::CExceptionPointer pDummy;
				DMibExpectException(TestActor(&CAsyncResultTestActor::f_TestWrappedErrorTransform).f_CallSync(), DMibErrorInstanceWrapped("Failed doing X: Wrapped transform exception", pDummy));

				fg_Move(TestActor).f_Destroy().f_CallSync();
			};

			DMibTestSuite("WrappedCoroutineTests")
			{
				DMibExpect(*fg_WrappedCoroutineReturnValue(), ==, 42);
				DMibExpectException(fg_WrappedCoroutineReturnException().f_Access(), DMibErrorInstance("Wrapped coroutine exception"));

				DMibExpectTrue(bool(fg_WrappedCoroutineReturnVoid()));
				DMibExpectException(fg_WrappedCoroutineReturnVoidException().f_Access(), DMibErrorInstance("Wrapped coroutine void exception"));

				DMibExpect(*fg_WrappedCoroutineAwaitWrappedValue(), ==, 42);
				DMibExpectException(fg_WrappedCoroutineAwaitWrappedException().f_Access(), DMibErrorInstance("Inner wrapped exception"));

				DMibExpect(*fg_WrappedCoroutineAwaitAsyncResultValue(), ==, 42);
				DMibExpectException(fg_WrappedCoroutineAwaitAsyncResultException().f_Access(), DMibErrorInstance("Inner async exception"));
				DMibExpectException(fg_WrappedCoroutineAwaitVoidException().f_Access(), DMibErrorInstance("Inner async void exception"));

				DMibExpect(*fg_WrappedCoroutineForwardWrapped(), ==, 99);
				DMibExpectException(fg_WrappedCoroutineForwardWrappedException().f_Access(), DMibErrorInstance("Forwarded wrapped exception"));

				DMibExpect(*fg_WrappedFromSameTypeAsyncResultValue(), ==, 123);
				DMibExpectException(fg_WrappedFromSameTypeAsyncResultException().f_Access(), DMibErrorInstance("Same-type async result exception"));
				DMibExpectTrue(bool(fg_WrappedFromSameTypeAsyncResultVoidValue()));
				DMibExpectException(fg_WrappedFromSameTypeAsyncResultVoidException().f_Access(), DMibErrorInstance("Same-type async result void exception"));
			};
		}
	};

	DMibTestRegister(CAsyncResult_Tests, Malterlib::Concurrency);
}
