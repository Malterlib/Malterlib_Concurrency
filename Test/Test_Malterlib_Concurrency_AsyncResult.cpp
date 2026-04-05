// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

#include <Mib/Concurrency/AsyncResult>

namespace
{
	using namespace NMib;
	using namespace NMib::NConcurrency;
	using namespace NMib::NException;

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
		}
	};

	DMibTestRegister(CAsyncResult_Tests, Malterlib::Concurrency);
}
