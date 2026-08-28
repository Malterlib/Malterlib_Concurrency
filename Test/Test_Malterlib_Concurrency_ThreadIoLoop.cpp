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

				umint QueuesToVisit[2] = {0, nQueues - 1};
				umint nQueuesToVisit = nQueues > 1 ? 2 : 1;
				for (umint iVisit = 0; iVisit < nQueuesToVisit; ++iVisit)
				{
					umint iQueue = QueuesToVisit[iVisit];
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
				if (!ConcurrencyManager.f_GetQueueIoLoop(c_Priority, 0))
					return;

				umint nQueues = ConcurrencyManager.f_GetNumQueues(c_Priority);

				// Match the manager: non-debug builds ignore the environment override.
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

				// Zero means enumeration is unavailable, which would make a 0 == 0 leak check vacuous.
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

					// Leave an enable queued to exercise stop before ownership is claimed.
					LocalManager.f_PickIoLoopBinding(EPriority_NormalHighCPU);

					LocalManager.f_BlockOnDestroy();
				}
				umint nDescriptorsAfter = fCountOpenDescriptors();

				// Concurrent suites can change the descriptor count; allow bounded noise.
				DMibExpectTrue(nDescriptorsAfter <= nDescriptorsBefore + 16);
			};
		}
	};

	DMibTestRegister(CThreadIoLoop_Tests, Malterlib::Concurrency);
}
