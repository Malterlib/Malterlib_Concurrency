// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Test/Test>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/RunLoop>
#ifdef DPlatformFamily_macOS
#	include <Mib/Concurrency/OSMainRunLoop>
#	include <mach/mach.h>
#	include <objc/message.h>
#	include <objc/runtime.h>
#	include <pthread.h>

// What @autoreleasepool compiles to; the SDK headers do not declare them
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *_pPool);
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

				DMibTestCategory("WakeConsumedThroughOtherWrapper")
				{
					NStorage::TCSharedPointer<COSMainRunLoop> pFirst = fg_Construct();
					NStorage::TCSharedPointer<COSMainRunLoop> pSecond = fg_Construct();

					pFirst->f_Wake();
					bool bSecondTimedOut = pSecond->f_WaitOnceTimeout(0.0);
					DMibExpect(bSecondTimedOut, ==, false);

					pFirst->f_Wake();
					bool bFirstTimedOut = pFirst->f_WaitOnceTimeout(0.0);
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

#ifdef DPlatformFamily_macOS
			DMibTestSuite("ApplicationEventPump")
			{
				if (!pthread_main_np())
				{
					DMibLog(Info, "Skipped: the application event pump needs the main thread");
					return;
				}

				// AppKit keeps its launch state for the life of the process, which a leak trace
				// cannot tell from a leak
				if (NMemory::fg_MemoryManagerFeatures() & NMemory::EMemoryManagerFeatureFlag_TraceLeaks)
				{
					DMibLog(Info, "Skipped: the application object's state would be traced as leaks");
					return;
				}

#if defined(DMibSanitizerEnabled_Address) || defined(DMibSanitizerEnabled_Thread)
				// Quarantined frees and shadow memory grow the resident size far past what a pool leak would
				DMibLog(Info, "Skipped: a sanitizer's own bookkeeping dominates the resident size");
				return;
#endif

				auto fResidentBytes = []
					{
						mach_task_basic_info Info;
						mach_msg_type_number_t nInfo = MACH_TASK_BASIC_INFO_COUNT;
						task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&Info, &nInfo);
						return smint(Info.resident_size);
					}
				;

				{
					void *pPool = objc_autoreleasePoolPush();
					id pApplication = ((id (*)(Class, SEL))objc_msgSend)(objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
					((void (*)(id, SEL, long))objc_msgSend)(pApplication, sel_registerName("setActivationPolicy:"), 2); // NSApplicationActivationPolicyProhibited
					objc_autoreleasePoolPop(pPool);
				}

				NStorage::TCSharedPointer<COSMainRunLoop> pLoop = fg_Construct(EOSMainRunLoopMode::mc_ApplicationEvents);

				constexpr umint c_nIterations = 200000;
				constexpr smint c_MaxGrowth = 16 * 1024 * 1024;

				auto fPump = [&](umint _nIterations)
					{
						for (umint i = 0; i < _nIterations; ++i)
						{
							pLoop->f_Wake();
							pLoop->f_WaitOnceTimeout(0.0);
						}
					}
				;

				fPump(c_nIterations / 10);
				smint Before = fResidentBytes();
				fPump(c_nIterations);
				smint MainThreadGrowth = fResidentBytes() - Before;
				DMibLog(Info, "Wakes from the main thread: resident size grew {} bytes over {} pumps", MainThreadGrowth, c_nIterations);

				NAtomic::TCAtomic<bool> bWakesDone = false;
				Before = fResidentBytes();
				auto pWaker = NThread::CThreadObject::fs_StartThread
					(
						[&](NThread::CThreadObject *) -> aint
						{
							for (umint i = 0; i < c_nIterations; ++i)
								pLoop->f_Wake();

							bWakesDone = true;
							pLoop->f_Wake();
							return 0;
						}
						, "Application wakes"
					)
				;
				while (!bWakesDone)
					pLoop->f_WaitOnceTimeout(0.01);
				pWaker->f_Stop(true);

				// A wake posted from another thread reaches the queue through the main run loop, so
				// the last one is only in the queue after a further pass
				while (!pLoop->f_WaitOnceTimeout(0.01))
					;
				pLoop->f_WaitOnceTimeout(0.05);
				smint OtherThreadGrowth = fResidentBytes() - Before;
				DMibLog(Info, "Wakes from another thread: resident size grew {} bytes over {} wakes", OtherThreadGrowth, c_nIterations);

				DMibExpect(MainThreadGrowth, <, c_MaxGrowth);
				DMibExpect(OtherThreadGrowth, <, c_MaxGrowth);
			};
#endif
		}
	};

	DMibTestRegister(CRunLoop_Tests, Malterlib::Concurrency);
}
