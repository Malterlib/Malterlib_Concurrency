// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/CommandLine/Console>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;

	enum class ECompletionExit
	{
		mc_Return
		, mc_Throw
		, mc_ReturnException
	};

	enum class ECompletionProbePlace
	{
		mc_Parameter
		, mc_Local
		, mc_Capture
	};

	struct CCompletionProbeState
	{
		NAtomic::TCAtomic<umint> m_NextOrder = 0;
		NAtomic::TCAtomic<umint> m_ParameterOrder = 0;
		NAtomic::TCAtomic<umint> m_LocalOrder = 0;
		NAtomic::TCAtomic<umint> m_CaptureOrder = 0;
	};

	struct CCompletionProbe
	{
		CCompletionProbe(TCSharedPointer<CCompletionProbeState> const &_pState, ECompletionProbePlace _Place)
			: m_pState(_pState)
			, m_Place(_Place)
		{
		}

		CCompletionProbe(CCompletionProbe &&) = default;
		CCompletionProbe(CCompletionProbe const &) = delete;

		~CCompletionProbe()
		{
			if (!m_pState)
				return;

			auto Order = m_pState->m_NextOrder.f_FetchAdd(1) + 1;
			switch (m_Place)
			{
			case ECompletionProbePlace::mc_Parameter:
				m_pState->m_ParameterOrder.f_Store(Order);

				break;
			case ECompletionProbePlace::mc_Local:
				m_pState->m_LocalOrder.f_Store(Order);

				break;
			case ECompletionProbePlace::mc_Capture:
				m_pState->m_CaptureOrder.f_Store(Order);

				break;
			}
		}

		TCSharedPointer<CCompletionProbeState> m_pState;
		ECompletionProbePlace m_Place;
	};

	TCFuture<uint32> fg_ProbeCoroutineCompletion(CCompletionProbe _Parameter, TCFuture<void> _Gate, TCPromise<void> _Started, ECompletionExit _Exit)
	{
		co_await ECoroutineFlag_CaptureExceptions;

		CCompletionProbe Local(_Parameter.m_pState, ECompletionProbePlace::mc_Local);
		_Started.f_SetResult();
		if (_Gate.f_IsValid())
			co_await fg_Move(_Gate);

		if (_Exit == ECompletionExit::mc_Throw)
			DMibError("Coroutine completion probe");
		if (_Exit == ECompletionExit::mc_ReturnException)
			co_return DMibErrorInstance("Coroutine completion probe");

		co_return 42;
	}

	struct CCompletionObservation
	{
		umint m_ResultOrder;
		umint m_ParameterAtResult;
		umint m_LocalAtResult;
		umint m_CaptureAtResult;
		uint32 m_Value = 0;
		CStr m_Error;
	};

	struct CCoroutineCompletion_Tests : NMib::NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("Ordering")
			{
				CActorRunLoopTestHelper Helper;
				TCActor<CDispatchingActor> Producer{fg_Construct(), Helper.m_pRunLoop->f_Dispatcher()};
				auto DestroyProducer = g_OnScopeExit / [&]
					{
						Producer->f_BlockDestroy(Helper.m_pRunLoop->f_ActorDestroyLoop());
					}
				;

				for (auto Exit : {ECompletionExit::mc_Return, ECompletionExit::mc_Throw, ECompletionExit::mc_ReturnException})
				{
					for (bool bSuspend : {false, true})
					{
						for (bool bCapture : {false, true})
						{
							CStr Name = "{}/{}/{}"_f << (Exit == ECompletionExit::mc_Return ? "co_return" : Exit == ECompletionExit::mc_Throw ? "throw" : "co_return_exception")
								<< (bSuspend ? "suspended" : "immediate") << (bCapture ? "dispatch_capture" : "bound_call")
							;
							DMibTestPath(Name);
							auto pState = TCSharedPointer<CCompletionProbeState>(fg_Construct());
							TCPromise<void> Started;
							TCPromise<void> Gate;
							TCPromise<CCompletionObservation> Finished;
							TCFuture<uint32> Result;
							TCFuture<void> GateFuture = bSuspend ? Gate.f_Future() : TCFuture<void>();

							if (bCapture)
							{
								Result = fg_Dispatch
									(
										Producer
										, [Capture = CCompletionProbe(pState, ECompletionProbePlace::mc_Capture)
											, Parameter = CCompletionProbe(pState, ECompletionProbePlace::mc_Parameter)
											, GateFuture = fg_Move(GateFuture), Started, Exit]() mutable -> TCFuture<uint32>
										{
											return fg_ProbeCoroutineCompletion(fg_Move(Parameter), fg_Move(GateFuture), fg_Move(Started), Exit);
										}
									)
								;
							}
							else
							{
								Result = Producer.f_Bind<fg_ProbeCoroutineCompletion>
									(
										CCompletionProbe(pState, ECompletionProbePlace::mc_Parameter), fg_Move(GateFuture), Started, Exit
									).f_Call()
								;
							}

							Result.f_OnResultSet
								(
									[pState, Finished](TCAsyncResult<uint32> &&_Result)
									{
										CCompletionObservation Observation;
										Observation.m_ResultOrder = pState->m_NextOrder.f_FetchAdd(1) + 1;
										Observation.m_ParameterAtResult = pState->m_ParameterOrder.f_Load();
										Observation.m_LocalAtResult = pState->m_LocalOrder.f_Load();
										Observation.m_CaptureAtResult = pState->m_CaptureOrder.f_Load();
										if (_Result)
											Observation.m_Value = *_Result;
										else
											Observation.m_Error = _Result.f_GetExceptionStr();

										Finished.f_SetResult(fg_Move(Observation));
									}
								)
							;

							Started.f_Future().f_CallSync(Helper.m_pRunLoop, 30.0);
							if (bSuspend)
								Gate.f_SetResult();
							auto Observation = Finished.f_Future().f_CallSync(Helper.m_pRunLoop, 30.0);
							fg_Dispatch(Producer, [] {}).f_CallSync(Helper.m_pRunLoop, 30.0);

							DMibExpect(Observation.m_LocalAtResult, >, 0u);
							DMibExpect(pState->m_ParameterOrder.f_Load(), >, 0u);
							if (bCapture)
								DMibExpect(pState->m_CaptureOrder.f_Load(), >, 0u);
							if (Exit == ECompletionExit::mc_Return)
							{
								DMibExpect(Observation.m_Value, ==, 42u);
								DMibExpectTrue(Observation.m_Error.f_IsEmpty());
							}
							else
								DMibExpect(Observation.m_Error.f_Find("Coroutine completion probe"), >=, 0);
						}
					}
				}
			};
		}
	};

	DMibTestRegister(CCoroutineCompletion_Tests, Malterlib::Concurrency);
}
