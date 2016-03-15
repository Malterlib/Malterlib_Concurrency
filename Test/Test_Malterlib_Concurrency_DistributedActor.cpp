// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Test/Performance>
#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorCallbackManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Web/WebSocket>

namespace
{
	using namespace NMib;
	using namespace NMib::NPtr;
	using namespace NMib::NMeta;
	using namespace NMib::NContainer;
	using namespace NMib::NConcurrency;
	using namespace NMib::NThread;
	using namespace NMib::NFunction;
	
	namespace NTest1
	{
		struct CTest
		{
		};
	}
	
	namespace NTest2
	{
		struct CTest
		{
		};
	}

	class CDistributedActorBase : public CDistributedActor
	{
	public:
		virtual void f_AddIntVirtual(uint32 _Value) pure;
		virtual uint32 f_GetResultVirtual() pure;
	};

	class CDistributedActor : public CDistributedActorBase
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

		void f_AddIntVirtual(uint32 _Value) override
		{
			m_Value += _Value;
		}
		
		uint32 f_GetResultVirtual() override
		{
			return m_Value;
		}
		
		template <typename tf_CTest>
		uint32 f_GetResultTemplated()
		{
			return m_Value;
		}
		

		TCContinuation<uint32> f_GetResultDeferred()
		{
			TCContinuation<uint32> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Continuation](TCAsyncResult<uint32> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntDeferred(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Continuation](TCAsyncResult<void> &&_Result)
				{
					Continuation.f_SetResult(fg_Move(_Result));
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> f_GetResultException()
		{
			TCContinuation<uint32> Continuation;
			Continuation.f_SetException(DMibErrorInstance("Test"));
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntException(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			Continuation.f_SetException(DMibErrorInstance("Test"));
			return Continuation;
		}

		TCContinuation<uint32> f_GetResultDeferredException()
		{
			TCContinuation<uint32> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Continuation](TCAsyncResult<uint32> &&_Result)
				{
					Continuation.f_SetException(DMibErrorInstance("Test"));
				}
			;
			return Continuation;
		}
		
		TCContinuation<void> f_AddIntDeferredException(uint32 _Value)
		{
			TCContinuation<void> Continuation;
			fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Continuation](TCAsyncResult<void> &&_Result)
				{
					Continuation.f_SetException(DMibErrorInstance("Test"));
				}
			;
			return Continuation;
		}
	};
	
	class CDistributedActor_Tests : public NMib::NTest::CTest
	{
	public:
		void f_FunctionalTests()
		{
			DMibTestCategory("Local")
			{
				DMibTestSuite("Direct")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					DMibCallActor(Actor, CDistributedActor::f_AddInt, 5).f_CallSync();
					
					uint32 Result = DMibCallActor(Actor, CDistributedActor::f_GetResult).f_CallSync();
					DMibExpect(Result, ==, 5);
				};
				DMibTestSuite("DirectTemplate")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					DMibCallActor(Actor, CDistributedActor::f_AddInt, 5).f_CallSync();

					uint32 Result1 = DMibCallActor(Actor, CDistributedActor::f_GetResultTemplated<NTest1::CTest>).f_CallSync();
					DMibExpect(Result1, ==, 5);
					uint32 Result2 = DMibCallActor(Actor, CDistributedActor::f_GetResultTemplated<NTest2::CTest>).f_CallSync();
					DMibExpect(Result2, ==, 5);
				};
				DMibTestSuite("Virtual")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					DMibCallActor(Actor, CDistributedActorBase::f_AddIntVirtual, 5).f_CallSync();

					uint32 Result = DMibCallActor(Actor, CDistributedActorBase::f_GetResultVirtual).f_CallSync();
					DMibExpect(Result, ==, 5);
				};
				DMibTestSuite("VirtualActor")
				{
					TCDistributedActor<CDistributedActorBase> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					
					DMibCallActor(Actor, CDistributedActorBase::f_AddIntVirtual, 5).f_CallSync();

					uint32 Result = DMibCallActor(Actor, CDistributedActorBase::f_GetResultVirtual).f_CallSync();
					DMibExpect(Result, ==, 5);
				};
				DMibTestSuite("Deferred")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					DMibCallActor(Actor, CDistributedActor::f_AddIntDeferred, 5).f_CallSync();
					
					uint32 Result = DMibCallActor(Actor, CDistributedActor::f_GetResultDeferred).f_CallSync();
					DMibExpect(Result, ==, 5);
				};
				DMibTestSuite("Exception")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					auto fTestCall = [&]
						{
							DMibCallActor(Actor, CDistributedActor::f_AddIntException, 5).f_CallSync();
						}
					;
					auto fTestResult = [&]
						{
							return DMibCallActor(Actor, CDistributedActor::f_GetResultException).f_CallSync();
						}
					;

					DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
					DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
				};
				DMibTestSuite("DeferredException")
				{
					TCDistributedActor<CDistributedActor> Actor = fg_ConstructDistributedActor<CDistributedActor>();
					auto fTestCall = [&]
						{
							DMibCallActor(Actor, CDistributedActor::f_AddIntDeferredException, 5).f_CallSync();
						}
					;
					auto fTestResult = [&]
						{
							return DMibCallActor(Actor, CDistributedActor::f_GetResultDeferredException).f_CallSync();
						}
					;

					DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
					DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
				};
			};
		}

		void f_DoTests()
		{
			f_FunctionalTests();
		}
	};

	DMibTestRegister(CDistributedActor_Tests, Malterlib::Concurrency);
}
