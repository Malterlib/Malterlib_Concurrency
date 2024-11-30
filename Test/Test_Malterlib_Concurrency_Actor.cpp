// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Performance>
#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Concurrency/ActorSubscription>

#if 0
#include <numeric>
#include <future>
#endif

#define DDoCheckMessages
#define DDoGarbageCollection true

#define DDoTest_Message_DispatchVector 1
#define DDoTest_Message_Dispatch 1
#define DDoTest_Message_DispatchThreaded 1
#define DDoTest_Message_ActorDiscard 1
#define DDoTest_Message_ActorDirect 1
#define DDoTest_Message_ActorToOther 1
#define DDoTest_Message_ActorToOtherCoroutine 0
#define DDoTest_Message_ActorToSame 1
#define DDoTest_Message_ActorToSameCoroutine 1
#define DDoTest_Message_PCActorToDiscard 1
#define DDoTest_Message_PCActorToDiscardCoro 1
#define DDoTest_Message_PCActorToResVector 1
#define DDoTest_Message_PCActorToResVectorCoro 1
#define DDoTest_Message_PCActorToVector 1
#define DDoTest_Message_PCActorToResMap 1
#define DDoTest_Message_PCActorToMap 1
#define DDoTest_Message_CActorToResVector 1
#define DDoTest_Message_CActorToResVectorCoro 1
#define DDoTest_Message_BranchedConcurrentActor 1
#define DDoTest_Message_BranchedConcurrentActorCoroutine 1

#define DDoTest_Create_Vector 1
#define DDoTest_Create_LinkedList 1
#define DDoTest_Create_ThreadSafeQueue 1
#define DDoTest_Create_ActorConcurrent 1
#define DDoTest_Create_Actor 1
#define DDoTest_Create_ActorOutside 1

//#define DDoSleep NSys::fg_Thread_Sleep(0.1f)
#define DDoSleep

namespace
{
	fp64 g_Timeout = 60.0 * NMib::NTest::gc_TimeoutMultiplier;

	using namespace NMib;
	using namespace NMib::NStorage;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	using namespace NMib::NStr;

	constexpr static mint gc_nRepetitions = 111;
	constexpr static mint gc_YieldMask = 0xff;
	// 0b1						742
	// 0b1111 					270
	// 0b11111					255
	// 0b111111					248
	// 0b1111111				244
	// 0b11111111				242
	// 0b111111111				245
	// 0b1111111111				251
	// 0b11111111111			254
	// 0b111111111111			250
	// 0b1111111111111			260
	// 0b11111111111111			291
	// 0b111111111111111		352
	// 0b1111111111111111		481
	// 0b11111111111111111		536
	// 0b111111111111111111 	511
	// 0b1111111111111111111	487


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
		TCActor<CConstructExceptionActor> m_SelfReference;

		void fp_Construct() override
		{
			m_SelfReference = fg_ThisActor(this);
			DMibError("Test error");
		}
	};

	class CPerformanceTestActor : public CActor
	{
		uint32 m_Value = 0;
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

		TCFuture<void> f_AddIntNoCoro(uint32 _Value)
		{
			TCPromise<void> Promise{CPromiseConstructNoConsume()};
			m_Value += _Value;

			return Promise.f_MoveFuture();
		}

		virtual TCFuture<void> f_AddInt(uint32 _Value)
		{
			TCPromise<void> Promise;
			m_Value += _Value;

			return Promise <<= g_Void;
		}

		void f_AddIntNoFuture(uint32 _Value)
		{
			m_Value += _Value;
		}

		uint32 f_AddIntRetNoFuture(uint32 _Value)
		{
			m_Value += _Value;
			return m_Value;
		}

		TCFuture<void> f_AddIntCoro(uint32 _Value)
		{
			m_Value += _Value;

			co_return {};
		}

		uint32 f_GetResult()
		{
			return m_Value;
		}

		uint32 f_ExchangeResult()
		{
			return fg_Exchange(m_Value, 0);
		}
	};

	struct CDestructActorSeparateThread : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		void f_NoFuture()
		{
		}

		TCFuture<void> fp_Destroy()
		{
			fg_CurrentActor();
			co_return {};
		}

		virtual TCFuture<void> f_FutureVirtual()
		{
			{
				fg_CurrentActor();
			}
			co_return {};
		}

		TCFuture<void> f_Future()
		{
			{
				fg_CurrentActor();
			}
			co_return {};
		}
	};

	struct CDestructActor : public CActor
	{
		void f_NoFuture()
		{
			{
				fg_CurrentActor();
			}
		}

		TCFuture<void> fp_Destroy()
		{
			fg_CurrentActor();
			co_return {};
		}

		virtual TCFuture<void> f_FutureVirtual()
		{
			{
				fg_CurrentActor();
			}
			co_return {};
		}

		TCFuture<void> f_Future()
		{
			{
				fg_CurrentActor();
			}
			co_return {};
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
#if DMibEnableSafeCheck > 0
			ThreadLocal.m_pCurrentlyConstructingActor = _pThis;
#endif
			return 0;
		}

		CPerformanceTestActorEmul()
			 : CPerformanceTestActor(fs_Setup(this))
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
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
			TCPromise<uint32> Promise;
			return Promise <<= DMibErrorInstance("Error");
		}

		virtual TCFuture<uint32> f_GetNoError()
		{
			TCPromise<uint32> Promise;
			return Promise <<= 5;
		}

		virtual TCFuture<void> f_GetErrorVoid()
		{
			TCPromise<void> Promise;
			return Promise <<= DMibErrorInstance("Error");
		}

		virtual TCFuture<void> f_GetNoErrorVoid()
		{
			TCPromise<void> Promise;
			return Promise <<= g_Void;
		}

		virtual TCFuture<void> f_GetResultWasNotSet()
		{
			TCPromise<void> Promise;
			return Promise.f_MoveFuture();
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

		virtual TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> _pValue) = 0;
		virtual TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> _pValue) = 0;
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
			TCPromise<void> Promise;
			return Promise <<= g_Void;
		}

		TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> _pValue) override
		{
			TCPromise<uint32> Promise;
			return Promise <<= _pValue->f_ToInt();
		}

		TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> _pValue) override
		{
			TCPromise<uint32> Promise;
			return Promise <<= _pValue->f_ToInt();
		}

		mint m_DummyMember0 = 0;
		mint m_DummyMember1 = 1;
	};

	class CCallbackActor : public CActor
	{
		TCActorFunctor<TCFuture<void> (int32)> m_fCallback;
	public:

		TCFuture<CActorSubscription> f_RegisterCallback(TCActorFunctor<TCFuture<void> (int32)> _fCallback)
		{
			m_fCallback = fg_Move(_fCallback);

			co_return g_ActorSubscription / [this]() -> TCFuture<void>
				{
					co_await fg_Move(m_fCallback).f_Destroy();
					co_return {};
				}
			;
		}

		void f_CallCallback(int32 _Value)
		{
			m_fCallback.f_CallDiscard(_Value);
		}
	};

	TCFuture<mint> fg_ReturnCoroOne()
	{
		co_return mint(1);
	}

	mint fg_ReturnOne()
	{
		return 1;
	}

	class CActor_Tests : public NMib::NTest::CTest
	{
		TCFunctionMovable<void ()> fp_GarbageCollectFunctor(bool _bGarbageCollect)
		{
			return [_bGarbageCollect]
				{
					(void)_bGarbageCollect;
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
	#ifdef DDoGarbageCollection
					if (_bGarbageCollect)
						Checkout.f_GarbageCollectLocalArena(DDoGarbageCollection); // Garbage collect memory
					else
	#endif
	#ifdef DDoCheckMessages
						Checkout.f_CheckMessages();
	#else
						;
	#endif
				}
			;
		}

		void fp_BlockOnAllThreads(TCFutureVector<void> &_Results, bool _bGarbageCollect)
		{
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
			for (mint i = 0; i < nThreads; ++i)
				fg_ConcurrentDispatch(fp_GarbageCollectFunctor(_bGarbageCollect)) > _Results;
		}
		void fp_BlockOnAllThreads(bool _bGarbageCollect)
		{
			mint nThreads = NSys::fg_Thread_GetVirtualCores();
			TCFutureVector<void> Results;
			Results.f_SetLen(nThreads);

			fp_BlockOnAllThreads(Results, _bGarbageCollect);
			fg_AllDoneWrapped(Results).f_CallSync();
			fp_GarbageCollectFunctor(_bGarbageCollect)();
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
						(fg_TempCopy(Actor).f_Destroy() + fg_TempCopy(Actor).f_Destroy()).f_CallSync(g_Timeout / 2);
					}
				;

				DMibExpectException
					(
						fDoubleDestroy()
						, DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed")
						, DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted")
					)
				;
			};
			DMibTestSuite("DestructRefCount") -> TCFuture<void>
			{
				{
					DMibTestPath("Future");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActor> Actor{fg_Construct()};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActor::f_Future>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}
				{
					DMibTestPath("FutureVirtual");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActor> Actor{fg_Construct()};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActor::f_FutureVirtual>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}
				{
					DMibTestPath("NoFuture");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActor> Actor{fg_Construct()};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActor::f_NoFuture>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}
				{
					DMibTestPath("SeparateThreadFuture");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActorSeparateThread> Actor{fg_Construct(), "TestThread"};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActorSeparateThread::f_Future>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}
				{
					DMibTestPath("SeparateThreadFutureVirtual");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActorSeparateThread> Actor{fg_Construct(), "TestThread"};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActorSeparateThread::f_FutureVirtual>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}
				{
					DMibTestPath("SeparateThreadNoFuture");
					bool bCrashed = false;
					TCFutureVector<void> Results;
					for (mint i = 0; i < 200; ++i)
					{
						TCActor<CDestructActorSeparateThread> Actor{fg_Construct(), "TestThread"};

						for (mint i = 0; i < 200; ++i)
							Actor.f_Bind<&CDestructActorSeparateThread::f_NoFuture>() > Results;

						g_Dispatch(fg_ConcurrentActor()) / [Actor = fg_Move(Actor)]
							{
							}
							> g_DiscardResult
						;
					}

					DMibExpectFalse(bCrashed);

					co_await fg_AllDone(Results);
				}

				co_return {};
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
					uint32 SharedPointerResult = fg_Move(SharedPointerFuture).f_CallSync(g_Timeout);
					DMibExpect(SharedPointerResult, ==, 5);

					TCFuture<uint32> UniquePointerFuture = TestActor(&CBaseActor::f_UniquePointer, fg_Construct(CStr("5")));
					uint32 UniquePointerResult = fg_Move(UniquePointerFuture).f_CallSync(g_Timeout);
					DMibExpect(UniquePointerResult, ==, 5);
				}
				{
					DMibTestPath("Weak actor");
					TCActor<CBaseActor> TestActor = fg_ConstructActor<CDerivedActor>();
					TCWeakActor<CBaseActor> WeakTestActor = TestActor;

					uint32 Value = WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(g_Timeout);
					DMibExpect(Value, ==, 2);;

					fg_Move(TestActor).f_Destroy().f_CallSync();

					DMibExpectExceptionType(WeakTestActor(&CBaseActor::f_GetSpecificValue, 2).f_CallSync(g_Timeout), NException::CException);
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
							[TestActor]() -> TCFuture<uint32>
							{
								TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoError) > Promise / [Promise](uint32 _Result)
									{
										Promise.f_SetResult(_Result);
									}
								;

								co_return co_await Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetError) > Promise / [Promise](uint32 _Result)
											{
												Promise.f_SetResult(_Result);
											}
										;
										co_return co_await Promise.f_MoveFuture();
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetError) > Promise % _Error / [Promise](uint32 _Result)
											{
												Promise.f_SetResult(_Result);
											}
										;
										co_return co_await Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Forward errors to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<void>
							{
								TCPromise<void> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > Promise / [Promise]()
									{
										Promise.f_SetResult();
									}
								;
								co_await Promise.f_MoveFuture();

								co_return {};
							}
						)
					;

					auto fCallWithError = [&]
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCFuture<void>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										co_await Promise.f_MoveFuture();

										co_return {};
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise % _Error / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										co_await Promise.f_MoveFuture();

										co_return {};
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Forward call pack errors to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
							{
								TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoError)
									+ TestActor(&CExceptionActor::f_GetNoError)
									> Promise / [Promise](uint32 _Result0, uint32 _Result1)
									{
										Promise.f_SetResult(_Result0);
									}
								;
								co_return co_await Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetError)
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise / [Promise](uint32 _Result0, uint32 _Result1)
											{
												Promise.f_SetResult(_Result0);
											}
										;
										co_return co_await Promise.f_MoveFuture();
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetNoError)
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise % _Error / [Promise](uint32 _Result0, uint32 _Result1)
											{
												Promise.f_SetResult(_Result0);
											}
										;
										co_return co_await Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;

					auto fCallWithErrorStringBind = [&](auto _Error)
						{
							fDispatchBoilerplate
								(
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor.f_Bind<&CExceptionActor::f_GetNoError>()
											+ TestActor.f_Bind<&CExceptionActor::f_GetError>()
											> Promise % _Error / [Promise](uint32 _Result0, uint32 _Result1)
											{
												Promise.f_SetResult(_Result0);
											}
										;
										co_return co_await Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorStringBind("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorStringBind(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Forward call pack errors to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
							{
								TCPromise<void> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoErrorVoid)
									+ TestActor(&CExceptionActor::f_GetNoErrorVoid)
									> Promise / [Promise]()
									{
										Promise.f_SetResult();
									}
								;
								co_await Promise.f_MoveFuture();

								co_return {};
							}
						)
					;

					auto fCallWithError = [&]
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCFuture<uint32>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetErrorVoid)
											+ TestActor(&CExceptionActor::f_GetError)
											> Promise / [Promise](CVoidTag, uint32 _Value)
											{
												Promise.f_SetResult();
											}
										;
										co_await Promise.f_MoveFuture();

										co_return {};
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetNoErrorVoid)
											+ TestActor(&CExceptionActor::f_GetErrorVoid)
											> Promise % _Error / [Promise]()
											{
												Promise.f_SetResult();
											}
										;
										co_await Promise.f_MoveFuture();

										co_return {};
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Forward result to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
							{
								TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoError) > fg_TempCopy(Promise);
								co_return co_await Promise.f_MoveFuture();
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
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetError) > fg_TempCopy(Promise);
										co_return co_await Promise.f_MoveFuture();
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<uint32> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetError) > Promise % _Error;
										co_return co_await Promise.f_MoveFuture();
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Forward result to promise void");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
							{
								TCPromise<void> Promise{CPromiseConstructNoConsume()};
								TestActor(&CExceptionActor::f_GetNoErrorVoid) > fg_TempCopy(Promise);
								co_await Promise.f_MoveFuture();

								co_return {};
							}
						)
					;

					auto fCallWithError = [&]
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCFuture<uint32>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetErrorVoid) > fg_TempCopy(Promise);
										co_await Promise.f_MoveFuture();

										co_return {};
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
									[TestActor, _Error]() -> TCFuture<uint32>
									{
										TCPromise<void> Promise{CPromiseConstructNoConsume()};
										TestActor(&CExceptionActor::f_GetErrorVoid) > Promise % _Error;
										co_await Promise.f_MoveFuture();

										co_return {};
									}
								)
							;
						}
					;
					DMibExpectException
						(
							fCallWithErrorString("Failed important call")
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
					DMibExpectException
						(
							fCallWithErrorString(NStr::CStr("Failed important call"))
							, DMibErrorInstanceWrapped("Failed important call: Error", DMibErrorInstance("Error").f_ExceptionPointer())
						)
					;
				}
				{
					DMibTestPath("Return actor call to promise");
					TCActor<CExceptionActor> TestActor = fg_ConstructActor<CExceptionActor>();

					uint32 Value = fDispatchBoilerplate
						(
							[TestActor]() -> TCFuture<uint32>
							{
								co_return co_await TestActor(&CExceptionActor::f_GetNoError);
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
										co_return co_await TestActor(&CExceptionActor::f_GetError);
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
								co_return co_await TestActor(&CExceptionActor::f_GetNoErrorVoid);
							}
						)
					;

					auto fCallWithError = [&]
						{
							fDispatchBoilerplate
								(
									[TestActor]() -> TCFuture<void>
									{
										co_return co_await TestActor(&CExceptionActor::f_GetErrorVoid);
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
						, g_ActorFunctor(fg_ConcurrentActor()) / [&](int32 _Value) -> TCFuture<void>
						{
							CallbackValue = _Value;
							FinishedEvent.f_Signal();

							co_return {};
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

				TestActor.f_Bind<&CCallbackActor::f_CallCallback>(32).f_DiscardResult();

				FinishedEvent.f_Wait();
				{
					DMibLock(Lock);
				}

				DMibExpect(CallbackValue.f_Load(), ==, 32);
			};
		}

		static TCFuture<uint32> DMibWorkaroundUBSanSectionErrors fs_BranchedConcurrentCoroutine(uint32 _Start, uint32 _End)
		{
			if (_End - _Start == 1)
				co_return 1;

			auto [Result0, Result1] = co_await
				(
					fg_ConcurrentActor().f_Bind<&fs_BranchedConcurrentCoroutine>(_Start, _Start + (_End - _Start) / 2)
					+ fg_ConcurrentActor().f_Bind<&fs_BranchedConcurrentCoroutine>(_Start + (_End - _Start) / 2, _End)
				)
			;

			co_return Result0 + Result1;
		}

		void f_PerformanceTests_Message()
		{
			DMibTestSuite(CTestCategory("Message") << CTestGroup("Performance"))
			{
				auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
				mint nIterations = 1'000;
#elif DMibConfig_RefCountDebugging
				mint nIterations = 10'000;
#elif defined(DConfig_ReleaseDebug)
				mint nIterations = 500'000;
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
									(void)ActorEmul.f_AddIntNoCoro(uint32(1));
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
										(void)ActorEmul.f_AddIntNoCoro(uint32(1));
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
										(void)ActorEmul.f_AddIntNoCoro(uint32(1));
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

					DMibConOut2("NPrivate::TCPromiseData<void>: {}\n", sizeof(NConcurrency::NPrivate::TCPromiseData<void>));
					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};

					uint32 Result = 0;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddInt>(uint32(1)).f_DiscardResult();
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					(g_Dispatch(LaunchActor) / fp_GarbageCollectFunctor(true)).f_CallSync();

					PerfTest.f_Add(ActorMeasure);
				}();
				[&]() inline_never
				{
					DMibTestPath("Actor Discard Coro");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Discard Coro");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();

					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};

					uint32 Result = 0;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddIntCoro>(uint32(1)).f_DiscardResult();
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					(g_Dispatch(LaunchActor) / fp_GarbageCollectFunctor(true)).f_CallSync();

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_ActorDirect
				DDoSleep;
				[&]() inline_never
				{
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();

					DMibTestPath("Actor Direct");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Direct");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};

					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DDoSleep;
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddInt>(uint32(1)) > g_DirectResult / [](TCAsyncResult<void> &&)
											{
											}
										;
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					(g_Dispatch(LaunchActor) / fp_GarbageCollectFunctor(true)).f_CallSync();

					PerfTest.f_Add(ActorMeasure);
				}();
				DDoSleep;
				[&]() inline_never
				{
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();

					DMibTestPath("Actor Direct NoFuture");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Direct NoFuture");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};

					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DDoSleep;
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)) > g_DirectResult / [](TCAsyncResult<void> &&)
											{
											}
										;
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					(g_Dispatch(LaunchActor) / fp_GarbageCollectFunctor(true)).f_CallSync();

					PerfTest.f_Add(ActorMeasure);
				}();
				DDoSleep;
				[&]() inline_never
				{
					auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();

					DMibTestPath("Actor Direct Coro");
					fp_BlockOnAllThreads(true);
					CTestPerformanceMeasure ActorMeasure("Actor->Direct Coro");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					TCActor<CSeparateThreadActor> LaunchActor{fg_Construct(), "Test"};

					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DDoSleep;
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddIntCoro>(uint32(1)) > g_DirectResult / [](TCAsyncResult<void> &&)
											{
											}
										;
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					(g_Dispatch(LaunchActor) / fp_GarbageCollectFunctor(true)).f_CallSync();

					PerfTest.f_Add(ActorMeasure);
				}();
				DDoSleep;
#endif
#if DDoTest_Message_ActorToOther
				[&]() inline_never
				{
					DMibTestPath("Actor->Other");
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull;

					CTestPerformanceMeasure ActorMeasure("Actor->Other");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
					TCActor<CActor> LaunchActor{fg_Construct()};

					uint32 Result = 0;;
					auto ReplyActor = fg_ConstructActor<CActor>();
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, 2);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddInt>(uint32(1)) > ReplyActor / [](TCAsyncResult<void> &&)
											{
											}
										;
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_ActorToOtherCoroutine
				[&]() inline_never
				{
					DMibTestPath("Actor->Other Coro");
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull;
					TCActor<CActor> LaunchActor{fg_Construct()};

					CTestPerformanceMeasure ActorMeasure("Actor->Other Coro");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();

					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, 2);
							(
								g_Dispatch(LaunchActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										co_await PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddInt>(uint32(1));
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
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
							(
								g_Dispatch(PerfTestActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddInt>(uint32(1)) > PerfTestActor / [](TCAsyncResult<void> &&)
											{
											}
										;
									}

									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_ActorToSameCoroutine
				[&]() inline_never
				{
					DMibTestPath("Actor->Same Coro");
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull;

					CTestPerformanceMeasure ActorMeasure("Actor->Same Coro");
					TCActor<CPerformanceTestActor> PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();

					uint32 Result = 0;;
					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasure(ActorMeasure, nIterations);
							(
								g_Dispatch(PerfTestActor) / [&]() -> TCFuture<void>
								{
									for (mint i = 0; i < nIterations; ++i)
									{
										if ((i & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										co_await PerfTestActor.f_Bind<&CPerformanceTestActor::f_AddIntCoro>(uint32(1));
									}
									co_return {};
								}
							)
							.f_CallSync()
						;

						Result = PerfTestActor.f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
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
						TCFutureVector<void> Results;
						Results.f_SetLen(nSplits);
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<void>
									{
										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											(*pActor).f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)).f_DiscardResult();
										}

										co_return {};
									}
								)
								> Results;
							;
						}
						fg_AllDoneWrapped(Results).f_CallSync();
						Result = PerfTestActors[0].f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						for (mint iSplit = 1; iSplit < nSplits; ++iSplit)
							Result += PerfTestActors[iSplit].f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToDiscardCoro
				[&]() inline_never
				{
					DMibTestPath("PC Actor->DiscardCo");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("PC Actor->DiscardCo");

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
						TCFutureVector<void> Results;
						Results.f_SetLen(nSplits);
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<void>
									{
										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											(*pActor).f_Bind<&CPerformanceTestActor::f_AddIntCoro>(uint32(1)).f_DiscardResult();
										}

										co_return {};
									}
								)
								> Results;
							;
						}
						fg_AllDoneWrapped(Results).f_CallSync();
						Result = PerfTestActors[0].f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						for (mint iSplit = 1; iSplit < nSplits; ++iSplit)
							Result += PerfTestActors[iSplit].f_Bind<&CPerformanceTestActor::f_GetResult>().f_CallSync();
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToResMap
				[&]() inline_never
				{
					DMibTestPath("PC Actor->ResMap");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("PC Actor->ResMap");

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
						TCFutureVector<uint32> Dispatches;
						Dispatches.f_SetLen(nSplits);

						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<uint32>
									{
										TCFutureMap<uint32, void> ResultMap;

										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											pActor->f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)) > ResultMap[i];
										}

										co_await fg_AllDoneWrapped(ResultMap);

										co_return co_await pActor->f_Bind<&CPerformanceTestActor::f_ExchangeResult>();
									}
								)
								> Dispatches;
							;
						}
						auto DispatchResults = fg_AllDoneWrapped(Dispatches).f_CallSync();
						for (auto &DispatchResult : DispatchResults)
							Result += *DispatchResult;
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToMap
				[&]() inline_never
				{
					DMibTestPath("PC Actor->Map");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("PC Actor->Map");

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
						TCFutureVector<uint32> Dispatches;
						Dispatches.f_SetLen(nSplits);

						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<uint32>
									{
										TCMapOfFutures<uint32, void> ResultMap;

										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											pActor->f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)) > ResultMap(i);
										}

										co_await fg_AllDoneWrapped(ResultMap);

										co_return co_await pActor->f_Bind<&CPerformanceTestActor::f_ExchangeResult>();
									}
								)
								> Dispatches;
							;
						}
						auto DispatchResults = fg_AllDoneWrapped(Dispatches).f_CallSync();
						for (auto &DispatchResult : DispatchResults)
							Result += *DispatchResult;
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToVector
				[&]() inline_never
				{
					DMibTestPath("PC Actor->Vector");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("PC Actor->Vector");

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
						TCVectorOfFutures<uint32> Dispatches;
						Dispatches.f_Reserve(nSplits);

						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<uint32>
									{
										TCVectorOfFutures<void> ResultVector;
										ResultVector.f_Reserve(nIterationsPerSplit);

										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											pActor->f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)) > ResultVector;
										}

										co_await fg_AllDoneWrapped(ResultVector);

										co_return co_await pActor->f_Bind<&CPerformanceTestActor::f_ExchangeResult>();
									}
								)
								> Dispatches;
							;
						}
						auto DispatchResults = fg_AllDoneWrapped(Dispatches).f_CallSync();
						for (auto &DispatchResult : DispatchResults)
							Result += *DispatchResult;
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
						TCFutureVector<uint32> Dispatches;
						Dispatches.f_SetLen(nSplits);

						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<uint32>
									{
										TCFutureVector<void> ResultVector;
										ResultVector.f_SetLen(nIterationsPerSplit);

										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											pActor->f_Bind<&CPerformanceTestActor::f_AddIntNoFuture>(uint32(1)) > ResultVector;
										}

										co_await fg_AllDoneWrapped(ResultVector);

										co_return co_await pActor->f_Bind<&CPerformanceTestActor::f_ExchangeResult>();
									}
								)
								> Dispatches;
							;
						}
						auto DispatchResults = fg_AllDoneWrapped(Dispatches).f_CallSync();
						for (auto &DispatchResult : DispatchResults)
							Result += *DispatchResult;
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_PCActorToResVectorCoro
				[&]() inline_never
				{
					DMibTestPath("PC Actor->ResVecCo");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("PC Actor->ResVecCo");

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
						TCFutureVector<uint32> Dispatches;
						Dispatches.f_SetLen(nSplits);

						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							auto *pActor = &PerfTestActors[iSplit];
							fg_ConcurrentDispatch
								(
									[nIterationsPerSplit, pActor]() -> TCFuture<uint32>
									{
										TCFutureVector<void> ResultVector;
										ResultVector.f_SetLen(nIterationsPerSplit);

										for (mint i = 0; i < nIterationsPerSplit; ++i)
										{
											if ((i & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											pActor->f_Bind<&CPerformanceTestActor::f_AddIntCoro>(uint32(1)) > ResultVector;
										}

										co_await fg_AllDoneWrapped(ResultVector);

										co_return co_await pActor->f_Bind<&CPerformanceTestActor::f_ExchangeResult>();
									}
								)
								> Dispatches;
							;
						}
						auto DispatchResults = fg_AllDoneWrapped(Dispatches).f_CallSync();
						for (auto &DispatchResult : DispatchResults)
							Result += *DispatchResult;
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
						TCFutureVector<TCVector<mint>> Dispatches;
						Dispatches.f_SetLen(nSplits);

						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							fg_ConcurrentDispatch
								(
									[&]() -> TCFuture<TCVector<mint>>
									{
										auto &ConcurrentActor = fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal);
										TCFutureVector<mint> Results;
										Results.f_SetLen(nIterationsPerSplit);
										for (mint iIteration = 0; iIteration < nIterationsPerSplit; ++iIteration)
										{
											if ((iIteration & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											ConcurrentActor.f_Bind<&fg_ReturnOne>() > Results;
										}

										co_return co_await fg_AllDone(Results);
									}
								)
								> Dispatches;
							;
						}
						for (auto &ResultVector : fg_AllDoneWrapped(Dispatches).f_CallSync())
						{
							for (auto &Results : *ResultVector)
								Result += Results;
						}

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_CActorToResVectorCoro
				[&]() inline_never
				{
					DMibTestPath("C Actor->ResVectorC");
					fp_BlockOnAllThreads(true);

					CTestPerformanceMeasure ActorMeasure("C Actor->ResVectorC");

					uint32 Result = 0;
					mint nIterations = nIterationsFull;
					mint nSplits = NSys::fg_Thread_GetVirtualCores();
					mint nIterationsPerSplit = nIterations / nSplits;
					nIterations = nIterationsPerSplit * nSplits;

					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterations, NSys::fg_Thread_GetPhysicalCores());
						TCFutureVector<TCVector<mint>> Dispatches;
						Dispatches.f_SetLen(nSplits);

						for (mint iSplit = 0; iSplit < nSplits; ++iSplit)
						{
							fg_ConcurrentDispatch
								(
									[&]() -> TCFuture<TCVector<mint>>
									{
										auto &ConcurrentActor = fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal);
										TCFutureVector<mint> Results;
										Results.f_SetLen(nIterationsPerSplit);
										for (mint iIteration = 0; iIteration < nIterationsPerSplit; ++iIteration)
										{
											if ((iIteration & gc_YieldMask) == gc_YieldMask)
												co_await g_Yield;

											ConcurrentActor.f_Bind<&fg_ReturnCoroOne>() > Results;
										}

										co_return co_await fg_AllDone(Results);
									}
								)
								> Dispatches;
							;
						}
						for (auto &ResultVector : fg_AllDoneWrapped(Dispatches).f_CallSync())
						{
							for (auto &Results : *ResultVector)
								Result += Results;
						}

						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_BranchedConcurrentActor && !defined(DCompiler_Workaround_Apple_clang)
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull / 2;
					DMibTestPath("Branched");
					CTestPerformanceMeasure ActorMeasure("Branched");
					auto fActor = [](this auto &_fThis, uint32 _Start, uint32 _End) -> TCUnsafeFuture<uint32>
						{
							if ((_End - _Start) == 1)
								co_return 1;

							auto [Result0, Result1] = co_await
								(
									fg_ConcurrentActor().f_Bind<&CActor::f_DispatchWithReturn<TCFuture<uint32>, uint32, uint32>>
									(
										_fThis
										, _Start
										, _Start + (_End - _Start) / 2
									)
									+ fg_ConcurrentActor().f_Bind<&CActor::f_DispatchWithReturn<TCFuture<uint32>, uint32, uint32>>
									(
										_fThis
										, _Start + (_End - _Start) / 2
										, _End
									)
								)
							;
							co_return Result0 + Result1;
						}
					;

					uint32 Result = 0;

					auto ConcurrentActor = fg_ConcurrentActor();

					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterationsFull, NSys::fg_Thread_GetPhysicalCores());
						Result += fg_ConcurrentActor().f_Bind<&CActor::f_DispatchWithReturn<TCFuture<uint32>>>
							(
								[&]() -> TCFuture<uint32>
								{
									co_return co_await fActor(0, nIterations);
								}
							).f_CallSync(g_Timeout / 3);
						;
						fp_BlockOnAllThreads(false);
					}

					DMibExpect(Result, ==, nIterations*gc_nRepetitions);

					PerfTest.f_Add(ActorMeasure);
				}();
#endif
#if DDoTest_Message_BranchedConcurrentActorCoroutine
				[&]() inline_never
				{
					fp_BlockOnAllThreads(true);
					mint nIterations = nIterationsFull / 2;
					DMibTestPath("Branched Coro");
					CTestPerformanceMeasure ActorMeasure("Branched Coro");

					uint32 Result = 0;

					for (mint i = 0; i < gc_nRepetitions; ++i)
					{
						DMibTestScopeMeasureThreads(ActorMeasure, nIterationsFull, NSys::fg_Thread_GetPhysicalCores());
						Result +=
							(
								g_ConcurrentDispatch / [&]() -> TCFuture<uint32>
								{
									co_return co_await fs_BranchedConcurrentCoroutine(0, nIterations);
								}
							)
							.f_CallSync(g_Timeout / 3)
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
			NMemory::CMemoryManagerCheckout &m_Checkout;
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
			CCurrentActorScope ProcessingScope(ThreadLocal, &Holder);

			TCVector<TCOptional<CPerformanceTestActorEmul>> Actors;
			Actors.f_SetLen(nIterations);

			for (mint i = 0; i < gc_nRepetitions; ++i)
			{
				{
					DMibTestScopeMeasure(DispatchMeasure, nIterations);
					for (auto &Actor : Actors)
						Actor.f_Set<1>();

				}
				{
					DMibTestScopeMeasure(DispatchMeasureDestroy, nIterations);
					for (auto &Actor : Actors)
						Actor.f_Set<0>();
				}
			}
			Actors.f_Clear();
#ifdef DDoGarbageCollection
			_CreateContext.m_Checkout.f_GarbageCollectLocalArena(DDoGarbageCollection); // Garbage collect memory
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
			CCurrentActorScope ProcessingScope(ThreadLocal, &Holder);

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
			_CreateContext.m_Checkout.f_GarbageCollectLocalArena(DDoGarbageCollection); // Garbage collect memory
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

			CCurrentActorScope ProcessingScope(ThreadLocal, &Holder);

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
			_CreateContext.m_Checkout.f_GarbageCollectLocalArena(DDoGarbageCollection); // Garbage collect memory
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
			CTestPerformanceMeasure ActorMeasureDestroy("D ActorConcurent");
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
					TCFutureVector<void> ThreadResults;
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
									{
										if (((pPerfTestActor - pArray) & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										*pPerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
									}

									TCFutureVector<void> Results;
									fp_BlockOnAllThreads(Results, false);
									co_await fg_AllDone(Results);

									co_return {};
								}
							)
							> ThreadResults
						;
					}
					fg_AllDoneWrapped(ThreadResults).f_CallSync();
					fp_BlockOnAllThreads(false);
				}
				{
					TCFutureVector<void> ThreadResults;
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
									TCFutureVector<void> Results;
									Results.f_SetLen(nIterationsPerThread);

									for (auto pPerfTestActor = pArray + iStart; pPerfTestActor != pEnd; ++pPerfTestActor)
									{
										if (((pPerfTestActor - pArray) & gc_YieldMask) == gc_YieldMask)
											co_await g_Yield;

										fg_Move(*pPerfTestActor).f_Destroy() > Results;
									}

									co_await fg_AllDone(Results);

									TCFutureVector<void> BlockResults;
									BlockResults.f_SetLen(nThreads);
									fp_BlockOnAllThreads(BlockResults, false);

									co_await fg_AllDone(BlockResults);

									co_return {};
								}
							)
							> ThreadResults
						;
					}
					fg_AllDoneWrapped(ThreadResults).f_CallSync();
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
			CTestPerformanceMeasure ActorMeasureDestroy("D Actor");
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
							, [&]() -> TCFuture<void>
							{
								mint i = 0;
								for (auto &PerfTestActor : PerfTestActors)
								{
									if ((i & gc_YieldMask) == gc_YieldMask)
										co_await g_Yield;
									++i;

									PerfTestActor = fg_ConstructActor<CPerformanceTestActor>();
								}

								co_return {};
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
							, [&]() -> TCFuture<void>
							{
								TCFutureVector<void> Results;
								Results.f_SetLen(PerfTestActors.f_GetLen());
								mint i = 0;
								for (auto &PerfTestActor : PerfTestActors)
								{
									if ((i & gc_YieldMask) == gc_YieldMask)
										co_await g_Yield;
									++i;

									fg_Move(PerfTestActor).f_Destroy() > Results;
								}

								co_await fg_AllDone(Results);

								co_return {};
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
			DMibTestPath("Actor Outside");
			CTestPerformanceMeasure ActorMeasure("ActorOutside");
			CTestPerformanceMeasure ActorMeasureDestroy("D ActorOutside");
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
					DMibTestScopeMeasure(ActorMeasureDestroy, nIterations);
					TCFutureVector<void> Results;
					Results.f_SetLen(PerfTestActors.f_GetLen());

					for (auto &PerfTestActor : PerfTestActors)
						fg_Move(PerfTestActor).f_Destroy() > Results;

					fg_AllDoneWrapped(Results).f_CallSync();
					fp_BlockOnAllThreads(false);
				}
			}
#ifdef DDoGarbageCollection
			_CreateContext.m_Checkout.f_GarbageCollectLocalArena(DDoGarbageCollection); // Garbage collect memory
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
#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
				mint nIterations = 1'000;
#elif DMibConfig_RefCountDebugging
				mint nIterations = 10'000;
#else
				mint nIterations = 1'000'000;
#endif
				[[maybe_unused]] mint nIterationsFull = nIterations;
				CTestPerformance PerfTest(0.1);
				CTestPerformance PerfTestDestroy(0.01);
				CCreateContext CreateContext{PerfTest, PerfTestDestroy, nIterations, Checkout};

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
