// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	struct CThreadCount_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("EnvironmentOverride")
			{
				auto Previous = fg_GetSys()->f_GetEnvironmentVariable("MibConcurrencyThreads");
				auto Restore = g_OnScopeExit / [&]
					{
						fg_GetSys()->f_SetEnvironmentVariable("MibConcurrencyThreads", Previous);
					}
				;
				auto fCount = [](NStr::CStr const &_Override)
					{
						fg_GetSys()->f_SetEnvironmentVariable("MibConcurrencyThreads", _Override);
						EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
						CConcurrencyManager Manager(Priorities);
						umint nQueues = Manager.f_GetNumQueues(EPriority_Normal);
						Manager.f_BlockOnDestroy();

						return nQueues;
					}
				;
				umint Default = fCount("");
				umint Maximum = NSys::fg_Thread_GetVirtualCores() * 2;

				DMibExpect(fCount("0"), ==, umint(1));
				DMibExpect(fCount("-1"), ==, umint(1));
				DMibExpect(fCount("2"), ==, umint(2));
				DMibExpect(fCount(NStr::CStr::fs_ToStr(Maximum)), ==, Maximum);
				DMibExpect(fCount(NStr::CStr::fs_ToStr(Maximum + 1)), ==, Maximum);
				DMibExpect(fCount("invalid"), ==, Default);
				DMibExpect(fCount("2threads"), ==, Default);
			};
		}
	};

	DMibTestRegister(CThreadCount_Tests, Malterlib::Concurrency);
}
