// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>

#ifdef DPlatformFamily_Windows
#	include <Windows.h>
#else
#	include <dirent.h>
#endif

namespace
{
	using namespace NMib;
	using namespace NMib::NConcurrency;
	using namespace NMib::NStr;

	// Runs the functor on one named pool queue and blocks until it has run, so the test can act as
	// that queue's thread for the calls that only it is allowed to make
	template <typename tf_CFunctor>
	void fg_RunOnQueue(EPriority _Priority, umint _iQueue, tf_CFunctor const &_fToRun)
	{
		NThread::CEvent Done;
		Done.f_ResetSignaled();

		fg_ConcurrencyManager().f_DispatchToQueue
			(
				_Priority
				, _iQueue
				, [&](auto && ...)
				{
					_fToRun();
					Done.f_SetSignaled();
				}
			)
		;

		Done.f_Wait();
	}

	struct CQueueProbeActor : public CActor
	{
		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;

		TCFuture<umint> f_WhichQueue()
		{
			co_return fg_ConcurrencyManager().f_GetQueue();
		}
	};

	struct CThreadIoLoop_Tests : public NMib::NTest::CTest
	{
		void f_DoTests()
		{
			auto &ConcurrencyManager = fg_ConcurrencyManager();
			constexpr EPriority c_Priority = EPriority_NormalHighCPU;

			DMibTestSuite("DispatchToQueue")
			{
				umint nQueues = ConcurrencyManager.f_GetNumQueues(c_Priority);
				DMibExpectTrue(nQueues > 0);

				// Every queue must be reachable by name, including the last one
				for (umint iQueue : {umint(0), nQueues - 1})
				{
					DMibTestPath("Queue {}"_f << iQueue);

					umint iRanOn = TCLimitsInt<umint>::mc_Max;
					EPriority RanAtPriority = EPriority_Max;

					fg_RunOnQueue
						(
							c_Priority
							, iQueue
							, [&]
							{
								iRanOn = fg_ConcurrencyManager().f_GetQueue();
								RanAtPriority = fg_ConcurrencyManager().f_GetQueuePriority();
							}
						)
					;

					DMibExpect(iRanOn, ==, iQueue);
					DMibExpect(RanAtPriority, ==, c_Priority);
				}
			};

			DMibTestSuite("ManagerOwnedLoops")
			{
				// The loops are startup constants owned by the manager; a queue either got one
				// at manager construction or never will. Platforms without loops skip here
				if (!ConcurrencyManager.f_GetQueueIoLoop(c_Priority, 0))
					return;

				umint nQueues = ConcurrencyManager.f_GetNumQueues(c_Priority);

				// The same knob the manager reads: MalterlibIoLoops caps the loop count. Builds
				// without the io debugging overrides ignore the environment, and so must the
				// expectation
#if DMibConfig_IoDebug_Enable
				smint nConfiguredLoops = NSys::fg_Process_GetEnvironmentVariable_NonProtected(NStr::gc_Str<"MalterlibIoLoops">.m_Str).f_ToInt(smint(-1));
#else
				smint nConfiguredLoops = -1;
#endif

				umint nExpectedLoops = nConfiguredLoops < 0 ? nQueues : fg_Min(umint(nConfiguredLoops), nQueues);
				for (umint iQueue = 0; iQueue < nExpectedLoops; ++iQueue)
				{
					DMibTestPath("Loop {}"_f << iQueue);

					DMibExpectTrue(ConcurrencyManager.f_GetQueueIoLoop(c_Priority, iQueue) != nullptr);
				}

				// Before the enable message is processed the thread parks on its event and
				// reports no loop; afterwards it parks in the loop and reports it
				constexpr umint c_iQueue = 0;
				ConcurrencyManager.f_EnableQueueIoLoop(c_Priority, c_iQueue);

				bool bActive = false;
				fg_RunOnQueue
					(
						c_Priority
						, c_iQueue
						, [&]
						{
							bActive = fg_ConcurrencyManager().f_GetThreadIoLoop() != nullptr;
						}
					)
				;
				DMibExpectTrue(bActive);

				// Enabling is idempotent, and a queue with an enabled loop still runs plain jobs:
				// the signal path reaches a loop parked thread through the event or the wake
				ConcurrencyManager.f_EnableQueueIoLoop(c_Priority, c_iQueue);

				for (umint iRepetition = 0; iRepetition < 64; ++iRepetition)
				{
					DMibTestPath("Job {}"_f << iRepetition);

					umint iRanOn = TCLimitsInt<umint>::mc_Max;
					fg_RunOnQueue(c_Priority, c_iQueue, [&] { iRanOn = fg_ConcurrencyManager().f_GetQueue(); });
					DMibExpect(iRanOn, ==, c_iQueue);
				}
			};

			DMibTestSuite("CreateScopeNesting")
			{
				CIoLoopBinding OuterBinding = ConcurrencyManager.f_PickIoLoopBinding(c_Priority);
				CIoLoopBinding InnerBinding = ConcurrencyManager.f_PickIoLoopBinding(c_Priority);
				if (!OuterBinding.m_pLoop || !InnerBinding.m_pLoop)
					return; // Platforms without loops have nothing to scope

				// A scope must hand back whatever binding was in effect when it opened, so an
				// inner scope cannot strand the outer one
				{
					CIoLoopCreateScope Outer(OuterBinding);
					{
						DMibTestPath("Outer open");

						DMibExpectTrue(NSys::fg_GetThreadIoLoop() == OuterBinding.m_pLoop);
					}

					{
						DMibTestPath("Inner open");

						CIoLoopCreateScope Inner(InnerBinding);
						DMibExpectTrue(NSys::fg_GetThreadIoLoop() == InnerBinding.m_pLoop);
					}

					{
						DMibTestPath("Inner closed");

						DMibExpectTrue(NSys::fg_GetThreadIoLoop() == OuterBinding.m_pLoop);
					}
				}

				DMibExpectTrue(NSys::fg_GetThreadIoLoop() == nullptr);
			};

			DMibTestSuite("ManagerLoopLifetime")
			{
				// Loops are created at manager construction and destroyed at stop, so a manager
				// lifetime must return the process to its descriptor baseline: every loop holds a
				// wake pipe plus a poll descriptor (a completion port and its AFD handles on
				// Windows), and leaking them once per manager exhausts the process on repeated
				// construction
				auto fCountOpenDescriptors = []() -> umint
					{
#ifdef DPlatformFamily_Windows
						DWORD nHandles = 0;
						if (!GetProcessHandleCount(GetCurrentProcess(), &nHandles))
							return 0;

						return nHandles;
#else
						DIR *pDescriptorDir = opendir("/dev/fd");
						if (!pDescriptorDir)
							return 0;

						umint nDescriptors = 0;
						while (readdir(pDescriptorDir))
							++nDescriptors;
						closedir(pDescriptorDir);

						return nDescriptors;
#endif
					}
				;

				umint nDescriptorsBefore = fCountOpenDescriptors();

				// A live process always has descriptors open; zero means the enumeration itself is
				// unavailable, and continuing would compare 0 == 0 and vacuously pass the leak check
				DMibExpectTrue(nDescriptorsBefore > 0);

				{
					EExecutionPriority ExecutionPriority[EPriority_Max]
#ifdef DPlatformFamily_macOS
						= {EExecutionPriority_BelowNormal, EExecutionPriority_Normal, EExecutionPriority_Normal}
#else
						= {EExecutionPriority_Lowest, EExecutionPriority_Normal, EExecutionPriority_Normal}
#endif
					;

					CConcurrencyManager LocalManager(ExecutionPriority);

					// A binding picked without its enable message ever running exercises the
					// stop path that claims and drains unclaimed loops before destroying them
					LocalManager.f_PickIoLoopBinding(EPriority_NormalHighCPU);

					LocalManager.f_BlockOnDestroy();
				}
				umint nDescriptorsAfter = fCountOpenDescriptors();

				// Concurrent tests open and close descriptors of their own, so exact equality is
				// not expectable; a loop leak is dozens to hundreds on any multicore machine
				DMibExpectTrue(nDescriptorsAfter <= nDescriptorsBefore + 16);
			};
		}
	};

	DMibTestRegister(CThreadIoLoop_Tests, Malterlib::Concurrency);
}
