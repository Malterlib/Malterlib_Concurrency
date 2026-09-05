// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Test/Test>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/RunLoop>
#ifdef DPlatformFamily_macOS
#	include <Mib/Concurrency/OSMainRunLoop>
#endif

namespace NMib::NConcurrency
{
	struct CRunLoop_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("ReferenceWake") -> TCFuture<void>
			{
				auto Capture = co_await (g_CaptureExceptions % "Testing run-loop reference notifications");

				DMibTestCategory("LastExternalReference")
				{
					NStorage::TCSharedPointer<CDefaultRunLoop> pLoop = fg_Construct();
					NStorage::TCSharedPointer<CRunLoop> pOther = pLoop;
					NStorage::TCSharedPointer<CRunLoop> pThird = pLoop;

					pOther.f_Clear();
					bool bTimedOutBeforeLastRelease = pLoop->f_WaitOnceTimeout(0.0);
					DMibExpect(bTimedOutBeforeLastRelease, ==, true);

					pThird.f_Clear();
					bool bTimedOutAfterLastRelease = pLoop->f_WaitOnceTimeout(0.0);
					bool bTimedOutAfterConsumption = pLoop->f_WaitOnceTimeout(0.0);
					DMibExpect(bTimedOutAfterLastRelease, ==, false);
					DMibExpect(bTimedOutAfterConsumption, ==, true);
				};

				DMibTestCategory("ReleaseOnAnotherThread") -> TCFuture<void>
				{
					auto Capture = co_await (g_CaptureExceptions % "Testing cross-thread reference release");
					NStorage::TCSharedPointer<CDefaultRunLoop> pLoop = fg_Construct();
					NStorage::TCSharedPointer<CRunLoop> pOther = pLoop;
					auto BlockingActor = fg_BlockingActor();

					co_await
						(
							g_Dispatch(BlockingActor) / [pOther = fg_Move(pOther)]() mutable
							{
								pOther.f_Clear();
							}
						)
					;

					DMibExpect(pLoop->f_WaitOnceTimeout(0.0), ==, false);
					DMibExpect(pLoop->m_RefCount.f_Get(), ==, 0);

					co_return {};
				};

#ifdef DPlatformFamily_macOS
				DMibTestCategory("DistinctNativeSources")
				{
					NStorage::TCSharedPointer<COSMainRunLoop> pFirst = fg_Construct();
					NStorage::TCSharedPointer<COSMainRunLoop> pSecond = fg_Construct();

					pSecond->f_Wake();
					bool bSecondTimedOut = pSecond->f_WaitOnceTimeout(0.0);
					pFirst->f_Wake();
					bool bFirstTimedOut = pFirst->f_WaitOnceTimeout(0.0);

					DMibExpect(bSecondTimedOut, ==, false);
					DMibExpect(bFirstTimedOut, ==, false);
				};

				DMibTestCategory("OSMainLoop") -> TCFuture<void>
				{
					auto Capture = co_await (g_CaptureExceptions % "Testing native run-loop reference release");
					NStorage::TCSharedPointer<COSMainRunLoop> pLoop = fg_Construct();
					NStorage::TCSharedPointer<CRunLoop> pOther = pLoop;
					auto BlockingActor = fg_BlockingActor();

					co_await
						(
							g_Dispatch(BlockingActor) / [pOther = fg_Move(pOther)]() mutable
							{
								pOther.f_Clear();
							}
						)
					;

					bool bTimedOutAfterRelease = pLoop->f_WaitOnceTimeout(0.0);
					bool bTimedOutAfterConsumption = pLoop->f_WaitOnceTimeout(0.01);
					DMibExpect(bTimedOutAfterRelease, ==, false);
					DMibExpect(bTimedOutAfterConsumption, ==, true);
					DMibExpect(pLoop->m_RefCount.f_Get(), ==, 0);

					pLoop->f_Wake();
					pLoop->f_Wake();
					pLoop->f_WaitOnce();
					bool bTimedOutAfterCoalescedWake = pLoop->f_WaitOnceTimeout(0.01);
					DMibExpect(bTimedOutAfterCoalescedWake, ==, true);

					co_return {};
				};
#endif

				co_return {};
			};
		}
	};

	DMibTestRegister(CRunLoop_Tests, Malterlib::Concurrency);
}
