// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	namespace
	{
		constexpr umint gc_NestedSingletonDepth = 32;

		template <umint t_Depth>
		struct TCNestedSingletonActor : CActor
		{
		};

		// Each singleton asks for the next one while it is created. A type asked for the first time gets a new singleton
		// index, so the manager grows its singleton storage while the slots of the outer singletons are being filled
		template <umint t_Depth>
		TCWrapped<TCActor<TCNestedSingletonActor<t_Depth>>> fg_GetNestedSingleton(CConcurrencyManager &_Manager, umint &o_nCreated)
		{
			return _Manager.f_GetSingleton<TCNestedSingletonActor<t_Depth>>
				(
					[&]
					{
						if constexpr (t_Depth + 1 < gc_NestedSingletonDepth)
						{
							TCActor<TCNestedSingletonActor<t_Depth + 1>> Next = fg_GetNestedSingleton<t_Depth + 1>(_Manager, o_nCreated).f_Move();
						}

						++o_nCreated;
						return _Manager.f_ConstructActor(fg_Construct<TCNestedSingletonActor<t_Depth>>());
					}
				)
			;
		}
	}

	struct CSingleton_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("General")
			{
				DMibTestCategory("NestedCreation")
				{
					EExecutionPriority Priorities[EPriority_Max] = {EExecutionPriority_Normal, EExecutionPriority_Normal, EExecutionPriority_Normal};
					CConcurrencyManager Manager(Priorities);

					{
						umint nCreated = 0;

						TCActor<TCNestedSingletonActor<0>> Outer = fg_GetNestedSingleton<0>(Manager, nCreated).f_Move();
						{
							DMibTestPath("FirstRequest");
							DMibExpect(nCreated, ==, gc_NestedSingletonDepth);
						}

						TCActor<TCNestedSingletonActor<0>> OuterAgain = fg_GetNestedSingleton<0>(Manager, nCreated).f_Move();
						TCActor<TCNestedSingletonActor<gc_NestedSingletonDepth - 1>> Innermost
							= fg_GetNestedSingleton<gc_NestedSingletonDepth - 1>(Manager, nCreated).f_Move()
						;

						{
							DMibTestPath("LaterRequests");
							DMibExpect(nCreated, ==, gc_NestedSingletonDepth);
						}

						DMibExpectTrue(Outer == OuterAgain);
						DMibExpectTrue(bool(Innermost));
					}

					Manager.f_BlockOnDestroy();
				};
			};
		}
	};

	DMibTestRegister(CSingleton_Tests, Malterlib::Concurrency);
}
