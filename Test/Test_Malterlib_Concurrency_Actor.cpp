// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Performance>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>

namespace
{
	using namespace NMib;
	using namespace NMib::NPtr;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	
	class CPerformanceTestActor : public CActor
	{
		zuint32 m_Value;
	public:
		void f_AddInt(uint32 _Value)
		{
			m_Value += _Value;
		}
		uint32 f_GetResult()
		{
			return m_Value;
		}
	};

	
	class CBaseActor : public CActor
	{
	public:
		virtual uint32 f_GetValue()
		{
			return 1;
		}
		virtual uint32 f_GetSpecificValue(uint32 _Value)
		{
			return 0;
		}
	};
	class CDerivedActor : public CBaseActor
	{
	public:
		virtual uint32 f_GetValue() override
		{
			return 2;
		}
		virtual uint32 f_GetSpecificValue(uint32 _Value) override
		{
			return _Value;
		}
		
		virtual TCContinuation<void> f_Destroy() override
		{
			return TCContinuation<void>::fs_Finished(); 
		}
	};
	
	class CCallbackActor : public CActor
	{
		TCActorCallbackManager<void (int32)> m_CallbackManager;
	public:
		CCallbackActor()
			: CActor()
			, m_CallbackManager(this, false)
		{
		}
		
		TCUniquePointer<CActorCallbackReference> f_RegisterCallback(TCActor<CActor> _pActor, TCFunction<void (int32 _Value)> && _fCallback)
		{
			return m_CallbackManager.f_Register(_pActor, fg_Move(_fCallback));
		}
		
		void f_CallCallback(int32 _Value)
		{
			m_CallbackManager(_Value);
		}
	};
	class CActor_Tests : public NMib::NTest::CTest
	{
	public:
		
		void f_FunctionalTests()
		{
			DMibTestSuite("Interface")
			{
				{
					DMibTestPath("One param");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCActor<CActor> ResultActor = fg_ConstructActor<CActor>();

					CEventAutoReset FinishedEvent;

					uint32 Value = 555;
					
					TestActor(&CBaseActor::f_GetSpecificValue, 2)
						> ResultActor / [&](TCAsyncResult<uint32> && _Result0)
						{
							Value = _Result0.f_Get();
							FinishedEvent.f_Signal();
						}
					;
					FinishedEvent.f_Wait();
					
					DMibTest(DMibExpr(Value) == DMibExpr(2));
				}
				{
					DMibTestPath("Two param");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCActor<CActor> ResultActor = fg_ConstructActor<CActor>();
					CEventAutoReset FinishedEvent;
					uint32 Value = 555;
					
					TestActor(&CBaseActor::f_GetValue) 
						+ TestActor(&CBaseActor::f_GetValue)
						> ResultActor / [&](TCAsyncResult<uint32> && _Result0, TCAsyncResult<uint32> && _Result1)
						{
							Value = _Result0.f_Get() + _Result1.f_Get();
							FinishedEvent.f_Signal();
						}
					;

					FinishedEvent.f_Wait();
					
					DMibTest(DMibExpr(Value) == DMibExpr(4));
				}
				{
					DMibTestPath("Many param");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCActor<CActor> ResultActor = fg_ConstructActor<CActor>();
					CEventAutoReset FinishedEvent;
					uint32 Value = 555;
					
					TestActor(&CBaseActor::f_GetValue) 
						+ TestActor(&CBaseActor::f_GetValue)
						+ TestActor(&CBaseActor::f_GetValue)
						+ TestActor(&CBaseActor::f_GetValue)
						+ TestActor(&CBaseActor::f_GetSpecificValue, 1)
						+ TestActor(&CBaseActor::f_GetSpecificValue, 2)
						+ TestActor(&CBaseActor::f_GetSpecificValue, 3)
						+ TestActor(&CBaseActor::f_GetSpecificValue, 4)
						> TestActor / [&]
						(
							TCAsyncResult<uint32> && _Result0
							, TCAsyncResult<uint32> && _Result1
							, TCAsyncResult<uint32> && _Result2
							, TCAsyncResult<uint32> && _Result3
							, TCAsyncResult<uint32> && _Result4
							, TCAsyncResult<uint32> && _Result5
							, TCAsyncResult<uint32> && _Result6
							, TCAsyncResult<uint32> && _Result7
						)
						{
							Value 
								= _Result0.f_Get() 
								+ _Result1.f_Get()
								+ _Result2.f_Get()
								+ _Result3.f_Get()
								+ _Result4.f_Get()
								+ _Result5.f_Get()
								+ _Result6.f_Get()
								+ _Result7.f_Get()
							;
							FinishedEvent.f_Signal();
						}
					;

					FinishedEvent.f_Wait();
					
					DMibTest(DMibExpr(Value) == DMibExpr(8+1+2+3+4));
				}
			};
			
			DMibTestSuite("Inheritance")
			{
				TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
				TCActor<CActor> ResultActor = fg_ConstructActor<CActor>();
				CEventAutoReset FinishedEvent;
				uint32 Value = 555;
				TestActor(&CBaseActor::f_GetValue)
					> ResultActor / [&](TCAsyncResult<uint32> && _Result)
					{
						Value = _Result.f_Get();
						FinishedEvent.f_Signal();
					}
				;
				
				FinishedEvent.f_Wait();
				
				DMibTest(DMibExpr(Value) == DMibExpr(2));
			};
			
			DMibTestSuite("Callbacks")
			{
				TCActor<CCallbackActor> TestActor = fg_ConstructActor<CCallbackActor>();
				
				CMutual Lock;
				TCUniquePointer<CActorCallbackReference> CallbackReference;
				CEventAutoReset FinishedEvent;
				NAtomic::TCAtomic<uint32> CallbackValue;
				TestActor
					(
						&CCallbackActor::f_RegisterCallback
						, fg_ConcurrentActor()
						, [&](int32 _Value)
						{
							CallbackValue = _Value;
							FinishedEvent.f_Signal();
						}
					)
					> fg_ConcurrentActor() / [&](TCAsyncResult<TCUniquePointer<CActorCallbackReference>> && _Result)
					{
						DMibLock(Lock);
						CallbackReference = fg_Move(*_Result);
						FinishedEvent.f_Signal();
					}
				;
				FinishedEvent.f_Wait();
				
				DMibTest(DMibExpr(CallbackReference) != DMibExpr(nullptr));
				
				TestActor(&CCallbackActor::f_CallCallback, 32)
					> fg_DiscardResult()
				;

				FinishedEvent.f_Wait();
				{
					DMibLock(Lock);
				}
				
				DMibTest(DMibExpr(CallbackValue.f_Load()) == DMibExpr(32));
				
			};
			
		}
		
		void f_PerformanceTests()
		{
			DMibTestSuite(CTestCategory("Performance") << CTestGroup("Performance"))
			{
#ifdef DMibDebug
				mint nIterations = 100'000;
#else
				mint nIterations = 5'000'000;
#endif
				mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.9);
#if 0
				{
					DMibTestPath("Dispatch vector");
					CTestPerformanceMeasure DispatchMeasure("Dispatch vector");
					CPerformanceTestActor ActorEmul;
					TCVector<TCFunction<void ()>> ToDispatch;
					ToDispatch.f_SetLen(nIterations);
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(DispatchMeasure, nIterations);
						for (auto& Dispatch : ToDispatch)
						{
							Dispatch 
								= [&]
								{
									ActorEmul.f_AddInt(1);
								}
							;
						}
						for (auto& Dispatch : ToDispatch)
						{
							Dispatch();
						}
					}
					
					DMibTest(DMibExpr(ActorEmul.f_GetResult()) == DMibExpr(nIterations*5));
					PerfTest.f_AddBaseline(DispatchMeasure);
				}
#endif
				
				{
					DMibTestPath("Dispatch");
					CTestPerformanceMeasure DispatchMeasure("Dispatch");
					CPerformanceTestActor ActorEmul;
					TCLinkedList<TCFunction<void ()>> ToDispatch;
	
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(DispatchMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
						{
							ToDispatch.f_Insert
								(
									[&]
									{
										ActorEmul.f_AddInt(1);
									}
								)
							;
						}
						while (!ToDispatch.f_IsEmpty())
						{
							auto &First = ToDispatch.f_GetFirst();
							First();
							ToDispatch.f_Remove(First);
						}
					}
					
					DMibTest(DMibExpr(ActorEmul.f_GetResult()) == DMibExpr(nIterations*5));
					PerfTest.f_AddBaseline(DispatchMeasure);
				}
				{
					DMibTestPath("Dispatch threaded");
					CTestPerformanceMeasure DispatchMeasure("Dispatch threaded");
					CPerformanceTestActor ActorEmul;
					TCThreadSafeQueue<TCFunction<void ()>> ToDispatch;
	
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(DispatchMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
						{
							ToDispatch.f_Push
								(
									[&]
									{
										ActorEmul.f_AddInt(1);
									}
								)
							;
						}
						while (auto Dispatch = ToDispatch.f_Pop())
							(*Dispatch)();
					}
					
					DMibTest(DMibExpr(ActorEmul.f_GetResult()) == DMibExpr(nIterations*5));
					PerfTest.f_AddBaseline(DispatchMeasure);
				}
				{
					DMibTestPath("Actor");
					CTestPerformanceMeasure ActorMeasure("Actor->Discard");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) > fg_DiscardResult();
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
					}
					
					DMibTest(DMibExpr(Result) == DMibExpr(nIterations*5));
					
					PerfTest.f_Add(ActorMeasure);
				}
				{
					DMibTestPath("Actor With Reply To Other Actor");
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Other");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					auto &ConcurrentActor = fg_ConcurrentActor();
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations * 2);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> ConcurrentActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
					}
					
					DMibTest(DMibExpr(Result) == DMibExpr(nIterations*5));
					
					PerfTest.f_Add(ActorMeasure);
				}
				{
					DMibTestPath("Actor With Reply To Same Actor");
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Same");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations * 2);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> PerfTestActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
					}
					
					DMibTest(DMibExpr(Result) == DMibExpr(nIterations*5));
					
					PerfTest.f_Add(ActorMeasure);
				}
				{
					DMibTestPath("Parallell Concurrent Actors");
					
					CTestPerformanceMeasure ActorMeasure("Parallell Actor");
					
					uint32 Result = 0;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterations = nIterationsFull;
					mint nIterationsPerSplit = nIterations / nSplits;
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					PerfTestActors.f_SetLen(nSplits);
					for (mint i = 0; i < nSplits; ++i)
						PerfTestActors[i] = fg_ConstructActor<CPerformanceTestActor>();
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetVirtualCores());
						TCActorResultVector<void> Results;
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentActor()
								(
									&CActor::f_Dispatch
									, [&PerfTestActors, nIterationsPerSplit, pActor]
									{
										for (mint i = 0; i < nIterationsPerSplit; ++i)
											(*pActor)(&CPerformanceTestActor::f_AddInt, 1) > fg_DiscardResult();
									}
								)
								> Results.f_AddResult();
							;
						}
						Results.f_GetResults().f_CallSync();
						Result = PerfTestActors[0](&CPerformanceTestActor::f_GetResult).f_CallSync();
						for (mint iSplit = 1; iSplit < nSplits; ++iSplit)
							Result += PerfTestActors[iSplit](&CPerformanceTestActor::f_GetResult).f_CallSync();
					}
					
					DMibTest(DMibExpr(Result) == DMibExpr(nIterations*5));
					
					PerfTest.f_Add(ActorMeasure);
				}
				{
					mint nIterations = nIterationsFull / 6;
					DMibTestPath("Branched Concurrent Actor");
					CTestPerformanceMeasure ActorMeasure("Branched Concurrent Actor");
					TCFunction<TCContinuation<uint32> (uint32 _Start, uint32 _End)> Actor;
					Actor = [&Actor](uint32 _Start, uint32 _End) -> TCContinuation<uint32>
						{
							if ((_End - _Start) == 1)
								return TCContinuation<uint32>::fs_Finished(1);
							
							TCContinuation<uint32> Continuation;
							
							fg_ConcurrentActor()
								(
									&CActor::f_DispatchWithReturn<TCContinuation<uint32>>
									, [&Actor, Start = _Start, End = _Start + (_End - _Start) / 2]
									{
										return Actor(Start, End);
									}
								)
								+ fg_ConcurrentActor()
								(
									&CActor::f_DispatchWithReturn<TCContinuation<uint32>>
									, [&Actor, Start = _Start + (_End - _Start) / 2, End = _End]
									{
										return Actor(Start, End);
									}
								)
								> fg_ConcurrentActor() / [Continuation](TCAsyncResult<uint32> &&_Result0, TCAsyncResult<uint32> &&_Result1)
								{
									Continuation.f_SetResult(*_Result0 + *_Result1);
								}
							;
							return Continuation;
						}
					;
					
					uint32 Result = 0;
					
					auto ConcurrentActor = fg_ConcurrentActor();

					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, ((nIterations * 2 - 1) + nIterations - 1) * 2, NSys::fg_Thread_GetVirtualCores());
						
						Result += fg_ConcurrentActor()
							(
								&CActor::f_DispatchWithReturn<TCContinuation<uint32>>
								, [&]
								{
									return Actor(0, nIterations);
								}
							).f_CallSync(20.0);
						;
					}
					
					DMibTest(DMibExpr(Result) == DMibExpr(nIterations*5));
					
					PerfTest.f_Add(ActorMeasure);
				}
				
				DMibTest(DMibExpr(PerfTest));
			};
		}
		
		void f_DoTests()
		{
			f_FunctionalTests();
			f_PerformanceTests();		
		}
	};

	
	DMibTestRegister(CActor_Tests, Malterlib::Concurrency);
}
