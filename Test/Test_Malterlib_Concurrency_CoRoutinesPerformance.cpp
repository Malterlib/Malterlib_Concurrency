// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Coroutine>
#include <Mib/Test/Performance>
#include <Mib/Test/Memory>

#if defined(DMalterlibEnableThirdPartyComparisonTests)
	#include <cppcoro/task.hpp>
	#include <cppcoro/sync_wait.hpp>
#endif

#ifdef DCompiler_clang
#pragma clang diagnostic ignored "-Walways-inline-coroutine"
#endif

//#define DDoSleep NSys::fg_Thread_Sleep(0.1f)
#define DDoSleep

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

	static thread_local umint g_ThreadLocalCpp;
	static thread_local umint g_ThreadLocalCpp2;

	class CCoroutinesPerformance_Tests : public NMib::NTest::CTest
	{
#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
		constexpr static umint mc_nLoops = 16384;
#else
		constexpr static umint mc_nLoops = 16384 * 128 * 10;
#endif

		static inline_never uint32 fs_TestRecursive(uint32 _Value)
		{
			volatile uint32 Result = _Value * 2;
			return Result;
		}

		static inline_never uint32 fs_TestFunction(uint32 _Value)
		{
			volatile uint32 Return = 0;
			for (umint i = 0; i < mc_nLoops; ++i)
				Return = Return + fs_TestRecursive(_Value);
			return Return;
		}

		static inline_always TCFuture<uint32> fs_TestRecursiveCoro(uint32 _Value)
		{
			co_return _Value * 2;
		}

		static inline_never TCFuture<uint32> fs_TestFunctionCoro(uint32 _Value)
		{
			uint32 Return = 0;
			for (umint i = 0; i < mc_nLoops; ++i)
				Return += co_await fs_TestRecursiveCoro(_Value);
			co_return Return;
		}

#if defined(DMalterlibEnableThirdPartyComparisonTests)
		static inline_always cppcoro::task<uint32> fs_TestRecursiveCppCoro(uint32 _Value)
		{
			co_return _Value * 2;
		}

		static inline_never cppcoro::task<uint32> fs_TestFunctionCppCoro(uint32 _Value)
		{
			uint32 Return = 0;
			for (umint i = 0; i < mc_nLoops; ++i)
				Return += co_await fs_TestRecursiveCppCoro(_Value);
			co_return Return;
		}
#endif

		void f_DoTests()
		{
			DMibTestSuite(CTestCategory("ThreadSelf") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal]() inline_never
							{
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += NMib::NSys::fg_Thread_GetCurrentUID();;
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGet") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal = NMib::NSys::fg_Thread_AllocLocal();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocal(ThreadLocal);
					}
				;
				NMib::NSys::fg_Thread_SetLocal(ThreadLocal, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal]() inline_never
							{
								auto ThreadLocal2 = ThreadLocal;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += (umint)NMib::NSys::fg_Thread_GetLocal(ThreadLocal2);
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGetC++") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				g_ThreadLocalCpp = 0;

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal]() inline_never
							{
								for (umint i = 0; i < mc_nLoops; ++i)
									ReturnNormal += (umint)g_ThreadLocalCpp;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGetC++2") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				g_ThreadLocalCpp2 = 0;

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal]() inline_never
							{
								for (umint i = 0; i < mc_nLoops; ++i)
									ReturnNormal += (umint)g_ThreadLocalCpp2;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGetAlwaysSet") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal = NMib::NSys::fg_Thread_AllocLocal();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocal(ThreadLocal);
					}
				;
				NMib::NSys::fg_Thread_SetLocal(ThreadLocal, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal]() inline_never
							{
								auto ThreadLocal2 = ThreadLocal;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += (umint)NMib::NSys::fg_Thread_GetLocalAlwaysSet(ThreadLocal2);

								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGetFast") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal = NMib::NSys::fg_Thread_AllocLocalFast();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocalFast(ThreadLocal);
					}
				;
				NMib::NSys::fg_Thread_SetLocalFast(ThreadLocal, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal]() inline_never
							{
								auto ThreadLocal2 = ThreadLocal;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += (umint)NMib::NSys::fg_Thread_GetLocalFast(ThreadLocal2);
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadGetAlwaysSetFast") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal = NMib::NSys::fg_Thread_AllocLocalFast();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocalFast(ThreadLocal);
					}
				;
				NMib::NSys::fg_Thread_SetLocalFast(ThreadLocal, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal]() inline_never
							{
								auto ThreadLocal2 = ThreadLocal;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += (umint)NMib::NSys::fg_Thread_GetLocalAlwaysSetFast(ThreadLocal2);
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadBoth") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal = NMib::NSys::fg_Thread_AllocLocal();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocal(ThreadLocal);
					}
				;
				NMib::NSys::fg_Thread_SetLocal(ThreadLocal, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				volatile uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal]() inline_never
							{
								auto ThreadLocal2 = ThreadLocal;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
									Return += NMib::NSys::fg_Thread_GetCurrentUID() + (umint)NMib::NSys::fg_Thread_GetLocal(ThreadLocal2);
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("ThreadTriple") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				auto ThreadLocal0 = NMib::NSys::fg_Thread_AllocLocal();
				auto ThreadLocal1 = NMib::NSys::fg_Thread_AllocLocal();
				auto Cleanup = g_OnScopeExit / [&]
					{
						NMib::NSys::fg_Thread_FreeLocal(ThreadLocal0);
						NMib::NSys::fg_Thread_FreeLocal(ThreadLocal1);
					}
				;
				NMib::NSys::fg_Thread_SetLocal(ThreadLocal0, nullptr);
				NMib::NSys::fg_Thread_SetLocal(ThreadLocal1, nullptr);

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				uint32 ReturnNormal;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						[&ReturnNormal, ThreadLocal0, ThreadLocal1]() inline_never
							{
								auto ThreadLocalCopy0 = ThreadLocal0;
								auto ThreadLocalCopy1 = ThreadLocal1;
								auto Return = ReturnNormal;
								for (umint i = 0; i < mc_nLoops; ++i)
								{
									Return += NMib::NSys::fg_Thread_GetCurrentUID()
										+ (umint)NMib::NSys::fg_Thread_GetLocal(ThreadLocalCopy0)
										+ (umint)NMib::NSys::fg_Thread_GetLocal(ThreadLocalCopy1)
									;
								}
								ReturnNormal = Return;
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(NormalTime);
				}

				DMibExpectTrue(PerfTest);

				co_return {};
			};

			DMibTestSuite(CTestCategory("Synchronous") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

				CTestPerformance PerfTest(0.015);
				CTestPerformanceMeasure NormalTime("Normal");
				CTestPerformanceMeasure CoroutineTime("Coroutine");
				CTestPerformanceMeasure CppCoroTime("CppCoro");
				uint32 ReturnNormal;
				DDoSleep;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						NormalTime.f_Start();
						uint32 Multiplier = 2;
						[&Multiplier, &ReturnNormal]() inline_never
							{
								ReturnNormal = fs_TestFunction(Multiplier);
							}
							()
						;
						NormalTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_AddBaseline(NormalTime);
				}

				DDoSleep;
				uint32 ReturnCoro;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						CoroutineTime.f_Start();
						uint32 Multiplier = 2;
						[&Multiplier, &ReturnCoro]() inline_never
							{
								ReturnCoro = fs_TestFunctionCoro(Multiplier).f_CallSync();
							}
							()
						;
						CoroutineTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_Add(CoroutineTime);
					DMibExpect(ReturnCoro, ==, ReturnNormal);
				}

				DDoSleep;
#if defined(DMalterlibEnableThirdPartyComparisonTests)
				uint32 ReturnCppCoro;
				{
					for(umint j = 0; j < nTests; ++j)
					{
						CppCoroTime.f_Start();
						uint32 Multiplier = 2;
						[&Multiplier, &ReturnCppCoro]() inline_never
							{
								ReturnCppCoro = cppcoro::sync_wait(fs_TestFunctionCppCoro(Multiplier));
							}
							()
						;
						CppCoroTime.f_Stop(mc_nLoops);
					}
					PerfTest.f_AddReference(CppCoroTime);
					DMibExpect(ReturnCppCoro, ==, ReturnNormal);
				}
#endif
				DDoSleep;
				DMibExpectTrue(PerfTest);

				co_return {};
			};
			DMibTestSuite(CTestCategory("Tasks") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 10;

#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
				constexpr umint c_nTasks = 1'000;
#else
				constexpr umint c_nTasks = 1'000'000;
#endif

				CTestPerformanceMeasure MalterlibTime("Malterlib5");
				TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};
				{
					for(umint j = 0; j < nTests; ++j)
					{
						MalterlibTime.f_Start();
						TCVector<TCPromise<void>> Promises;
						Promises.f_SetLen(c_nTasks);

						TCFutureVector<void> Results;
						Results.f_SetLen(c_nTasks);

						NAtomic::TCAtomic<bool> bAllScheduled{false};
						NThread::CEvent Event;
						for (auto &Promise : Promises)
						{
							g_Dispatch(LaunchActor) / [&, Future = Promise.f_Future()]() mutable -> TCFuture<void>
								{
									if (!bAllScheduled.f_Load())
										Event.f_Wait();

									co_await fg_Move(Future);
									co_return {};
								}
								> Results
							;
						}

						bAllScheduled.f_Exchange(true);
						Event.f_SetSignaled();
						for (auto &Promise : Promises)
							Promise.f_SetResult();

						co_await fg_AllDoneWrapped(Results);
						MalterlibTime.f_Stop(c_nTasks);
					}
				}

				CTestPerformance PerfTest(0.015);
				PerfTest.f_Add(MalterlibTime);
				DMibExpectTrue(PerfTest);

				co_return {};
			};
		}
	};

	DMibTestRegister(CCoroutinesPerformance_Tests, Malterlib::Concurrency);
}
