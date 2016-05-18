// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Performance>
#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>

#if 0
#include <numeric>
#include <future>
#endif

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

	class CExceptionActor : public CActor
	{
	public:
		virtual TCContinuation<uint32> f_GetError()
		{
			return DMibErrorInstance("Error");
		}
		virtual TCContinuation<uint32> f_GetNoError()
		{
			return fg_Explicit(5);
		}
		virtual TCContinuation<void> f_GetErrorVoid()
		{
			return DMibErrorInstance("Error");
		}
		virtual TCContinuation<void> f_GetNoErrorVoid()
		{
			return fg_Explicit();
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
		void fp_BlockOnAllThreads()
		{
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
			TCActorResultVector<void> Results{nThreads};
			for (mint i = 0; i < nThreads; ++i)
			{
				fg_ConcurrentActor()
					(
						&CActor::f_DispatchWithReturn<void>
						, []
						{
							auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
					)
					> Results.f_AddResult()
				;
			}
			Results.f_GetResults().f_CallSync();
			auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
			Checkout.f_CheckMessages(); // Garbage collect memory
		}
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
					
					DMibExpect(Value, ==, 2);
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
					
					DMibExpect(Value, ==, 4);
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
					
					DMibExpect(Value, ==, 8+1+2+3+4);
				}
				{
					DMibTestPath("Dispatch");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCActor<CActor> ResultActor = fg_ConstructActor<CActor>();
					CEventAutoReset FinishedEvent;

					uint32 Value = 555;
					
					fg_Dispatch
						(
							TestActor
							, []
							{
								return 5;
							}
						)
						> ResultActor / [&](TCAsyncResult<int> &&_Result)
						{
							Value = *_Result;
							FinishedEvent.f_Signal();
						}
					;
					
					FinishedEvent.f_Wait();
					
					DMibExpect(Value, ==, 5);
				}
				{
					DMibTestPath("Weak actor");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCWeakActor<CBaseActor> WeakTestActor = TestActor; 

					uint32 Value = WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(60.0);
					DMibExpect(Value, ==, 2);;
					
					TestActor.f_Clear();
					
					DMibExpectExceptionType(WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(60.0), NException::CException);
				}
				auto fDispatchBoilerplate = [](auto _fToDispatch)
					{
						return fg_ConcurrentDispatch(_fToDispatch).f_CallSync();
					}
				;
				{
					DMibTestPath("Forward errors to continuation");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<uint32> Continuation;
								TestActor(&CExceptionActor::f_GetNoError) > Continuation / [Continuation](uint32 _Result)
									{
										Continuation.f_SetResult(_Result);
									}
								;
								return Continuation;
							}
						)
					;
					DMibExpect(Value, ==, 5);
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCContinuation<uint32> Continuation;
										TestActor(&CExceptionActor::f_GetError) > Continuation / [Continuation](uint32 _Result)
											{
												Continuation.f_SetResult(_Result);
											}
										;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
					
					auto fCallWithErrorString = [&](auto _Error) 
						{
							fDispatchBoilerplate
								(
									[TestActor, _Error]
									{
										TCContinuation<uint32> Continuation;
										TestActor(&CExceptionActor::f_GetError) > Continuation % _Error / [Continuation](uint32 _Result)
											{
												Continuation.f_SetResult(_Result);
											}
										;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward errors to continuation void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<void> Continuation;
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > Continuation / [Continuation]()
									{
										Continuation.f_SetResult();
									}
								;
								return Continuation;
							}
						)
					;
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCContinuation<void> Continuation;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Continuation / [Continuation]()
											{
												Continuation.f_SetResult();
											}
										;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
					
					auto fCallWithErrorString = [&](auto _Error) 
						{
							fDispatchBoilerplate
								(
									[TestActor, _Error]
									{
										TCContinuation<void> Continuation;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Continuation % _Error / [Continuation]()
											{
												Continuation.f_SetResult();
											}
										;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward result to continuation");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<uint32> Continuation;
								TestActor(&CExceptionActor::f_GetNoError) > Continuation;								
								return Continuation;
							}
						)
					;
					DMibExpect(Value, ==, 5);
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCContinuation<uint32> Continuation;
										TestActor(&CExceptionActor::f_GetError) > Continuation;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
					
					auto fCallWithErrorString = [&](auto _Error) 
						{
							fDispatchBoilerplate
								(
									[TestActor, _Error]
									{
										TCContinuation<uint32> Continuation;
										TestActor(&CExceptionActor::f_GetError) > Continuation % _Error;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward result to continuation void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<void> Continuation;
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > Continuation;
								return Continuation;
							}
						)
					;
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCContinuation<void> Continuation;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Continuation;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
					
					auto fCallWithErrorString = [&](auto _Error) 
						{
							fDispatchBoilerplate
								(
									[TestActor, _Error]
									{
										TCContinuation<void> Continuation;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Continuation % _Error;
										return Continuation;
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
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
				
				DMibExpect(Value, ==, 2);
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
				
				DMibExpect(CallbackReference, !=, nullptr);
				
				TestActor(&CCallbackActor::f_CallCallback, 32)
					> fg_DiscardResult()
				;

				FinishedEvent.f_Wait();
				{
					DMibLock(Lock);
				}
				
				DMibExpect(CallbackValue.f_Load(), ==, 32);
			};
		}

		void f_PerformanceTests_Message()
		{
			DMibTestSuite(CTestCategory("Message") << CTestGroup("Performance"))
			{
				auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#ifdef DMibDebug
				mint nIterations = 100'000;
#else
				mint nIterations = 5'000'000;
#endif
				mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.1);

				[&]() inline_never
				{
					DMibTestPath("Dispatch vector");
					CTestPerformanceMeasure DispatchMeasure("Dispatch vector");
					CPerformanceTestActor ActorEmul;
					TCVector<TCFunction<void ()>> ToDispatch;
					ToDispatch.f_SetLen(nIterations);
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(DispatchMeasure, nIterations);
						for (auto &Dispatch : ToDispatch)
						{
							Dispatch 
								= [&]
								{
									ActorEmul.f_AddInt(1);
								}
							;
						}
						for (auto &Dispatch : ToDispatch)
						{
							Dispatch();
						}
					}
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*5);
					PerfTest.f_AddBaseline(DispatchMeasure);
				}();
				
				[&]() inline_never
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
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*5);
					PerfTest.f_AddReference(DispatchMeasure);
				}();
				
				[&]() inline_never
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
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*5);
					PerfTest.f_AddReference(DispatchMeasure);
				}();
				
				[&]() inline_never
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
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Actor With Reply To Other Actor");
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Other");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					auto &ConcurrentActor = fg_ConcurrentActor();
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations*2, 2);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> ConcurrentActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Actor With Reply To Same Actor");
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Same");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations*2);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> PerfTestActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();

				[&]() inline_never
				{
					DMibTestPath("Parallell Concurrent Actors");
					
					CTestPerformanceMeasure ActorMeasure("PC Actor->Discard");
					
					uint32 Result = 0;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterations = nIterationsFull;
					mint nIterationsPerSplit = nIterations / nSplits;
					nIterations = nIterationsPerSplit * nSplits;
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					PerfTestActors.f_SetLen(nSplits);
					for (mint i = 0; i < nSplits; ++i)
						PerfTestActors[i] = fg_ConstructActor<CPerformanceTestActor>();
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations + nSplits*2, NSys::fg_Thread_GetPhysicalCores());
						TCActorResultVector<void> Results{nSplits};
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
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();

				[&]() inline_never
				{
					DMibTestPath("Parallell Concurrent Actors Result Vector");
					
					CTestPerformanceMeasure ActorMeasure("PC Actor->ResVector");
					
					uint32 Result = 0;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterations = nIterationsFull;
					mint nIterationsPerSplit = nIterations / nSplits;
					nIterations = nIterationsPerSplit * nSplits;
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					PerfTestActors.f_SetLen(nSplits);
					for (mint i = 0; i < nSplits; ++i)
						PerfTestActors[i] = fg_ConstructActor<CPerformanceTestActor>();
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations*2 + nSplits*2, NSys::fg_Thread_GetPhysicalCores());
						TCVector<TCActorResultVector<void>> Results;
						TCActorResultVector<void> Dispatches{nSplits};
						Results.f_SetLen(nSplits);
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							auto &ResultVector = Results[iSplit];
							ResultVector.f_SetLen(nIterationsPerSplit);
							fg_ConcurrentActor()
								(
									&CActor::f_Dispatch
									, [&PerfTestActors, nIterationsPerSplit, pActor, iSplit, &ResultVector]
									{
										for (mint i = 0; i < nIterationsPerSplit; ++i)
											(*pActor)(&CPerformanceTestActor::f_AddInt, 1) > ResultVector.f_AddResult();
									}
								)
								> Dispatches.f_AddResult();
							;
						}
						Dispatches.f_GetResults().f_CallSync();
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
							Results[iSplit].f_GetResults().f_CallSync();
						Result = PerfTestActors[0](&CPerformanceTestActor::f_GetResult).f_CallSync();
						for (mint iSplit = 1; iSplit < nSplits; ++iSplit)
							Result += PerfTestActors[iSplit](&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();

				[&]() inline_never
				{
					DMibTestPath("Concurrent Result Vector");
					
					CTestPerformanceMeasure ActorMeasure("C Actor->ResVector");
					
					uint32 Result = 0;
					mint nIterations = nIterationsFull;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterationsPerSplit = nIterations / nSplits;
					nIterations = nIterationsPerSplit * nSplits;
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations*2 + nSplits*2, NSys::fg_Thread_GetPhysicalCores());
						TCActorResultVector<mint> Results{nIterations};
						TCActorResultVector<void> Dispatches{nSplits};
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							fg_ConcurrentActor()
								(
									&CActor::f_Dispatch
									, [&]
									{
										auto &ConcurrentActor = fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal);
										for (mint iIteration = 0; iIteration < nIterationsPerSplit; ++iIteration)
										{
											ConcurrentActor
												(
													&CActor::f_DispatchWithReturn<mint>
													, []
													{
														return 1;
													}
												)
												> Results.f_AddResult();
											;
										}
									}
								)
								> Dispatches.f_AddResult();
							;
						}
						Dispatches.f_GetResults().f_CallSync();
						auto ResultsVector = Results.f_GetResults().f_CallSync();
						for (auto &Results : ResultsVector)
							Result += *Results;
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();

				[&]() inline_never
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
								> fg_AnyConcurrentActor() / [Continuation](TCAsyncResult<uint32> &&_Result0, TCAsyncResult<uint32> &&_Result1)
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
						DMibTestScopeMeasureThreads(ActorMeasure, ((nIterations * 2 - 1) + nIterations - 1) * 2, NSys::fg_Thread_GetPhysicalCores());
						
						Result += fg_ConcurrentActor()
							(
								&CActor::f_DispatchWithReturn<TCContinuation<uint32>>
								, [&]
								{
									return Actor(0, nIterations);
								}
							).f_CallSync(20.0);
						;
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();
				
#if 0
				[&]() inline_never
				{
					mint nIterations = nIterationsFull / 6;
					DMibTestPath("Branched std::async");
					CTestPerformanceMeasure ActorMeasure("Branched std::async");
					TCFunction<uint32 (uint32 _Start, uint32 _End)> Actor;
					Actor = [&Actor](uint32 _Start, uint32 _End) -> uint32
						{
							if ((_End - _Start) == 1)
								return 1;
							
							auto Result1 = std::async(std::launch::deferred, Actor, _Start, _Start + (_End - _Start) / 2);
							auto Result2 = std::async(std::launch::deferred, Actor, _Start + (_End - _Start) / 2, _End);

							return Result1.get() + Result2.get();
						}
					;
					
					uint32 Result = 0;
					
					for (mint i = 0; i < 5; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations * 2, NSys::fg_Thread_GetPhysicalCores());

						Result += Actor(0, nIterations);
						fp_BlockOnAllThreads();
					}
					
					DMibExpect(Result, ==, nIterations*5);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
				
				DMibExpectTrue(PerfTest);
			};
		}

		void f_PerformanceTests_Create()
		{
			DMibTestSuite(CTestCategory("Lifetime") << CTestGroup("Performance"))
			{
				auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#ifdef DMibDebug
				mint nIterations = 100'000;
#else
				mint nIterations = 1'000'000;
#endif
				mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.1);
				CTestPerformance PerfTestDestroy(0.025);
				[&]() inline_never
				{
					DMibTestPath("Vector");
					CTestPerformanceMeasure DispatchMeasure("Vector");
					CTestPerformanceMeasure DispatchMeasureDestroy("Vector");
					TCVector<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < 5; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							ToDispatch.f_SetLen(nIterations);
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
					}
					PerfTest.f_AddBaseline(DispatchMeasure);
					PerfTestDestroy.f_AddBaseline(DispatchMeasureDestroy);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Linked List");
					CTestPerformanceMeasure DispatchMeasure("Linked List");
					CTestPerformanceMeasure DispatchMeasureDestroy("Linked List");
					TCLinkedList<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < 5; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							for (mint i = 0; i < nIterations; ++i)
							{
								ToDispatch.f_Insert
									(
										CPerformanceTestActor()
									)
								;
							}
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
					}
					PerfTest.f_AddReference(DispatchMeasure);
					PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Thread safe queue");
					CTestPerformanceMeasure DispatchMeasure("Thread safe queue");
					CTestPerformanceMeasure DispatchMeasureDestroy("Thread safe queue");
					TCThreadSafeQueue<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < 5; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							for (mint i = 0; i < nIterations; ++i)
							{
								ToDispatch.f_Push
									(
										CPerformanceTestActor()
									)
								;
							}
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
					}
					PerfTest.f_AddReference(DispatchMeasure);
					PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Actor");
					CTestPerformanceMeasure ActorMeasure("Actor");
					CTestPerformanceMeasure ActorMeasureDestroy("Actor");
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					
					auto &ConcurrentActor = fg_ConcurrentActor();
		
					for (mint i = 0; i < 5; ++i)
					{
						PerfTestActors.f_SetLen(nIterations);
						{
							DMibTestScopeMeasure(ActorMeasure, nIterations);
							ConcurrentActor
								(
									&CActor::f_Dispatch
									, [&]
									{
										for (auto &PerfTestActor : PerfTestActors)
											PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
									}
								).f_CallSync()
							;
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
						{
							DMibTestScopeMeasure(ActorMeasureDestroy, nIterations);
							ConcurrentActor
								(
									&CActor::f_Dispatch
									, [&]
									{
										PerfTestActors.f_Clear();
									}
								).f_CallSync()
							;
							fp_BlockOnAllThreads();
						}
					}
					
					PerfTest.f_Add(ActorMeasure);
					PerfTestDestroy.f_Add(ActorMeasureDestroy);
				}();
				
				[&]() inline_never
				{
					DMibTestPath("Actor Outside System");
					CTestPerformanceMeasure ActorMeasure("ActorOutside");
					CTestPerformanceMeasure ActorMeasureDestroy("ActorOutside");
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					
					for (mint i = 0; i < 5; ++i)
					{
						PerfTestActors.f_SetLen(nIterations);
						{
							DMibTestScopeMeasure(ActorMeasure, nIterations);
							for (auto &PerfTestActor : PerfTestActors)
								PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
							Checkout.f_CheckMessages(); // Garbage collect memory
						}
						{
							DMibTestScopeMeasureThreads(ActorMeasureDestroy, nIterations, 1);
							PerfTestActors.f_Clear();
							fp_BlockOnAllThreads();
						}
					}
					
					PerfTest.f_Add(ActorMeasure);
					PerfTestDestroy.f_Add(ActorMeasureDestroy);
				}();
				
				DMibExpectTrue(PerfTest);
				DMibExpectTrue(PerfTestDestroy);
			};
		}

		void f_PerformanceTests()
		{
			f_PerformanceTests_Create();
			f_PerformanceTests_Message();
		}
		
		void f_DoTests()
		{
			f_FunctionalTests();
			f_PerformanceTests();		
		}
	};

	DMibTestRegister(CActor_Tests, Malterlib::Concurrency);
}
