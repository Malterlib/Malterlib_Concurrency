// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Cryptography/RandomID>
#include <Mib/File/File>

using namespace NMib;
using namespace NMib::NStorage;
using namespace NMib::NContainer;
using namespace NMib::NConcurrency;
using namespace NMib::NFunction;
using namespace NMib::NStr;
using namespace NMib::NAtomic;
using namespace NMib::NTest;
using namespace NMib::NNetwork;

#if DMibConfig_Concurrency_DebugSubscriptions
	static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier * 10.0;
#else
	static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;
#endif

namespace NDistributedActorPriorityTests
{
	// Simple struct to hold call order info - must be outside test class for stream operators
	struct CCallOrderEntry
	{
		uint32 m_CallID;
		uint8 m_Priority;

		template <typename t_CStream>
		void f_Stream(t_CStream &_Stream)
		{
			_Stream % m_CallID;
			_Stream % m_Priority;
		}
	};

	class CDistributedActorPriority_Tests : public NMib::NTest::CTest
	{
	public:
		// Test actor that can report the priority of incoming calls
		struct CPriorityTestActor : public CActor
		{
			enum : uint32
			{
				EProtocolVersion_Min = 0x10B // Priority support version
				, EProtocolVersion_Current = 0x10B
			};

			CPriorityTestActor()
			{
				DMibPublishActorFunction(CPriorityTestActor::f_GetCallPriority);
				DMibPublishActorFunction(CPriorityTestActor::f_TestWithPriority);
				DMibPublishActorFunction(CPriorityTestActor::f_RecordCallOrder);
				DMibPublishActorFunction(CPriorityTestActor::f_GetCallOrder);
				DMibPublishActorFunction(CPriorityTestActor::f_ClearCallOrder);
			}

			// Returns the priority of the current incoming call
			uint8 f_GetCallPriority()
			{
				return fg_GetCallingHostInfo().f_GetCallPriority();
			}

			// Simple test function that returns a value
			uint32 f_TestWithPriority()
			{
				return 5;
			}

			// Record a call with its ID and priority for order verification
			void f_RecordCallOrder(uint32 _CallID)
			{
				m_CallOrder.f_Insert(CCallOrderEntry{_CallID, fg_GetCallingHostInfo().f_GetCallPriority()});
			}

			// Get the recorded call order
			TCVector<CCallOrderEntry> f_GetCallOrder()
			{
				return m_CallOrder;
			}

			// Clear the call order
			void f_ClearCallOrder()
			{
				m_CallOrder.f_Clear();
			}

		private:
			TCVector<CCallOrderEntry> m_CallOrder;
		};

	public:
		void fp_CPriorityScopeTests()
		{
			DMibTestSuite("CPriorityScope")
			{
				{
					DMibTestPath("Default priority is 128");
					CActorRunLoopTestHelper RunLoopHelper;
					DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(128));
				}
				{
					DMibTestPath("CPriorityScope sets high priority");
					CActorRunLoopTestHelper RunLoopHelper;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(64));
					}
					DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(128));
				}
				{
					DMibTestPath("CPriorityScope sets low priority");
					CActorRunLoopTestHelper RunLoopHelper;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(192));
					}
					DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(128));
				}
				{
					DMibTestPath("Nested priority scopes");
					CActorRunLoopTestHelper RunLoopHelper;

					// Before any scope
					uint8 PriorityBefore = CPriorityScope::fs_GetCurrentPriority();
					DMibExpect(PriorityBefore, ==, uint8(128));

					uint8 PriorityOuter, PriorityInner, PriorityAfterInner, PriorityAfterOuter;
					{
						CPriorityScope Outer(CPriorityScope::mc_PriorityHigh);
						PriorityOuter = CPriorityScope::fs_GetCurrentPriority();
						{
							CPriorityScope Inner(CPriorityScope::mc_PriorityLow);
							PriorityInner = CPriorityScope::fs_GetCurrentPriority();
						}
						PriorityAfterInner = CPriorityScope::fs_GetCurrentPriority();
					}
					PriorityAfterOuter = CPriorityScope::fs_GetCurrentPriority();

					DMibExpect(PriorityOuter, ==, uint8(64));
					DMibExpect(PriorityInner, ==, uint8(192));
					DMibExpect(PriorityAfterInner, ==, uint8(64));
					DMibExpect(PriorityAfterOuter, ==, uint8(128));
				}
				{
					DMibTestPath("Custom priority value");
					CActorRunLoopTestHelper RunLoopHelper;
					{
						CPriorityScope CustomPriority(42);
						DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(42));
					}
					DMibExpect(CPriorityScope::fs_GetCurrentPriority(), ==, uint8(128));
				}
			};
		}

		void fp_RemoteCallPriorityTests()
		{
			DMibTestSuite("Remote Calls")
			{
				CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorPriorityRemote";

				{
					DMibTestPath("Remote call with default priority");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Call without priority scope - should receive default 128
					uint8 Priority = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					DMibExpect(Priority, ==, uint8(128));
				}
				{
					DMibTestPath("Remote call with high priority");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Call with high priority scope
					uint8 Priority;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						Priority = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					}
					DMibExpect(Priority, ==, uint8(64));
				}
				{
					DMibTestPath("Remote call with low priority");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Call with low priority scope
					uint8 Priority;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						Priority = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					}
					DMibExpect(Priority, ==, uint8(192));
				}
				{
					DMibTestPath("Concurrent calls with different priorities");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Launch calls with different priorities concurrently
					TCFuture<uint8> FutureHigh;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					TCFuture<uint8> FutureNormal;
					FutureNormal = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();

					TCFuture<uint8> FutureLow;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						FutureLow = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					// All calls should return their respective priorities
					DMibExpect(fg_Move(FutureHigh).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(64));
					DMibExpect(fg_Move(FutureNormal).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(128));
					DMibExpect(fg_Move(FutureLow).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(192));
				}
				{
					DMibTestPath("Custom priority value remote call");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Call with custom priority value
					uint8 Priority;
					{
						CPriorityScope CustomPriority(17);
						Priority = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					}
					DMibExpect(Priority, ==, uint8(17));
				}
			};
		}

		void fp_ReconnectRecoveryTests()
		{
			DMibTestSuite("Reconnect Recovery")
			{
				CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorPriorityReconnect";

				{
					DMibTestPath("High priority packet recovery after disconnect");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init(true); // Enable reconnect

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Queue packet that can't be received
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_StopProcessingReceive);

					TCFuture<uint8> FutureHigh;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					// Force reconnect
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends | ESocketDebugFlag_DelayClose);

					// Should complete with correct priority after recovery
					DMibExpect(fg_Move(FutureHigh).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(64));
				}
				{
					DMibTestPath("Multiple priority queue recovery after disconnect");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init(true); // Enable reconnect

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Queue packets with different priorities that can't be received
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_StopProcessingReceive);

					TCFuture<uint8> FutureHigh;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					TCFuture<uint8> FutureNormal = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();

					TCFuture<uint8> FutureLow;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						FutureLow = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					// Force reconnect
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends | ESocketDebugFlag_DelayClose);

					// All should complete with correct priorities after recovery
					DMibExpect(fg_Move(FutureHigh).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(64));
					DMibExpect(fg_Move(FutureNormal).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(128));
					DMibExpect(fg_Move(FutureLow).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(192));
				}
				{
					DMibTestPath("Interleaved priority calls recovery");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init(true); // Enable reconnect

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Clear any previous call order
					Actor.f_CallActor(&CPriorityTestActor::f_ClearCallOrder)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

					// Queue interleaved priority calls
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_StopProcessingReceive);

					TCFuture<void> FutureHigh1;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh1 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(1);
					}

					TCFuture<void> FutureNormal1 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(2);

					TCFuture<void> FutureLow1;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						FutureLow1 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(3);
					}

					TCFuture<void> FutureHigh2;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh2 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(4);
					}

					TCFuture<void> FutureNormal2 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(5);

					TCFuture<void> FutureLow2;
					{
						CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
						FutureLow2 = Actor.f_CallActor(&CPriorityTestActor::f_RecordCallOrder)(6);
					}

					// Force reconnect
					TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends | ESocketDebugFlag_DelayClose);

					// Wait for all calls to complete
					fg_Move(FutureHigh1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					fg_Move(FutureNormal1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					fg_Move(FutureLow1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					fg_Move(FutureHigh2).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					fg_Move(FutureNormal2).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					fg_Move(FutureLow2).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

					// Get call order and verify priorities are correct
					auto CallOrder = Actor.f_CallActor(&CPriorityTestActor::f_GetCallOrder)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

					DMibExpect(CallOrder.f_GetLen(), ==, mint(6));

					// Verify each call recorded the correct priority
					for (auto const &Call : CallOrder)
					{
						uint32 CallID = Call.m_CallID;
						uint8 Priority = Call.m_Priority;

						if (CallID == 1 || CallID == 4)
							DMibExpect(Priority, ==, uint8(64))(ETestFlag_Aggregated); // High priority calls
						else if (CallID == 2 || CallID == 5)
							DMibExpect(Priority, ==, uint8(128))(ETestFlag_Aggregated); // Normal priority calls
						else if (CallID == 3 || CallID == 6)
							DMibExpect(Priority, ==, uint8(192))(ETestFlag_Aggregated); // Low priority calls
					}
				}
				{
					DMibTestPath("Server connection break with priority recovery");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init(true); // Enable reconnect

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Break server connection first - server will reconnect when trying to send the reply
					TestState.f_BreakServerConnections(fp64::fs_Inf(), ESocketDebugFlag_FailSends | ESocketDebugFlag_DelayClose);

					// Make a high priority call - server will reconnect when trying to reply
					TCFuture<uint8> FutureHigh;
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
						FutureHigh = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)();
					}

					// Should complete with correct priority after server reconnects
					DMibExpect(fg_Move(FutureHigh).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), ==, uint8(64));
				}
			};
		}

		void fp_MixedPriorityTests()
		{
			DMibTestSuite("Mixed Priority Scenarios")
			{
				CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorPriorityMixed";

				{
					DMibTestPath("Priority scope across multiple calls");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Make multiple calls within same priority scope
					{
						CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);

						uint8 Priority1 = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
						uint8 Priority2 = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
						uint8 Priority3 = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

						DMibExpect(Priority1, ==, uint8(64));
						DMibExpect(Priority2, ==, uint8(64));
						DMibExpect(Priority3, ==, uint8(64));
					}
				}
				{
					DMibTestPath("Rapid priority switching");
					CActorRunLoopTestHelper RunLoopHelper;

					CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
					TestState.f_SeparateServerManager();
					TestState.f_Init();

					auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CPriorityTestActor>();
					TestState.f_Publish<CPriorityTestActor>(LocalActor, "Test");
					CStr SubscriptionID = TestState.f_Subscribe("Test");
					auto Actor = TestState.f_GetRemoteActor<CPriorityTestActor>(SubscriptionID);

					// Rapid switching between priorities
					for (mint i = 0; i < 5; ++i)
					{
						uint8 PriorityHigh, PriorityLow;
						{
							CPriorityScope HighPriority(CPriorityScope::mc_PriorityHigh);
							PriorityHigh = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
						}
						{
							CPriorityScope LowPriority(CPriorityScope::mc_PriorityLow);
							PriorityLow = Actor.f_CallActor(&CPriorityTestActor::f_GetCallPriority)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
						}

						DMibExpect(PriorityHigh, ==, uint8(64))(ETestFlag_Aggregated);
						DMibExpect(PriorityLow, ==, uint8(192))(ETestFlag_Aggregated);
					}
				}
			};
		}

		void f_DoTests()
		{
			DMibTestCategory("Distributed Actor Priority")
			{
				fp_CPriorityScopeTests();
				fp_RemoteCallPriorityTests();
				fp_ReconnectRecoveryTests();
				fp_MixedPriorityTests();
			};
		}
	};

	DMibTestRegister(CDistributedActorPriority_Tests, Malterlib::Concurrency::DistributedActorPriority);
}
