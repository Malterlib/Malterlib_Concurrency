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

#define DDoTest_Create_Vector 1
#define DDoTest_Create_LinkedList 1
#define DDoTest_Create_ThreadSafeQueue 1
#define DDoTest_Create_ActorConcurrent 1
#define DDoTest_Create_Actor 1
#define DDoTest_Create_ActorOutside 1

namespace
{
	using namespace NMib;
	using namespace NMib::NStorage;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NStr;

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
		enum
		{
			mc_bAllowInternalAccess = true
		};

		CPerformanceTestActor(int _Dummy = 0)
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

	class CPerformanceTestActorEmul : public CPerformanceTestActor
	{
	public:
		enum
		{
			mc_bAllowInternalAccess = true
		};

		static int fs_Setup(CPerformanceTestActorEmul *_pThis)
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			ThreadLocal.m_pCurrentActor = _pThis;
#if DMibEnableSafeCheck > 0
			ThreadLocal.m_pCurrentlyConstructingActor = _pThis;
#endif
			return 0;
		}

		CPerformanceTestActorEmul()
			 : CPerformanceTestActor(fs_Setup(this))
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			ThreadLocal.m_pCurrentActor = nullptr;
#if DMibEnableSafeCheck > 0
			ThreadLocal.m_pCurrentlyConstructingActor = nullptr;
#endif
#if DMibConfig_Tests_Enable
			ThreadLocal.m_pCurrentlyProcessingActorHolder->f_TestDetach();
#endif
		}

		CPerformanceTestActorEmul(CPerformanceTestActorEmul &&_Other) = default;
	};

	class CExceptionActor : public CActor
	{
	public:
		virtual TCFuture<uint32> f_GetError()
		{
			return DMibErrorInstance("Error");
		}

		virtual TCFuture<uint32> f_GetNoError()
		{
			return fg_Explicit(5);
		}

		virtual TCFuture<void> f_GetErrorVoid()
		{
			return DMibErrorInstance("Error");
		}

		virtual TCFuture<void> f_GetNoErrorVoid()
		{
			return fg_Explicit();
		}

		virtual TCFuture<void> f_GetResultWasNotSet()
		{
			return {};
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

		virtual TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> &&_pValue) = 0;
		virtual TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> &&_pValue) = 0;
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

		virtual TCFuture<void> fp_Destroy() override
		{
			return fg_Explicit();
		}

		TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> &&_pValue) override
		{
			return fg_Explicit(_pValue->f_ToInt());
		}

		TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> &&_pValue) override
		{
			return fg_Explicit(_pValue->f_ToInt());
		}

		mint m_DummyMember0 = 0;
		mint m_DummyMember1 = 1;
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

		CActorSubscription f_RegisterCallback(TCActor<CActor> _pActor, TCFunctionMovable<void (int32 _Value)> && _fCallback)
		{
			return m_CallbackManager.f_Register(_pActor, fg_Move(_fCallback));
		}

		void f_CallCallback(int32 _Value)
		{
			m_CallbackManager(_Value) > NConcurrency::fg_DiscardResult();
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
							(void)_bGarbageCollect;
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
						(Actor->f_Destroy() + Actor->f_Destroy()).f_CallSync(30.0);
					}
				;

#if DMibConfig_Concurrency_DebugBlockDestroy
				DMibTest
					(
					 	DMibExpr
					 	(
						 	fg_ThrowsException
						 	(
							 	DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed")
							 	, DMibImpExceptionInstance(CExceptionActorDeleted, "Actor 'NMib::NConcurrency::CActor' called has been deleted")
							)
						)
					 	== DMibLExpr(fDoubleDestroy())
					)
				;
#else
				DMibTest
					(
					 	DMibExpr
					 	(
						 	fg_ThrowsException
						 	(
							 	DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed")
							 	, DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted")
							)
						)
					 	== DMibLExpr(fDoubleDestroy())
					)
				;
#endif
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
					DMibTestPath("Pointer params");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();

					TCFuture<uint32> SharedPointerFuture = TestActor(&CBaseActor::f_SharedPointer, fg_Construct(CStr("5")));
					uint32 SharedPointerResult = fg_Move(SharedPointerFuture).f_CallSync(60.0);
					DMibExpect(SharedPointerResult, ==, 5);

					TCFuture<uint32> UniquePointerFuture = TestActor(&CBaseActor::f_UniquePointer, fg_Construct(CStr("5")));
					uint32 UniquePointerResult = fg_Move(UniquePointerFuture).f_CallSync(60.0);
					DMibExpect(UniquePointerResult, ==, 5);
				}
				{
					DMibTestPath("Weak actor");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCWeakActor<CBaseActor> WeakTestActor = TestActor; 

					uint32 Value = WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(60.0);
					DMibExpect(Value, ==, 2);;

					TestActor->f_Destroy().f_CallSync();
					TestActor.f_Clear();

					DMibExpectExceptionType(WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(60.0), NException::CException);
				}
				auto fDispatchBoilerplate = [](auto _fToDispatch)
					{
						return fg_ConcurrentDispatch(_fToDispatch).f_CallSync();
					}
				;
				{
					DMibTestPath("Forward errors to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<uint32> Promise;
								TestActor(&CExceptionActor::f_GetNoError) > Promise / [Promise](uint32 _Result)
									{
										Promise.f_SetResult(_Result);
									}
								;
								return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetError) > Promise / [Promise](uint32 _Result)
											{
												Promise.f_SetResult(_Result);
											}
										;
										return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetError) > Promise % _Error / [Promise](uint32 _Result)
											{
												Promise.f_SetResult(_Result);
											}
										;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward errors to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<void> Promise;
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > Promise / [Promise]()
									{
										Promise.f_SetResult();
									}
								;
								return Promise.f_MoveFuture();
							}
						)
					;

					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										return Promise.f_MoveFuture();
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
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise % _Error / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward call pack errors to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<uint32> Promise;
								TestActor(&CExceptionActor::f_GetNoError)
									+ TestActor(&CExceptionActor::f_GetNoError)
									> Promise / [Promise](uint32 _Result0, uint32 _Result1)
									{
										Promise.f_SetResult(_Result0);
									}
								;
								return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetError) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise / [Promise](uint32 _Result0, uint32 _Result1)
											{
												Promise.f_SetResult(_Result0);
											}
										;
										return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetNoError) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise % _Error / [Promise](uint32 _Result0, uint32 _Result1)
											{
												Promise.f_SetResult(_Result0);
											}
										;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward call pack errors to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<void> Promise;
								TestActor(&CExceptionActor::f_GetNoErrorVoid)
									+ TestActor(&CExceptionActor::f_GetNoErrorVoid)
									> Promise / [Promise]()
									{
										Promise.f_SetResult();
									}
								;
								return Promise.f_MoveFuture();
							}
						)
					;

					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetErrorVoid) 
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise / [Promise](CVoidTag, uint32 _Value)
											{
												Promise.f_SetResult();
											}
										;
										return Promise.f_MoveFuture();
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
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetNoErrorVoid) 
											+ TestActor(&CExceptionActor::f_GetErrorVoid)
											> Promise % _Error / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}				
				{
					DMibTestPath("Forward result to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<uint32> Promise;
								TestActor(&CExceptionActor::f_GetNoError) > Promise;								
								return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetError) > Promise;
										return Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise;
										TestActor(&CExceptionActor::f_GetError) > Promise % _Error;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Forward result to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]
							{
								TCPromise<void> Promise;
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > Promise;
								return Promise.f_MoveFuture();
							}
						)
					;

					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]
									{
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise;
										return Promise.f_MoveFuture();
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
										TCPromise<void> Promise;
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise % _Error;
										return Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithErrorString("Failed important call"), DMibErrorInstance("Failed important call: Error"));
					DMibExpectException(fCallWithErrorString(NStr::CStr("Failed important call")), DMibErrorInstance("Failed important call: Error"));
				}
				{
					DMibTestPath("Return actor call to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
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
									[TestActor]() -> TCFuture<uint32>
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
					DMibTestPath("Return actor call to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();
					
					fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<void>
							{
								return TestActor(&CExceptionActor::f_GetNoErrorVoid);
							}
						)
					;

					auto fCallWithError = [&] 
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCFuture<void>
									{
										return TestActor(&CExceptionActor::f_GetErrorVoid);
									}
								)
							;
						}
					;
					DMibExpectException(fCallWithError(), DMibErrorInstance("Error"));
				}
				{
					DMibTestPath("Result was not set");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					DMibExpectException(TestActor(&CExceptionActor::f_GetResultWasNotSet).f_CallSync(), DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "Result was not set"));
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
				[[maybe_unused]] mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.1);

#if DDoTest_Message_DispatchVector
				[&]() inline_never
				{
					DMibTestPath("Dispatch vector");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure DispatchMeasure("Dispatch vector");

					TCActor<CPerformanceTestActor> ActorEmulActor = fg_Construct();
					auto &ActorEmul = ActorEmulActor->f_AccessInternal();
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
					TCActor<CPerformanceTestActor> ActorEmulActor = fg_Construct();
					auto &ActorEmul = ActorEmulActor->f_AccessInternal();
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
					TCActor<CPerformanceTestActor> ActorEmulActor = fg_Construct();
					auto &ActorEmul = ActorEmulActor->f_AccessInternal();
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
									[nIterationsPerSplit, pActor]
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
									[nIterationsPerSplit, pActor, &ResultVector]
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
					TCFunction<TCFuture<uint32> (uint32 _Start, uint32 _End)> Actor;
					Actor = [&Actor](uint32 _Start, uint32 _End) -> TCFuture<uint32>
						{
							if ((_End - _Start) == 1)
								return fg_Explicit(1);
							
							TCPromise<uint32> Promise;

							fg_ConcurrentActor()
								(
									&CActor::f_DispatchWithReturn<TCFuture<uint32>>
									, [&Actor, Start = _Start, End = _Start + (_End - _Start) / 2]
									{
										return Actor(Start, End);
									}
								)
								+ fg_ConcurrentActor()
								(
									&CActor::f_DispatchWithReturn<TCFuture<uint32>>
									, [&Actor, Start = _Start + (_End - _Start) / 2, End = _End]
									{
										return Actor(Start, End);
									}
								)
								> fg_AnyConcurrentActor() / [Promise](TCAsyncResult<uint32> &&_Result0, TCAsyncResult<uint32> &&_Result1)
								{
									Promise.f_SetResult(*_Result0 + *_Result1);
								}
							;
							return Promise.f_MoveFuture();
						}
					;

					uint32 Result = 0;

					auto ConcurrentActor = fg_ConcurrentActor();

					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						
						Result += fg_ConcurrentActor()
							(
								&CActor::f_DispatchWithReturn<TCFuture<uint32>>
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

		struct CCreateContext
		{
			CTestPerformance &m_PerfTest;
			CTestPerformance &m_PerfTestDestroy;
			mint m_nIterations;
		};

		inline_never void f_PerformanceTests_Create_Vector(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_Vector
			mint nIterations = _CreateContext.m_nIterations;

			fp_BlockOnAllThreads(true);
			DMibTestPath("Vector");
			CTestPerformanceMeasure DispatchMeasure("Vector");
			CTestPerformanceMeasure DispatchMeasureDestroy("Vector");

			auto &ConcurrencyManager = fg_ConcurrencyManager();
			TCActorInternal<CActor> Holder{&ConcurrencyManager, nullptr};
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			auto pOldActorHalder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = &Holder;
			auto Cleanup = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHalder;
				}
			;

			TCVector<CPerformanceTestActorEmul> ToDispatch;

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
			_CreateContext.m_PerfTest.f_AddBaseline(DispatchMeasure);
			_CreateContext.m_PerfTestDestroy.f_AddBaseline(DispatchMeasureDestroy);
#endif
		}

		inline_never void f_PerformanceTests_Create_LinkedList(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_LinkedList
			mint nIterations = _CreateContext.m_nIterations;

			fp_BlockOnAllThreads(true);
			DMibTestPath("Linked List");
			CTestPerformanceMeasure DispatchMeasure("Linked List");
			CTestPerformanceMeasure DispatchMeasureDestroy("Linked List");

			auto &ConcurrencyManager = fg_ConcurrencyManager();
			TCActorInternal<CActor> Holder{&ConcurrencyManager, nullptr};
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			auto pOldActorHalder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = &Holder;
			auto Cleanup = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHalder;
				}
			;

			TCLinkedList<CPerformanceTestActorEmul> ToDispatch;

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
			_CreateContext.m_PerfTest.f_AddReference(DispatchMeasure);
			_CreateContext.m_PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
#endif
		}

		inline_never void f_PerformanceTests_Create_ThreadSafeQueue(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_ThreadSafeQueue
			mint nIterations = _CreateContext.m_nIterations;

			fp_BlockOnAllThreads(true);
			DMibTestPath("Thread safe queue");
			CTestPerformanceMeasure DispatchMeasure("Thread safe queue");
			CTestPerformanceMeasure DispatchMeasureDestroy("Thread safe queue");

			auto &ConcurrencyManager = fg_ConcurrencyManager();
			TCActorInternal<CActor> Holder{&ConcurrencyManager, nullptr};
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			auto pOldActorHalder = ThreadLocal.m_pCurrentlyProcessingActorHolder;
			ThreadLocal.m_pCurrentlyProcessingActorHolder = &Holder;
			auto Cleanup = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pCurrentlyProcessingActorHolder = pOldActorHalder;
				}
			;

			TCThreadSafeQueue<CPerformanceTestActorEmul> ToDispatch;

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
			_CreateContext.m_PerfTest.f_AddReference(DispatchMeasure);
			_CreateContext.m_PerfTestDestroy.f_AddReference(DispatchMeasureDestroy);
#endif
		}

		inline_never void f_PerformanceTests_Create_ActorConcurrent(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_ActorConcurrent
			mint nIterations = _CreateContext.m_nIterations;

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
								, [&, iStart = iThread * nIterationsPerThread]() -> TCFuture<void>
								{
									auto pArray = PerfTestActors.f_GetArray();
									auto pEnd = pArray + iStart + nIterationsPerThread;
									for (auto pPerfTestActor = pArray + iStart; pPerfTestActor != pEnd; ++pPerfTestActor)
										*pPerfTestActor = fg_ConstructActor<CPerformanceTestActor>();

									TCActorResultVector<void> Results;
									fp_BlockOnAllThreads(Results, false);
									TCPromise<void> Promise;
									Results.f_GetResults() > Promise / [Promise](TCVector<TCAsyncResult<void>> &&)
										{
											Promise.f_SetResult();
										}
									;
									return Promise.f_MoveFuture();
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
								, [&, iStart = iThread * nIterationsPerThread]() -> TCFuture<void>
								{
									auto pArray = PerfTestActors.f_GetArray();
									auto pEnd = pArray + iStart + nIterationsPerThread;
									TCActorResultVector<void> Results{nIterationsPerThread};
									for (auto pPerfTestActor = pArray + iStart; pPerfTestActor != pEnd; ++pPerfTestActor)
									{
										(*pPerfTestActor)->f_Destroy() > Results.f_AddResult();
										pPerfTestActor->f_Clear();
									}
									TCPromise<void> Promise;
									Results.f_GetResults() > Promise / [this, Promise, nThreads](TCVector<TCAsyncResult<void>> &&)
										{
											TCActorResultVector<void> Results{nThreads};
											fp_BlockOnAllThreads(Results, false);
											Results.f_GetResults() > Promise / [Promise](TCVector<TCAsyncResult<void>> &&)
												{
													Promise.f_SetResult();
												}
											;
										}
									;
									return Promise.f_MoveFuture();
								}
							)
							> ThreadResults.f_AddResult()
						;
					}
					ThreadResults.f_GetResults().f_CallSync();
					fp_BlockOnAllThreads(false);
				}
			}

			_CreateContext.m_PerfTest.f_Add(ActorMeasure);
			_CreateContext.m_PerfTestDestroy.f_Add(ActorMeasureDestroy);
#endif
		}

		inline_never void f_PerformanceTests_Create_Actor(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_Actor
			mint nIterations = _CreateContext.m_nIterations;

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
									PerfTestActor->f_Destroy() > Results.f_AddResult();
									PerfTestActor.f_Clear();
								}
										
								TCPromise<void> Promise;
								Results.f_GetResults() > [Promise](auto &&)
									{
										Promise.f_SetResult();
									}
								;
								return Promise.f_MoveFuture();
							}
						).f_CallSync()
					;
					fp_BlockOnAllThreads(false);
				}
			}

			_CreateContext.m_PerfTest.f_Add(ActorMeasure);
			_CreateContext.m_PerfTestDestroy.f_Add(ActorMeasureDestroy);
#endif
		}

		inline_never void f_PerformanceTests_Create_ActorOutside(CCreateContext const &_CreateContext)
		{
#if DDoTest_Create_ActorOutside
			mint nIterations = _CreateContext.m_nIterations;

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
						PerfTestActor->f_Destroy() > Results.f_AddResult();
						PerfTestActor.f_Clear();
					}
					Results.f_GetResults().f_CallSync();
					fp_BlockOnAllThreads(false);
				}
			}
#ifdef DDoGarbageCollection
			Checkout.f_GarbageCollectLocalArena(false); // Garbage collect memory
#endif
			_CreateContext.m_PerfTest.f_Add(ActorMeasure);
			_CreateContext.m_PerfTestDestroy.f_Add(ActorMeasureDestroy);
#endif
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
				[[maybe_unused]] mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.1);
				CTestPerformance PerfTestDestroy(0.025);
				CCreateContext CreateContext{PerfTest, PerfTestDestroy, nIterations};

				f_PerformanceTests_Create_Vector(CreateContext);
				f_PerformanceTests_Create_LinkedList(CreateContext);
				f_PerformanceTests_Create_ThreadSafeQueue(CreateContext);
				f_PerformanceTests_Create_ActorConcurrent(CreateContext);
				f_PerformanceTests_Create_Actor(CreateContext);
				f_PerformanceTests_Create_ActorOutside(CreateContext);

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
