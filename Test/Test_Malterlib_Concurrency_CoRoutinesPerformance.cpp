// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

	class CPingPongActor : public CActor
	{
	public:
		CPingPongActor()
		{
		}

		CPingPongActor(TCActor<CPingPongActor> &&_Next)
			: mp_Next(fg_Move(_Next))
		{
		}

		TCFuture<uint32> f_Echo(uint32 _Value)
		{
			if (mp_Next)
				co_return co_await mp_Next(&CPingPongActor::f_Echo, _Value + 1);

			co_return _Value + 1;
		}

		TCFuture<uint32> f_Run(TCActor<CPingPongActor> _Target, umint _nRoundTrips)
		{
			uint32 Value = 0;
			for (umint i = 0; i < _nRoundTrips; ++i)
				Value = co_await _Target(&CPingPongActor::f_Echo, Value);

			co_return Value;
		}

		TCFuture<uint32> f_Work(umint _nIterations)
		{
			volatile uint32 Value = 1;
			for (umint i = 0; i < _nIterations; ++i)
				Value = Value * 3 + 1;

			co_return Value;
		}

		TCFuture<uint32> f_FanOut(TCVector<TCActor<CPingPongActor>> _Workers, umint _nIterations)
		{
			TCFutureVector<uint32> Results;
			for (auto &Worker : _Workers)
				Worker(&CPingPongActor::f_Work, _nIterations) > Results;

			co_await fg_AllDoneWrapped(Results);

			co_return 0;
		}

	private:
		TCActor<CPingPongActor> mp_Next;
	};

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
			DMibTestSuite(CTestCategory("ActorPingPong") << CTestGroup("Performance")) -> TCFuture<void>
			{
				umint nTests = 9;

#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
				constexpr umint c_nRoundTrips = 1'000;
#else
				constexpr umint c_nRoundTrips = 50'000;
#endif

#if DMibConfig_Concurrency_SchedulerStats
				auto fReportStats = [](NStr::CStr const &_Name, umint _nRoundTrips, CTestPerformanceMeasure &_Measure)
					{
						auto Stats = fg_ConcurrencyManager().f_GetSchedulerStats();
						DMibTrace
							(
								"ActorPingPong {}: {} round trips, {} cycles/rt, Signals {} Sleeps {} LocalEnqueues {} AtomicEnqueues {} Handoffs {} IdleClaims {} RoundRobin {}\n"
								, _Name
								, _nRoundTrips
								, _Measure.f_CyclesMedian()
								, Stats.m_nSignals
								, Stats.m_nSleeps
								, Stats.m_nLocalEnqueues
								, Stats.m_nAtomicEnqueues
								, Stats.m_nHandoffs
								, Stats.m_nIdleClaims
								, Stats.m_nRoundRobinSelections
							)
						;
					}
				;
#	define DPingPongStatsReset() fg_ConcurrencyManager().f_ResetSchedulerStats()
#	define DPingPongStatsReport(d_Name, d_nRoundTrips, d_Measure) fReportStats(d_Name, d_nRoundTrips, d_Measure)
#else
#	define DPingPongStatsReset() ((void)0)
#	define DPingPongStatsReport(d_Name, d_nRoundTrips, d_Measure) ((void)0)
#endif

				CTestPerformance PerfTest(0.015);

				// A pool actor driving request/response round trips against another pool actor
				{
					CTestPerformanceMeasure PairTime("PoolPair");
					TCActor<CPingPongActor> Ping = fg_ConstructActor<CPingPongActor>();
					TCActor<CPingPongActor> Pong = fg_ConstructActor<CPingPongActor>();

					DPingPongStatsReset();
					for (umint j = 0; j < nTests; ++j)
					{
						PairTime.f_Start();
						co_await Ping(&CPingPongActor::f_Run, Pong, c_nRoundTrips);
						PairTime.f_Stop(c_nRoundTrips);
					}
					DPingPongStatsReport("PoolPair", c_nRoundTrips * nTests, PairTime);
					PerfTest.f_Add(PairTime);
				}

				// The driver runs on a separate (off-pool) thread, so every call crosses into the pool
				{
					CTestPerformanceMeasure OffPoolTime("OffPoolDriver");
					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "PingPong"};
					TCActor<CPingPongActor> Pong = fg_ConstructActor<CPingPongActor>();

					DPingPongStatsReset();
					for (umint j = 0; j < nTests; ++j)
					{
						OffPoolTime.f_Start();
						co_await
							(
								g_Dispatch(LaunchActor) / [Pong]() -> TCFuture<uint32>
								{
									TCActor<CPingPongActor> Target = Pong;

									uint32 Value = 0;
									for (umint i = 0; i < c_nRoundTrips; ++i)
										Value = co_await Target(&CPingPongActor::f_Echo, Value);

									co_return Value;
								}
							)
						;
						OffPoolTime.f_Stop(c_nRoundTrips);
					}
					DPingPongStatsReport("OffPoolDriver", c_nRoundTrips * nTests, OffPoolTime);
					PerfTest.f_Add(OffPoolTime);
				}

				// A three-actor chain per round trip
				{
					CTestPerformanceMeasure ChainTime("Chain3");
					TCActor<CPingPongActor> Tail = fg_ConstructActor<CPingPongActor>();
					TCActor<CPingPongActor> Middle = fg_ConstructActor<CPingPongActor>(fg_Move(Tail));
					TCActor<CPingPongActor> Head = fg_ConstructActor<CPingPongActor>(fg_Move(Middle));
					TCActor<CPingPongActor> Ping = fg_ConstructActor<CPingPongActor>();

					DPingPongStatsReset();
					for (umint j = 0; j < nTests; ++j)
					{
						ChainTime.f_Start();
						co_await Ping(&CPingPongActor::f_Run, Head, c_nRoundTrips);
						ChainTime.f_Stop(c_nRoundTrips);
					}
					DPingPongStatsReport("Chain3", c_nRoundTrips * nTests, ChainTime);
					PerfTest.f_Add(ChainTime);
				}

				// One independent pair per core; verifies work still spreads across the pool
				{
					CTestPerformanceMeasure PairsTime("ConcurrentPairs");
					umint nPairs = fg_ConcurrencyManager().f_GetConcurrency();
					TCVector<TCActor<CPingPongActor>> Pings;
					TCVector<TCActor<CPingPongActor>> Pongs;
					for (umint i = 0; i < nPairs; ++i)
					{
						Pings.f_InsertLast(fg_ConstructActor<CPingPongActor>());
						Pongs.f_InsertLast(fg_ConstructActor<CPingPongActor>());
					}

					DPingPongStatsReset();
					for (umint j = 0; j < nTests; ++j)
					{
						PairsTime.f_Start();
						TCFutureVector<uint32> Results;
						for (umint i = 0; i < nPairs; ++i)
							Pings[i](&CPingPongActor::f_Run, Pongs[i], c_nRoundTrips) > Results;

						co_await fg_AllDoneWrapped(Results);
						PairsTime.f_Stop(c_nRoundTrips * nPairs);
					}
					DPingPongStatsReport("ConcurrentPairs", c_nRoundTrips * nPairs * nTests, PairsTime);
					PerfTest.f_Add(PairsTime);
				}

				// A pool actor bursting one CPU-bound job per core; the burst lands on the driver's
				// thread and must spread to the other cores for the measured time to scale
				{
#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
					constexpr umint c_nWorkIterations = 10'000;
#else
					constexpr umint c_nWorkIterations = 1'000'000;
#endif

					CTestPerformanceMeasure FanOutTime("FanOutBurst");
					umint nWorkers = fg_ConcurrencyManager().f_GetConcurrency();
					TCActor<CPingPongActor> Driver = fg_ConstructActor<CPingPongActor>();
					TCVector<TCActor<CPingPongActor>> Workers;
					for (umint i = 0; i < nWorkers; ++i)
						Workers.f_InsertLast(fg_ConstructActor<CPingPongActor>());

					DPingPongStatsReset();
					for (umint j = 0; j < nTests; ++j)
					{
						FanOutTime.f_Start();
						co_await Driver(&CPingPongActor::f_FanOut, Workers, c_nWorkIterations);
						FanOutTime.f_Stop(nWorkers);
					}
					DPingPongStatsReport("FanOutBurst", nWorkers * nTests, FanOutTime);
					PerfTest.f_Add(FanOutTime);
				}

				DMibExpectTrue(PerfTest);

#undef DPingPongStatsReset
#undef DPingPongStatsReport

				co_return {};
			};
		}
	};

	DMibTestRegister(CCoroutinesPerformance_Tests, Malterlib::Concurrency);
}
