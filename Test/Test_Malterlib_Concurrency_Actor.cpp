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

//#define DDoCheckMessages
//#define DDoGarbageCollection

#define DDoTest_Message_DispatchVector 1
#define DDoTest_Message_Dispatch 1
#define DDoTest_Message_DispatchThreaded 1
#define DDoTest_Message_ActorDiscard 1
#define DDoTest_Message_ActorDirect 1
#define DDoTest_Message_ActorToOther 1
#define DDoTest_Message_ActorToSame 1
#define DDoTest_Message_PCActorToDiscard 1
#define DDoTest_Message_PCActorToResVector 1
#define DDoTest_Message_CActorToResVector 1
#define DDoTest_Message_BranchedConcurrentActor 1

#define DDoTest_Create_Vector 0
#define DDoTest_Create_LinkedList 0
#define DDoTest_Create_ThreadSafeQueue 0
#define DDoTest_Create_ActorConcurrent 0
#define DDoTest_Create_Actor 0
#define DDoTest_Create_ActorOutside 0

namespace
{
	using namespace NMib;
	using namespace NMib::NPtr;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	
	constexpr mint gc_nRepetitions = 11;

	class CConstructorExceptionActor : public CActor
	{
	public:
		CConstructorExceptionActor()
		{
			DMibError("Test error");
		}
	};

	class CConstructExceptionActor : public CActor
	{
	public:
		void fp_Construct() override
		{
			DMibError("Test error");
		}
	};
	
	class CPerformanceTestActor : public CActor
	{
		zuint32 m_Value;
	public:
		CPerformanceTestActor()
		{
		}
		CPerformanceTestActor(CPerformanceTestActor &&_Other)
			: m_Value(_Other.m_Value)
		{
		}
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
		
		virtual TCContinuation<void> fp_Destroy() override
		{
			return TCContinuation<void>::fs_Finished(); 
		}
	};
	
	class CCallbackActor : public CActor
	{
		TCActorSubscriptionManager<void (int32)> m_CallbackManager;
	public:
		CCallbackActor()
			: CActor()
			, m_CallbackManager(this, false)
		{
		}
		
		CActorSubscription f_RegisterCallback(TCActor<CActor> _pActor, TCFunction<void (int32 _Value)> && _fCallback)
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
		void fp_BlockOnAllThreads(TCActorResultVector<void> &_Results, bool _bGarbageCollect)
		{
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
			for (mint i = 0; i < nThreads; ++i)
			{
				fg_ConcurrentDispatch
					(
						[_bGarbageCollect]
						{
							auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#ifdef DDoGarbageCollection
							if (_bGarbageCollect)
								Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
							else
#endif
#ifdef DDoCheckMessages
								Checkout.f_CheckMessages();
#else
								;
#endif
						}
					)
					> _Results.f_AddResult()
				;
			}
		}
		void fp_BlockOnAllThreads(bool _bGarbageCollect)
		{
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
			TCActorResultVector<void> Results{nThreads};
			fp_BlockOnAllThreads(Results, _bGarbageCollect);
			Results.f_GetResults().f_CallSync();
			auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#ifdef DDoGarbageCollection
			if (_bGarbageCollect)
				Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
			else
#endif
#ifdef DDoCheckMessages
				Checkout.f_CheckMessages();
#else
				;
#endif
		}
	public:

		void f_ConstructionTests()
		{
			DMibTestSuite("Construction")
			{
				auto fConstructActorConstructorException = []
					{
						TCActor<CConstructorExceptionActor> pActor;
						pActor = fg_Construct();
					}
				;

				auto fConstructActorConstructException = []
					{
						TCActor<CConstructExceptionActor> pActor = fg_Construct();
					}
				;
				
				DMibExpectException(fConstructActorConstructorException(), DMibErrorInstance("Test error"));
				DMibExpectException(fConstructActorConstructException(), DMibErrorInstance("Test error"));
			};
		}
		
		void f_DestructTests()
		{
			DMibTestSuite("Destruct")
			{
				TCActor<CActor> Actor = fg_Construct();
				
				auto fDoubleDestroy = [&]
					{
						(Actor->f_Destroy2() + Actor->f_Destroy2()).f_CallSync(30.0);
					}
				;
				
				DMibExpectException(fDoubleDestroy(), DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed"));
			};
		}
		
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
					DMibTestPath("Forward call pack errors to continuation");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<uint32> Continuation;
								TestActor(&CExceptionActor::f_GetNoError)
									+ TestActor(&CExceptionActor::f_GetNoError)
									> Continuation / [Continuation](uint32 _Result0, uint32 _Result1)
									{
										Continuation.f_SetResult(_Result0);
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
										TestActor(&CExceptionActor::f_GetError) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Continuation / [Continuation](uint32 _Result0, uint32 _Result1)
											{
												Continuation.f_SetResult(_Result0);
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
										TestActor(&CExceptionActor::f_GetNoError) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Continuation % _Error / [Continuation](uint32 _Result0, uint32 _Result1)
											{
												Continuation.f_SetResult(_Result0);
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
					DMibTestPath("Forward call pack errors to continuation void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCContinuation<void> Continuation;
								TestActor(&CExceptionActor::f_GetNoErrorVoid)
									+ TestActor(&CExceptionActor::f_GetNoErrorVoid)
									> Continuation / [Continuation]()
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
										TestActor(&CExceptionActor::f_GetErrorVoid) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Continuation / [Continuation](CVoidTag, uint32 _Value)
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
										TestActor(&CExceptionActor::f_GetNoErrorVoid) 
											+ TestActor(&CExceptionActor::f_GetErrorVoid)
											> Continuation % _Error / [Continuation]()
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
				{
					DMibTestPath("Return actor call to continuation");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]() -> TCContinuation<uint32>
							{
								return TestActor(&CExceptionActor::f_GetNoError);								
							}
						)
					;
					DMibExpect(Value, ==, 5);
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCContinuation<uint32>
									{
										return TestActor(&CExceptionActor::f_GetError);
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
				}
				{
					DMibTestPath("Return actor call to continuation void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					fDispatchBoilerplate
						(
							[TestActor]() -> TCContinuation<void>
							{
								return TestActor(&CExceptionActor::f_GetNoErrorVoid);
							}
						)
					;
					
					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCContinuation<void>
									{
										return TestActor(&CExceptionActor::f_GetErrorVoid);
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
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
				CActorSubscription CallbackReference;
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
					> fg_ConcurrentActor() / [&](TCAsyncResult<CActorSubscription> && _Result)
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

#if DDoTest_Message_DispatchVector
				[&]() inline_never
				{
					DMibTestPath("Dispatch vector");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure DispatchMeasure("Dispatch vector");
					CPerformanceTestActor ActorEmul;
					TCVector<TCFunction<void ()>> ToDispatch;
					ToDispatch.f_SetLen(nIterations);
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*gc_nRepetitions);
					PerfTest.f_AddBaseline(DispatchMeasure);
				}();
#endif
#if DDoTest_Message_Dispatch
				[&]() inline_never
				{
					DMibTestPath("Dispatch");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure DispatchMeasure("Dispatch");
					CPerformanceTestActor ActorEmul;
					TCLinkedList<TCFunction<void ()>> ToDispatch;
	
					for (mint i = 0; i < gc_nRepetitions; ++i)
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*gc_nRepetitions);
					PerfTest.f_AddReference(DispatchMeasure);
				}();
#endif
#if DDoTest_Message_DispatchThreaded			
				[&]() inline_never
				{
					DMibTestPath("Dispatch threaded");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure DispatchMeasure("Dispatch threaded");
					CPerformanceTestActor ActorEmul;
					TCThreadSafeQueue<TCFunction<void ()>> ToDispatch;
	
					for (mint i = 0; i < gc_nRepetitions; ++i)
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(ActorEmul.f_GetResult(), ==, nIterations*gc_nRepetitions);
					PerfTest.f_AddReference(DispatchMeasure);
				}();
#endif			
#if DDoTest_Message_ActorDiscard
				[&]() inline_never
				{
					DMibTestPath("Actor Discard");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Discard");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) > fg_DiscardResult();
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif			
#if DDoTest_Message_ActorDirect			
				[&]() inline_never
				{
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
					
					DMibTestPath("Actor Direct");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Direct");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) > NConcurrency::NPrivate::fg_DirectResultActor() / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_ActorToOther			
				[&]() inline_never
				{
					DMibTestPath("Actor->Other");
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Other");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					auto ReplyActor = fg_ConstructActor<CActor>();
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, 2);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> ReplyActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif				
#if DDoTest_Message_ActorToSame
				[&]() inline_never
				{
					DMibTestPath("Actor->Same");
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull;
					
					CTestPerformanceMeasure ActorMeasure("Actor->Same");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					
					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
						for (mint i = 0; i < nIterations; ++i)
						{
							PerfTestActor(&CPerformanceTestActor::f_AddInt, 1) 
								> PerfTestActor / [](TCAsyncResult<void> &&)
								{
								}
							;
						}
						
						Result = PerfTestActor(&CPerformanceTestActor::f_GetResult).f_CallSync();
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToDiscard
				[&]() inline_never
				{
					DMibTestPath("PC Actor->Discard");
					fp_BlockOnAllThreads(true);
					
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
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						TCActorResultVector<void> Results{nSplits};
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[&PerfTestActors, nIterationsPerSplit, pActor]
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToResVector
				[&]() inline_never
				{
					DMibTestPath("PC Actor->ResVector");
					fp_BlockOnAllThreads(true);
					
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
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						TCVector<TCActorResultVector<void>> Results;
						TCActorResultVector<void> Dispatches{nSplits};
						Results.f_SetLen(nSplits);
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							auto &ResultVector = Results[iSplit];
							ResultVector.f_SetLen(nIterationsPerSplit);
							fg_ConcurrentDispatch
								(
									[&PerfTestActors, nIterationsPerSplit, pActor, iSplit, &ResultVector]
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_CActorToResVector
				[&]() inline_never
				{
					DMibTestPath("C Actor->ResVector");
					fp_BlockOnAllThreads(true);
					
					CTestPerformanceMeasure ActorMeasure("C Actor->ResVector");
					
					uint32 Result = 0;
					mint nIterations = nIterationsFull;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterationsPerSplit = nIterations / nSplits;
					nIterations = nIterationsPerSplit * nSplits;
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						TCActorResultVector<mint> Results{nIterations};
						TCActorResultVector<void> Dispatches{nSplits};
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							fg_ConcurrentDispatch
								(
									[&]
									{
										auto &ConcurrentActor = fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal);
										for (mint iIteration = 0; iIteration < nIterationsPerSplit; ++iIteration)
										{
											fg_Dispatch
												(
													ConcurrentActor
													, []
													{
														return mint(1);
													}
												)
												> Results.f_AddResult()
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
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_BranchedConcurrentActor
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull / 2;
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

					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						
						Result += fg_ConcurrentActor()
							(
								&CActor::f_DispatchWithReturn<TCContinuation<uint32>>
								, [&]
								{
									return Actor(0, nIterations);
								}
							).f_CallSync(20.0);
						;
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
					PerfTest.f_Add(ActorMeasure);
				}();
#endif
				
#if 0
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
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
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations * 2, NSys::fg_Thread_GetPhysicalCores());

						Result += Actor(0, nIterations);
						fp_BlockOnAllThreads(false);
					}
					
					DMibExpect(Result, ==, nIterations*gc_nRepetitions);
					
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
#if DDoTest_Create_Vector
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("Vector");
					CTestPerformanceMeasure DispatchMeasure("Vector");
					CTestPerformanceMeasure DispatchMeasureDestroy("Vector");
					TCVector<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							ToDispatch.f_SetLen(nIterations);
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
						}
					}
#ifdef DDoGarbageCollection
					Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
#endif
					PerfTest.f_AddBaseline(DispatchMeasure);
					PerfTestDestroy.f_AddBaseline(DispatchMeasureDestroy);
				}();
#endif
#if DDoTest_Create_LinkedList
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("Linked List");
					CTestPerformanceMeasure DispatchMeasure("Linked List");
					CTestPerformanceMeasure DispatchMeasureDestroy("Linked List");
					TCLinkedList<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							for (mint i = 0; i < nIterations; ++i)
							{
								ToDispatch.f_Insert();
							}
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
						}
					}
#ifdef DDoGarbageCollection
					Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
#endif
					PerfTest.f_AddReference(DispatchMeasure);
					PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
				}();
#endif
#if DDoTest_Create_ThreadSafeQueue
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("Thread safe queue");
					CTestPerformanceMeasure DispatchMeasure("Thread safe queue");
					CTestPerformanceMeasure DispatchMeasureDestroy("Thread safe queue");
					TCThreadSafeQueue<CPerformanceTestActor> ToDispatch;
	
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						{
							DMibTestScopeMeasure(DispatchMeasure, nIterations);
							for (mint i = 0; i < nIterations; ++i)
							{
								ToDispatch.f_Push();
							}
						}
						{
							DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
							ToDispatch.f_Clear();
						}
					}
#ifdef DDoGarbageCollection
					Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
#endif
					PerfTest.f_AddReference(DispatchMeasure);
					PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
				}();
#endif
#if DDoTest_Create_ActorConcurrent
				[&, nIterations]() inline_never mutable
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("ActorConcurrent");
					CTestPerformanceMeasure ActorMeasure("ActorConcurrent");
					CTestPerformanceMeasure ActorMeasureDestroy("ActorConcurent");
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					
					mint nThreads = NSys::fg_Thread_GetVirtualCores();
					mint nIterationsPerThread = nIterations / nThreads;
					nIterations = nIterationsPerThread * nThreads;
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						fp_BlockOnAllThreads(true);
						PerfTestActors.f_SetLen(nIterations);
						
						TCVector<NConcurrency::TCActor<NConcurrency::CConcurrentActor>> ConcurrentActors;
						ConcurrentActors.f_SetLen(nThreads);
						for (auto &Actor : ConcurrentActors)
							Actor = fg_ConcurrentActor();
						{
							TCActorResultVector<void> ThreadResults;
							DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
							for (mint iThread = 0; iThread < nThreads; ++iThread)
							{
								fg_Dispatch
									(
										ConcurrentActors[iThread]
										, [&, iStart = iThread * nIterationsPerThread]() -> TCContinuation<void>
										{
											auto pArray = PerfTestActors.f_GetArray();
											auto pEnd = pArray + iStart + nIterationsPerThread;
											for (auto pPerfTestActor = pArray + iStart; pPerfTestActor != pEnd; ++pPerfTestActor)
												*pPerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
											
											TCActorResultVector<void> Results;
											fp_BlockOnAllThreads(Results, false);
											TCContinuation<void> Continuation;
											Results.f_GetResults() > Continuation / [Continuation, iStart](TCVector<TCAsyncResult<void>> &&)
												{
													Continuation.f_SetResult();
												}
											;
											return Continuation;
										}
									)
									> ThreadResults.f_AddResult()
								;
							}
							ThreadResults.f_GetResults().f_CallSync();
							fp_BlockOnAllThreads(false);
						}
						{
							TCActorResultVector<void> ThreadResults;
							DMibTestScopeMeasureThreads(ActorMeasureDestroy, nIterations, NSys::fg_Thread_GetPhysicalCores());
							for (mint iThread = 0; iThread < nThreads; ++iThread)
							{
								fg_Dispatch
									(
										ConcurrentActors[iThread]
										, [&, iStart = iThread * nIterationsPerThread]() -> TCContinuation<void>
										{
											auto pArray = PerfTestActors.f_GetArray();
											auto pEnd = pArray + iStart + nIterationsPerThread;
											TCActorResultVector<void> Results{nIterationsPerThread};
											for (auto pPerfTestActor = pArray + iStart; pPerfTestActor != pEnd; ++pPerfTestActor)
											{
												(*pPerfTestActor)->f_Destroy(Results.f_AddResult());
												pPerfTestActor->f_Clear();
											}
											TCContinuation<void> Continuation;
											Results.f_GetResults() > Continuation / [this, Continuation, nThreads, iStart](TCVector<TCAsyncResult<void>> &&)
												{
													TCActorResultVector<void> Results{nThreads};
													fp_BlockOnAllThreads(Results, false);
													Results.f_GetResults() > Continuation / [Continuation, nThreads, iStart](TCVector<TCAsyncResult<void>> &&)
														{
															Continuation.f_SetResult();
														}
													;
												}
											;
											return Continuation;
										}
									)
									> ThreadResults.f_AddResult()
								;
							}
							ThreadResults.f_GetResults().f_CallSync();
							fp_BlockOnAllThreads(false);
						}
					}
					
					PerfTest.f_Add(ActorMeasure);
					PerfTestDestroy.f_Add(ActorMeasureDestroy);
				}();
#endif
#if DDoTest_Create_Actor
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("Actor");
					CTestPerformanceMeasure ActorMeasure("Actor");
					CTestPerformanceMeasure ActorMeasureDestroy("Actor");
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					
					auto &ConcurrentActor = fg_ConcurrentActor();
		
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						PerfTestActors.f_SetLen(nIterations);
						{
							DMibTestScopeMeasure(ActorMeasure, nIterations);
							fg_Dispatch
								(
									ConcurrentActor
									, [&]
									{
										for (auto &PerfTestActor : PerfTestActors)
											PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
									}
								).f_CallSync()
							;
							fp_BlockOnAllThreads(false);
						}
						{
							DMibTestScopeMeasure(ActorMeasureDestroy, nIterations);
							fg_Dispatch
								(
									ConcurrentActor
									, [&]
									{
										TCActorResultVector<void> Results{PerfTestActors.f_GetLen()};
										for (auto &PerfTestActor : PerfTestActors)
										{
											PerfTestActor->f_Destroy(Results.f_AddResult());
											PerfTestActor.f_Clear();
										}
										
										TCContinuation<void> Continuation;
										Results.f_GetResults() > [Continuation](auto &&)
											{
												Continuation.f_SetResult();
											}
										;
										return Continuation;
									}
								).f_CallSync()
							;
							fp_BlockOnAllThreads(false);
						}
					}
					
					PerfTest.f_Add(ActorMeasure);
					PerfTestDestroy.f_Add(ActorMeasureDestroy);
				}();
#endif
#if DDoTest_Create_ActorOutside
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					DMibTestPath("Actor Outside System");
					CTestPerformanceMeasure ActorMeasure("ActorOutside");
					CTestPerformanceMeasure ActorMeasureDestroy("ActorOutside");
					TCVector<TCActor<CPerformanceTestActor>> PerfTestActors;
					
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						PerfTestActors.f_SetLen(nIterations);
						{
							DMibTestScopeMeasure(ActorMeasure, nIterations);
							for (auto &PerfTestActor : PerfTestActors)
								PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
						}
						{
							DMibTestScopeMeasureThreads(ActorMeasureDestroy, nIterations, NSys::fg_Thread_GetPhysicalCores());
							TCActorResultVector<void> Results{PerfTestActors.f_GetLen()};
							for (auto &PerfTestActor : PerfTestActors)
							{
								PerfTestActor->f_Destroy(Results.f_AddResult());
								PerfTestActor.f_Clear();
							}
							Results.f_GetResults().f_CallSync();
							fp_BlockOnAllThreads(false);
						}
					}
#ifdef DDoGarbageCollection
					Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
#endif
					
					PerfTest.f_Add(ActorMeasure);
					PerfTestDestroy.f_Add(ActorMeasureDestroy);
				}();
#endif
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
			f_ConstructionTests();
			f_DestructTests();
			f_FunctionalTests();
			f_PerformanceTests();		
		}
	};

	DMibTestRegister(CActor_Tests, Malterlib::Concurrency);
}
