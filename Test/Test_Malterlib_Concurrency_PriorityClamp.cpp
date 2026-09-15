// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	struct CClampProbeActor : CActor
	{
		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;

		CClampProbeActor(NAtomic::TCAtomic<umint> *_pPriority)
			: mp_pPriority(_pPriority)
		{
		}

		void fp_Construct() override
		{
			mp_pPriority->f_Store(self.m_pThis->f_GetPriority());
		}

		NAtomic::TCAtomic<umint> *mp_pPriority = nullptr;
	};

	struct CPriorityClamp_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("General")
			{
				DMibTestCategory("ClampRaisesLowerBands")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities, EPriority_Normal);

					DMibExpect(Manager.f_ClampPriority(EPriority_Low), ==, EPriority_Normal);
					DMibExpect(Manager.f_ClampPriority(EPriority_NormalHighCPU), ==, EPriority_Normal);
					DMibExpect(Manager.f_ClampPriority(EPriority_Normal), ==, EPriority_Normal);

					Manager.f_BlockOnDestroy();
				};

				DMibTestCategory("UnclampedKeepsDeclaredBands")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities);

					DMibExpect(Manager.f_ClampPriority(EPriority_Low), ==, EPriority_Low);
					DMibExpect(Manager.f_ClampPriority(EPriority_NormalHighCPU), ==, EPriority_NormalHighCPU);
					DMibExpect(Manager.f_ClampPriority(EPriority_Normal), ==, EPriority_Normal);

					Manager.f_BlockOnDestroy();
				};

				DMibTestCategory("PartialClampLeavesHigherBands")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities, EPriority_NormalHighCPU);

					DMibExpect(Manager.f_ClampPriority(EPriority_Low), ==, EPriority_NormalHighCPU);
					DMibExpect(Manager.f_ClampPriority(EPriority_Normal), ==, EPriority_Normal);

					Manager.f_BlockOnDestroy();
				};

				DMibTestCategory("ActorCreationTakesTheClampedBand")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					NAtomic::TCAtomic<umint> Priority = EPriority_Max;
					CConcurrencyManager Manager(Priorities, EPriority_Normal);

					{
						auto Actor = Manager.f_ConstructActor(fg_Construct<CClampProbeActor>(&Priority));
						fg_Move(Actor).f_Destroy().f_CallSync();
					}

					DMibExpect(Priority.f_Load(), ==, umint(EPriority_Normal));

					Manager.f_BlockOnDestroy();
				};

				DMibTestCategory("ActorCreationKeepsItsBandUnclamped")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					NAtomic::TCAtomic<umint> Priority = EPriority_Max;
					CConcurrencyManager Manager(Priorities);

					{
						auto Actor = Manager.f_ConstructActor(fg_Construct<CClampProbeActor>(&Priority));
						fg_Move(Actor).f_Destroy().f_CallSync();
					}

					DMibExpect(Priority.f_Load(), ==, umint(EPriority_NormalHighCPU));

					Manager.f_BlockOnDestroy();
				};

				DMibTestCategory("ForeignTimersFollowTheClamp")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Source(Priorities);
					CConcurrencyManager Destination(Priorities, EPriority_Normal);
					NAtomic::TCAtomic<umint> Priority = EPriority_Max;
					auto Actor = Source.f_ConstructActor(fg_Construct<CClampProbeActor>(&Priority));

					DMibExpect(Destination.f_GetTimerActor(Actor)->f_GetPriority(), ==, EPriority_Normal);

					TCPromise<EPriority> QueuePriority;
					Source.f_DispatchToQueue
						(
							EPriority_NormalHighCPU, 0
							, [&Destination, QueuePriority](CConcurrencyThreadLocal &)
							{
								QueuePriority.f_SetResult(Destination.f_GetTimerActor()->f_GetPriority());
							}
						)
					;
					DMibExpect(QueuePriority.f_Future().f_CallSync(), ==, EPriority_Normal);

					fg_Move(Actor).f_Destroy().f_CallSync();
					Destination.f_BlockOnDestroy();
					Source.f_BlockOnDestroy();
				};

				DMibTestCategory("IoLoopBindingFollowsTheClamp")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities, EPriority_Normal);

					auto Binding = Manager.f_PickIoLoopBinding(EPriority_NormalHighCPU);
					if (Binding)
						DMibExpect(Binding.m_Priority, ==, EPriority_Normal);

					Manager.f_BlockOnDestroy();
				};
			};
		}
	};

	DMibTestRegister(CPriorityClamp_Tests, Malterlib::Concurrency);
}
