// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Coroutine>
#include <Mib/Test/Performance>

#ifdef DCompiler_clang
#pragma clang diagnostic ignored "-Walways-inline-coroutine"
#endif

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


	class CCoroutinesPerformance_Tests : public NMib::NTest::CTest
	{
		constexpr static mint mc_nLoops = 16384 * 128 * 10;

		static inline_never uint32 fs_TestRecursive(uint32 _Value)
		{
			volatile uint32 Result = _Value * 2;
			return Result;
		}

		static inline_never uint32 fs_TestFunction(uint32 _Value)
		{
			volatile uint32 Return = 0;
			for (mint i = 0; i < mc_nLoops; ++i)
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
			for (mint i = 0; i < mc_nLoops; ++i)
				Return += co_await fs_TestRecursiveCoro(_Value);
			co_return Return;
		}

		void f_DoTests()
		{
			DMibTestSuite(CTestCategory("Synchronous") << CTestGroup("Performance")) -> TCFuture<void>
			{
				mint nTests = 9;

				CTestPerformanceMeasure NormalTime("Normal");
				CTestPerformanceMeasure CoroutineTime("Coroutine");
				uint32 ReturnNormal;
				uint32 ReturnCoro;
				{
					for(mint j = 0; j < nTests; ++j)
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
				}
				{
					for(mint j = 0; j < nTests; ++j)
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
				}

				DMibExpect(ReturnNormal, ==, ReturnCoro);

				CTestPerformance PerfTest(0.015);
				PerfTest.f_AddReference(NormalTime);
				PerfTest.f_Add(CoroutineTime);
				DMibExpectTrue(PerfTest);

				co_return {};
			};
			DMibTestSuite(CTestCategory("Tasks") << CTestGroup("Performance")) -> TCFuture<void>
			{
				mint nTests = 10;

				constexpr mint c_nTasks = 1'000'000;

				CTestPerformanceMeasure MalterlibTime("Malterlib5");
				TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};
				{
					for(mint j = 0; j < nTests; ++j)
					{
						MalterlibTime.f_Start();
						TCVector<TCPromise<void>> Promises;
						Promises.f_SetLen(c_nTasks);

						TCActorResultVector<void> Results;
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
								> Results.f_AddResult()
							;
						}

						bAllScheduled.f_Exchange(true);
						Event.f_SetSignaled();
						for (auto &Promise : Promises)
							Promise.f_SetResult();

						co_await Results.f_GetResults();
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
