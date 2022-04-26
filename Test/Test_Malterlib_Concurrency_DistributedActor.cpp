// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Test/Performance>
#include <Mib/Test/Exception>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Cryptography/RandomID>
#include <Mib/Web/WebSocket>
#include <Mib/Log/Destinations>
#include <Mib/Function/Function>
#include <Mib/File/File>

#include <Mib/Concurrency/AsyncGenerator>

using namespace NMib;
using namespace NMib::NStorage;
using namespace NMib::NMeta;
using namespace NMib::NContainer;
using namespace NMib::NConcurrency;
using namespace NMib::NThread;
using namespace NMib::NFunction;
using namespace NMib::NStr;
using namespace NMib::NThread;
using namespace NMib::NAtomic;
using namespace NMib::NTest;

static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;

namespace NTest1
{
	struct CTest
	{
		void f_TestFunc0()
		{
		}

		TCVector<CStr> f_TestTemplateTypes(TCSet<CStr> const &)
		{
			return {};
		}
	};
}

namespace NTest2
{
	struct CTest
	{
		void f_TestFunc0()
		{
		}
	};
}

class CActorSubscriptionReferenceDummy : public CActorSubscriptionReference
{
public:
	TCFuture<void> f_Destroy() override
	{
		co_return {};
	}
};

class CDistributedActorBase : public CActor
{
public:
	enum : uint32
	{
		EProtocolVersion_Min = 0x101
		, EProtocolVersion_Current = 0x101
	};

	CDistributedActorBase()
	{
		DMibPublishActorFunction(CDistributedActorBase::f_AddIntVirtual);
		DMibPublishActorFunction(CDistributedActorBase::f_GetResultVirtual);
		DMibPublishActorFunction(CDistributedActorBase::f_GetCallingHostID);
	}

	virtual void f_AddIntVirtual(uint32 _Value)
	{
	}
	virtual uint32 f_GetResultVirtual() = 0;
	virtual CStr f_GetCallingHostID() const = 0;
};

class CDistributedActorNonPublished : public CDistributedActorBase
{
public:
	CDistributedActorNonPublished()
	{
		DMibPublishActorFunction(CDistributedActorNonPublished::f_SetSubscriptionNonPublished);
	}

	TCFuture<> f_SetSubscriptionNonPublished(CActorSubscription &&_Subscription, bool _bError)
	{
		if (_bError)
			co_return DMibErrorInstance("Error");
		co_return {};
	}
};

class CDistributedActorInterface : public CActor
{
public:
	enum : uint32
	{
		EProtocolVersion_Min = 0x101
		, EProtocolVersion_Current = 0x101
	};

	CDistributedActorInterface()
	{
		DMibPublishActorFunction(CDistributedActorInterface::f_Test);
	}

	uint32 f_Test()
	{
		return 5;
	}
};

constinit TCAtomicAggregate<mint> g_TestValueGetSubscription = {DAggregateInit};

struct CMultipleSubscriptions
{
	TCActorSubscriptionWithID<0> m_Subscription0;
	TCActorSubscriptionWithID<1> m_Subscription1;

	TCActorSubscriptionWithID<> &f_Subscription(uint32 _SequenceID)
	{
		if (_SequenceID == 0)
			return m_Subscription0;
		else
			return (TCActorSubscriptionWithID<0> &)m_Subscription1;
	}

	template <typename tf_CStream>
	void f_Feed(tf_CStream &_Stream) &&
	{
		_Stream << fg_Move(m_Subscription0);
		_Stream << fg_Move(m_Subscription1);
	}

	template <typename tf_CStream>
	void f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Subscription0;
		_Stream >> m_Subscription1;
	}
};

struct CMultipleSubscriptionsVector
{
	TCVector<TCActorSubscriptionWithID<>> m_Subscriptions;

	TCActorSubscriptionWithID<> &f_Subscription(uint32 _SequenceID)
	{
		return m_Subscriptions[_SequenceID];
	}

	template <typename tf_CStream>
	void f_Feed(tf_CStream &_Stream) &&
	{
		_Stream << fg_Move(m_Subscriptions);
	}

	template <typename tf_CStream>
	void f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Subscriptions;
	}
};

class CDistributedActor : public CDistributedActorNonPublished
{
public:
	struct CCallback
	{
		~CCallback()
		{
			*m_pDeleted = true;
		}

		TCActorFunctor<TCFuture<CStr> (CStr const &_Test)> m_fCallback;
		TCSharedPointer<bool> m_pDeleted = fg_Construct(false);
	};

	uint32 m_Value = 0;
	TCMap<CStr, CActorSubscription> m_CallingHostTests;
	TCActorFunctor<TCFuture<CStr> (CStr const &_Test)> m_fCallback;
	TCLinkedList<CCallback> m_Callbacks;

	TCSharedPointer<TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)>> m_Callback[2];
	TCActor<CActor> m_Actor[2];

	TCSharedPointer<TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)>> m_pCallbackNoSub = fg_Construct();
	TCActor<CActor> m_ActorNoSub;

	TCDistributedActor<CDistributedActorInterface> m_Interface;

	CActorSubscription m_Subscription;

	struct CActorFunctor
	{
		TCActorFunctor<TCFuture<CStr> (CStr const &_Test)> m_ActorFunctor;
		bool m_bCleared = false;
	};

	TCVector<CActorFunctor> m_ActorFunctors;

	bool m_bReturnedFunctionActorSubscriptionCalled = false;
	bool m_bGetInterfaceSubscriptionCalled = false;
	bool m_bSetInterfaceSubscriptionCalled = false;

	TCDistributedActorInterface<CDistributedActorInterface> m_SetDistributedActorInterface;

public:
	CDistributedActor()
	{
		m_Callback[0] = fg_Construct();
		m_Callback[1] = fg_Construct();

		DMibPublishActorFunction(CDistributedActor::f_AddInt);
		DMibPublishActorFunction(CDistributedActor::f_GetResult);
		DMibPublishActorFunction(CDistributedActor::f_AddIntVirtual);
		DMibPublishActorFunction(CDistributedActor::f_GetInterface);
		DMibPublishActorFunction(CDistributedActor::f_GetInterfaceSubscriptionCalled);
		DMibPublishActorFunction(CDistributedActor::f_SetInterface);
		DMibPublishActorFunction(CDistributedActor::f_ClearSetInterface);
		DMibPublishActorFunction(CDistributedActor::f_SetInterfaceSubscriptionCalled);
		DMibPublishActorFunction(CDistributedActor::f_CallSetInterface);
		DMibPublishActorFunction(CDistributedActor::f_GetSubscription);
		DMibPublishActorFunction(CDistributedActor::f_SetSubscription);
		DMibPublishActorFunction(CDistributedActor::f_RegisterActorFunctors);
		DMibPublishActorFunction(CDistributedActor::f_RegisterActorFunctorsVector);
		DMibPublishActorFunction(CDistributedActor::f_GetClearedActorFunctors);
		DMibPublishActorFunction(CDistributedActor::f_ClearActorFunctor);
		DMibPublishActorFunction(CDistributedActor::f_ResetClearedActorFunctors);
		DMibPublishActorFunction(CDistributedActor::f_CallActorFunctor);
		DMibPublishActorFunction(CDistributedActor::f_GetActorFunctorWithoutSubscription);
		DMibPublishActorFunction(CDistributedActor::f_GetActorFunctor);
		DMibPublishActorFunction(CDistributedActor::f_GetActorFunctorSubscriptionCalled);
		DMibPublishActorFunction(CDistributedActor::f_ClearSubscription);
		DMibPublishActorFunction(CDistributedActor::f_RegisterCallback);
		DMibPublishActorFunction(CDistributedActor::f_RegisterCallbacks);
		DMibPublishActorFunction(CDistributedActor::f_RegisterManualCallback);
		DMibPublishActorFunction(CDistributedActor::f_RegisterDual);
		DMibPublishActorFunction(CDistributedActor::f_CallDual);
		DMibPublishActorFunction(CDistributedActor::f_RegisterWithError);
		DMibPublishActorFunction(CDistributedActor::f_RegisterNoSubscription);
		DMibPublishActorFunction(CDistributedActor::f_RegisterNoActor);
		DMibPublishActorFunction(CDistributedActor::f_RegisterNoFunction);
		DMibPublishActorFunction(CDistributedActor::f_CallNoSubscription);
		DMibPublishActorFunction(CDistributedActor::f_CallCallback);
		DMibPublishActorFunction(CDistributedActor::f_CallCallbackWithoutDispatch);
		DMibPublishActorFunction(CDistributedActor::f_CallWrong);
		DMibPublishActorFunction(CDistributedActor::f_ClearCallback);
		DMibPublishActorFunction(CDistributedActor::f_CallRegisteredCallback);
		DMibPublishActorFunction(CDistributedActor::f_CallRegisteredCallbackWithReturn);
		DMibPublishActorFunction(CDistributedActor::f_CallRegisteredCallbacksWithReturn);
		DMibPublishActorFunction(CDistributedActor::f_CallbacksEmpty);
		DMibPublishActorFunction(CDistributedActor::f_GetResultVirtual);
		DMibPublishActorFunction(CDistributedActor::f_GetCallingHostID);
		DMibPublishActorFunction(CDistributedActor::f_TestOnDisconnect);
		DMibPublishActorFunction(CDistributedActor::f_HasOnDisconnect);
		//DMibPublishActorFunction(CDistributedActor::f_GetResultTemplated<NTest1::CTest>); // Not supported in clang
		//DMibPublishActorFunction(CDistributedActor::f_GetResultTemplated<NTest2::CTest>); // Not supported in clang
		DMibPublishActorFunction(CDistributedActor::f_GetResultDeferred);
		DMibPublishActorFunction(CDistributedActor::f_AddIntDeferred);
		DMibPublishActorFunction(CDistributedActor::f_GetResultException);
		DMibPublishActorFunction(CDistributedActor::f_AddIntException);
		DMibPublishActorFunction(CDistributedActor::f_GetResultDeferredException);
		DMibPublishActorFunction(CDistributedActor::f_AddIntDeferredException);
		DMibPublishActorFunction(CDistributedActor::f_SharedPointer);
		DMibPublishActorFunction(CDistributedActor::f_UniquePointer);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGenerator);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorSend);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorFunctor);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorFunctorSend);
	}

	void f_AddInt(uint32 _Value)
	{
		m_Value += _Value;
	}

	uint32 f_GetResult()
	{
		auto Ret = m_Value;
		m_Value = 0;
		return Ret;
	}

	void f_AddIntVirtual(uint32 _Value) override
	{
		m_Value += _Value;
	}

	TCDistributedActorInterfaceWithID<CDistributedActorInterface> f_GetInterface()
	{
		m_Interface = fg_ConstructDistributedActor<CDistributedActorInterface>(fg_GetDistributionManager());
		return
			{
				m_Interface->f_ShareInterface<CDistributedActorInterface>()
				, g_ActorSubscription / [this]
				{
					m_bGetInterfaceSubscriptionCalled = true;
				}
			}
		;
	}

	bool f_GetInterfaceSubscriptionCalled()
	{
		bool bRet = m_bGetInterfaceSubscriptionCalled;
		m_bGetInterfaceSubscriptionCalled = false;
		return bRet;
	}

	TCActorSubscriptionWithID<> f_SetInterface(TCDistributedActorInterfaceWithID<CDistributedActorInterface> &&_Interface)
	{
		m_SetDistributedActorInterface = fg_Move(_Interface);
		return g_ActorSubscription / [this]
			{
				m_bSetInterfaceSubscriptionCalled = true;
			}
		;
	}

	void f_ClearSetInterface()
	{
		m_SetDistributedActorInterface.f_Clear();
	}

	bool f_SetInterfaceSubscriptionCalled()
	{
		bool bRet = m_bSetInterfaceSubscriptionCalled;
		m_bSetInterfaceSubscriptionCalled = false;
		return bRet;
	}

	TCFuture<uint32> f_CallSetInterface()
	{
		co_return co_await m_SetDistributedActorInterface.f_CallActor(&CDistributedActorInterface::f_Test)();
	}

	TCFuture<int32> f_TestFuture()
	{
		co_return 7;
	}

	TCAsyncGenerator<int32> f_TestAsyncGenerator()
	{
		co_yield 5;
		co_yield 6;
		co_yield co_await f_TestFuture();
		co_yield 8;

		co_return {};
	}

	TCActorFunctorWithID<TCAsyncGenerator<int32> (int32 const &_Value), gc_SubscriptionNotRequired> f_TestAsyncGeneratorFunctor()
	{
		return g_ActorFunctor(g_ActorSubscription / [] {}) / [this] (int32 const &_Value) -> TCAsyncGenerator<int32>
			{
				co_yield 5;
				co_yield 6;
				co_yield co_await f_TestFuture();
				co_yield 8;
				co_yield _Value;

				co_return {};
			}
		;
	}

	TCFuture<int32> f_TestAsyncGeneratorSend(TCAsyncGenerator<int32> &&_Generator)
	{
		int32 Return = 0;
		for (auto iValue = co_await fg_Move(_Generator).f_GetIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		co_return Return;
	}

	TCFuture<int32> f_TestAsyncGeneratorFunctorSend(TCActorFunctorWithID<TCAsyncGenerator<int32> (int32 const &_Value), gc_SubscriptionNotRequired> &&_fGenerator)
	{
		int32 Return = 0;
		for (auto iValue = co_await (co_await _fGenerator(5)).f_GetIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		for (auto iValue = co_await (co_await _fGenerator(7)).f_GetIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		co_return Return;
	}

	TCFuture<CActorSubscription> f_GetSubscription()
	{
		co_return
			(
				g_ActorSubscription / []
				{
					g_TestValueGetSubscription.f_Exchange(1);
				}
			)
		;
	}

	TCFuture<> f_SetSubscription(CActorSubscription &&_Subscription, bool _bError)
	{
		if (_bError)
			co_return DMibErrorInstance("Error");

		m_Subscription = fg_Move(_Subscription);
		co_return {};
	}

	CMultipleSubscriptions f_RegisterActorFunctors
		(
			TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test), 0> &&_Callback0
			, TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test), 1> &&_Callback1
		)
	{
		CMultipleSubscriptions Subscriptions;
		m_ActorFunctors.f_SetLen(2);
		m_ActorFunctors[0].m_ActorFunctor = fg_Move(_Callback0);
		m_ActorFunctors[1].m_ActorFunctor = fg_Move(_Callback1);

		Subscriptions.m_Subscription0 = g_ActorSubscription / [this]
			{
				m_ActorFunctors[0].m_bCleared = true;
			}
		;
		Subscriptions.m_Subscription1 = g_ActorSubscription / [this]
			{
				m_ActorFunctors[1].m_bCleared = true;
			}
		;
		return Subscriptions;
	}

	CMultipleSubscriptionsVector f_RegisterActorFunctorsVector
		(
			TCVector<TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test), 0>> &&_Callbacks
		)
	{
		CMultipleSubscriptionsVector Subscriptions;

		m_ActorFunctors.f_Clear();
		mint iFunctor = 0;
		for (auto &Callback : _Callbacks)
		{
			Subscriptions.m_Subscriptions.f_Insert
				(
					fg_Construct
					(
						g_ActorSubscription / [this, iFunctor]
						{
							m_ActorFunctors[iFunctor].m_bCleared = true;
						}
						, Callback.f_GetID()
					)
				)
			;
			m_ActorFunctors.f_Insert().m_ActorFunctor = fg_Move(Callback);
			++iFunctor;
		}
		return Subscriptions;
	}

	bool f_GetClearedActorFunctors(uint32 _iCallback) const
	{
		return m_ActorFunctors[_iCallback].m_bCleared;
	}

	void f_ClearActorFunctor(uint32 _iCallback)
	{
		return m_ActorFunctors[_iCallback].m_ActorFunctor.f_Clear();
	}

	void f_ResetClearedActorFunctors()
	{
		m_ActorFunctors.f_Clear();
	}

	TCFuture<CStr> f_CallActorFunctor(uint32 _iCallback, CStr const &_Message)
	{
		co_return co_await m_ActorFunctors[_iCallback].m_ActorFunctor(_Message);
	}

	TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test)> f_GetActorFunctorWithoutSubscription()
	{
		return g_ActorFunctor / [] (CStr const &_Test) -> TCFuture<CStr>
			{
				co_return _Test + "WithoutSubscription";
			}
		;
	}

	TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test)> f_GetActorFunctor()
	{
		return g_ActorFunctor
			(
				g_ActorSubscription / [this]
				{
					m_bReturnedFunctionActorSubscriptionCalled = true;
				}
			)
			/ [] (CStr const &_Test) -> TCFuture<CStr>
			{
				co_return _Test + "WithSubscription";
			}
		;
	}

	bool f_GetActorFunctorSubscriptionCalled()
	{
		bool bRet = m_bReturnedFunctionActorSubscriptionCalled;
		m_bReturnedFunctionActorSubscriptionCalled = false;
		return bRet;
	}

	void f_ClearSubscription()
	{
		m_Subscription.f_Clear();
	}

	TCActorSubscriptionWithID<> f_RegisterCallback(TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test)> &&_fCallback)
	{
		m_fCallback = fg_Move(_fCallback);

		return g_ActorSubscription / [this]() -> TCFuture<void>
			{
				co_await fg_Move(m_fCallback).f_Destroy();

				co_return {};
			}
		;
	}

	TCActorSubscriptionWithID<> f_RegisterCallbacks(TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test)> &&_fCallback)
	{
		auto &Callback = m_Callbacks.f_Insert();
		Callback.m_fCallback = fg_Move(_fCallback);

		return g_ActorSubscription / [this, pDeleted = Callback.m_pDeleted, pCallback = &Callback]() -> TCFuture<void>
			{
				if (*pDeleted)
					co_return {};

				auto ToDestroy = fg_Move(pCallback->m_fCallback);

				m_Callbacks.f_Remove(*pCallback);

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
	}

	CActorSubscription f_RegisterManualCallback(uint32 _iCallback, TCActor<CActor> const &_Actor, TCFunction<TCFuture<CStr> (CStr const &_Test)> &&_Callback)
	{
		*m_Callback[_iCallback] = fg_Move(_Callback);
		m_Actor[_iCallback] = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	CActorSubscription f_RegisterDual
		(
			TCActor<CActor> const &_Actor
			, TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)> &&_Callback0
			, TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)> &&_Callback1
		)
	{
		*m_Callback[0] = fg_Move(_Callback0);
		*m_Callback[1] = fg_Move(_Callback1);
		m_Actor[0] = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	TCFuture<CStr> f_CallDual(CStr const &_Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[0]]
				{
					return (*pCallback)(_Message);
				}
			)
			+ fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[1]]
				{
					return (*pCallback)(_Message);
				}
			)
			> Result / [Result](CStr &&_Result0, CStr &&_Result1)
			{
				Result.f_SetResult(_Result0 + _Result1);
			}
		;
		return Result.f_MoveFuture();
	}

	TCFuture<CActorSubscription> f_RegisterWithError(TCActor<CActor> const &_Actor, TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)> &&_Callback)
	{
		co_return DMibErrorInstance("Register failed");
	}

	void f_RegisterNoSubscription(TCActor<CActor> const &_Actor, TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)> &&_Callback)
	{
		*m_pCallbackNoSub = fg_Move(_Callback);
		m_ActorNoSub = _Actor;
	}

	CActorSubscription f_RegisterNoActor(TCFunctionMovable<TCFuture<CStr> (CStr const &_Test)> &&_Callback)
	{
		*m_pCallbackNoSub = fg_Move(_Callback);
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	CActorSubscription f_RegisterNoFunction(TCActor<CActor> const &_Actor)
	{
		m_ActorNoSub = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	TCFuture<CStr> f_CallNoSubscription(CStr const &_Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_ActorNoSub
				, [_Message, pCallback = m_pCallbackNoSub]
				{
					return (*pCallback)(_Message);
				}
			)
			> Result
		;
		return Result.f_MoveFuture();
	}

	TCFuture<CStr> f_CallCallback(uint32 _iCallback, CStr const &_Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[_iCallback]
				, [_Message, pCallback = m_Callback[_iCallback]]
				{
					return (*pCallback)(_Message);
				}
			)
			> Result
		;
		return Result.f_MoveFuture();
	}

	TCFuture<CStr> f_CallCallbackWithoutDispatch(uint32 _iCallback, CStr const &_Message)
	{
		return (*m_Callback[_iCallback])(_Message);
	}

	TCFuture<CStr> f_CallWrong(CStr const &_Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[1]]
				{
					return (*pCallback)(_Message);
				}
			)
			> Result
		;
		return Result.f_MoveFuture();
	}

	void f_ClearCallback(uint32 _iCallback)
	{
		m_Callback[_iCallback].f_Clear();
		m_Actor[_iCallback].f_Clear();
	}

	void f_CallRegisteredCallback(CStr const &_Message)
	{
		m_fCallback(_Message) > NConcurrency::fg_DiscardResult();
	}

	TCFuture<CStr> f_CallRegisteredCallbackWithReturn(CStr const &_Message)
	{
		co_return co_await m_fCallback(_Message);
	}

	TCFuture<TCVector<TCAsyncResult<CStr>>> f_CallRegisteredCallbacksWithReturn(CStr const &_Message)
	{
		TCActorResultVector<CStr> Results;
		for (auto &Callback : m_Callbacks)
		{
			Callback.m_fCallback(_Message) > Results.f_AddResult();
		}

		co_return co_await Results.f_GetResults();
	}

	uint8 f_CallbacksEmpty()
	{
		return m_fCallback.f_IsEmpty();
	}

	uint32 f_GetResultVirtual() override
	{
		auto Ret = m_Value;
		m_Value = 0;
		return Ret;
	}

	CStr f_GetCallingHostID() const override
	{
		return CActorDistributionManager::fs_GetCallingHostInfo().f_GetRealHostID();
	}

	TCFuture<CStr> f_TestOnDisconnect()
	{
		TCPromise<CStr> Promise;

		auto &HostInfo = CActorDistributionManager::fs_GetCallingHostInfo();
		CStr HostID = HostInfo.f_GetRealHostID();

		if (HostID.f_IsEmpty())
			return Promise <<= CStr();

		HostInfo.f_OnDisconnect
			(
				g_ActorFunctorWeak / [this, HostID]() -> TCFuture<void>
				{
					m_CallingHostTests.f_Remove(HostID);
					co_return {};
				}
			)
			> Promise / [this, Promise, HostID](CActorSubscription &&_Subscription)
			{
				if (!_Subscription)
				{
					Promise.f_SetException(DMibErrorInstance("Subscription failed"));
					return;
				}
				m_CallingHostTests[HostID] = fg_Move(_Subscription);

				Promise.f_SetResult(HostID);
			}
		;

		return Promise.f_MoveFuture();
	}

	uint8 f_HasOnDisconnect(CStr const &_HostID) const
	{
		return m_CallingHostTests.f_FindEqual(_HostID) != nullptr;
	}

/*	template <typename tf_CTest>
	uint32 f_GetResultTemplated()
	{
		auto Ret = m_Value;
		m_Value = 0;
		return Ret;
	}*/ // Not supported in clang

	TCFuture<uint32> f_GetResultDeferred()
	{
		TCPromise<uint32> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Promise](TCAsyncResult<uint32> &&_Result)
			{
				Promise.f_SetResult(fg_Move(_Result));
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntDeferred(uint32 _Value)
	{
		TCPromise<void> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Promise](TCAsyncResult<void> &&_Result)
			{
				Promise.f_SetResult(fg_Move(_Result));
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_GetResultException()
	{
		TCPromise<uint32> Promise;
		Promise.f_SetException(DMibErrorInstance("Test"));
		return Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntException(uint32 _Value)
	{
		TCPromise<void> Promise;
		Promise.f_SetException(DMibErrorInstance("Test"));
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_GetResultDeferredException()
	{
		TCPromise<uint32> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Promise](TCAsyncResult<uint32> &&_Result)
			{
				Promise.f_SetException(DMibErrorInstance("Test"));
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntDeferredException(uint32 _Value)
	{
		TCPromise<void> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Promise](TCAsyncResult<void> &&_Result)
			{
				Promise.f_SetException(DMibErrorInstance("Test"));
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> &&_pValue)
	{
		co_return _pValue->f_ToInt();
	}

	TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> &&_pValue)
	{
		co_return _pValue->f_ToInt();
	}
};

class CDistributedActor_Tests : public NMib::NTest::CTest
{
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_AddInt)>() == 0x9064522d);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetResult)>() == 0x5a3610f5);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetInterface)>() == 0x2939aa90);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetInterfaceSubscriptionCalled)>() == 0x48fc8d0d);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_SetInterface)>() == 0xaa217652);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_ClearSetInterface)>() == 0xe7a490da);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_SetInterfaceSubscriptionCalled)>() == 0x19fa3314);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallSetInterface)>() == 0x00692d1e);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetSubscription)>() == 0x62e31099);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_SetSubscription)>() == 0x3a196912);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterActorFunctors)>() == 0x1ffca9cd);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterActorFunctorsVector)>() == 0x77ed7438);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetClearedActorFunctors)>() == 0x9f18b675);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_ClearActorFunctor)>() == 0x13a2ffa0);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_ResetClearedActorFunctors)>() == 0x07d3d4d8);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallActorFunctor)>() == 0x67821b2c);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetActorFunctorWithoutSubscription)>() == 0x0815784e);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetActorFunctor)>() == 0x96cc4c1e);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetActorFunctorSubscriptionCalled)>() == 0xd1091f2d);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_ClearSubscription)>() == 0x3d5c54ea);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterCallback)>() == 0xf2f1c76d);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterCallbacks)>() == 0xeac03e3f);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterManualCallback)>() == 0x0dcd7e6f);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterDual)>() == 0xdd7ee203);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallDual)>() == 0xa5bb08d3);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterWithError)>() == 0x3df8781b);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterNoSubscription)>() == 0x7a405ace);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterNoActor)>() == 0x66c06129);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_RegisterNoFunction)>() == 0x7784dee7);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallNoSubscription)>() == 0xfcf3ac31);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallCallback)>() == 0x1cc44243);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallCallbackWithoutDispatch)>() == 0xd5d159c4);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallWrong)>() == 0x62b5f5c6);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_ClearCallback)>() == 0xc02f9ac7);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallRegisteredCallback)>() == 0xd33d02bd);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallRegisteredCallbackWithReturn)>() == 0x295e8e75);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallRegisteredCallbacksWithReturn)>() == 0xc2999db1);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_CallbacksEmpty)>() == 0x21a3d1bc);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_TestOnDisconnect)>() == 0xcca95f9d);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_HasOnDisconnect)>() == 0x6ad9c0aa);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetResultDeferred)>() == 0x0e479b8f);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_AddIntDeferred)>() == 0x3cb95a2c);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetResultException)>() == 0x2edcd552);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_AddIntException)>() == 0xc45a4d64);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetResultDeferredException)>() == 0x5ccfd739);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_AddIntDeferredException)>() == 0x18be22b3);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_SharedPointer)>() == 0x6dda3321);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_UniquePointer)>() == 0xf3b29256);

	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&NTest1::CTest::f_TestFunc0)>() == 0x718e3a33);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&NTest2::CTest::f_TestFunc0)>() == 0x3beb0e60);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&NTest1::CTest::f_TestTemplateTypes)>() == 0x1d3ba129);

	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetResultVirtual)>() == 0xbca85324);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_GetCallingHostID)>() == 0x7b394da7);
	static_assert(fg_GetMemberFunctionHash<DMibPointerToMemberFunctionForHash(&CDistributedActor::f_AddIntVirtual)>() == 0x1d4ae31d);

	void fp_RunTests(TCFunction<TCDistributedActor<CDistributedActor> ()> const &_fGetActor, bool _bRemote)
	{
		{
			// Init distribution manager
			uint16 Port = 31413;
			CDistributedActorTestHelperCombined TestState(Port);
			TestState.f_InitServer();
		}
		{
			DMibTestPath("Direct");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActor::f_GetResult)().f_CallSync(g_Timeout);
			DMibExpect(Result, ==, 5);
		}
/*		{
			DMibTestPath("DirectTemplate");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(g_Timeout);

			uint32 Result1 = Actor.f_CallActor(&CDistributedActor::f_GetResultTemplated<NTest1::CTest>)().f_CallSync(g_Timeout);
			DMibExpect(Result1, ==, 5);

			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(g_Timeout);
			uint32 Result2 = Actor.f_CallActor(&CDistributedActor::f_GetResultTemplated<NTest2::CTest>)().f_CallSync(g_Timeout);
			DMibExpect(Result2, ==, 5);
		}*/ // Not supported in clang
		{
			DMibTestPath("Pointers");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();

			TCFuture<uint32> SharedPointerFuture = g_Future <<= Actor.f_CallActor(&CDistributedActor::f_SharedPointer)(fg_Construct(CStr("5")));
			uint32 SharedPointerResult = fg_Move(SharedPointerFuture).f_CallSync(g_Timeout);
			DMibExpect(SharedPointerResult, ==, 5);

			TCFuture<uint32> UniquePointerFuture = g_Future <<= Actor.f_CallActor(&CDistributedActor::f_UniquePointer)(fg_Construct(CStr("5")));
			uint32 UniquePointerResult = fg_Move(UniquePointerFuture).f_CallSync(g_Timeout);
			DMibExpect(UniquePointerResult, ==, 5);
		}
		{
			DMibTestPath("Virtual");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			Actor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("VirtualActor");
			TCDistributedActor<CDistributedActorBase> Actor = _fGetActor();

			Actor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("Deferred");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			Actor.f_CallActor(&CDistributedActor::f_AddIntDeferred)(5).f_CallSync(g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActor::f_GetResultDeferred)().f_CallSync(g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("Exception");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto fTestCall = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_AddIntException)(5).f_CallSync(g_Timeout);
				}
			;
			auto fTestResult = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetResultException)().f_CallSync(g_Timeout);
				}
			;

			DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
			DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
		}
		{
			DMibTestPath("DeferredException");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto fTestCall = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_AddIntDeferredException)(5).f_CallSync(g_Timeout);
				}
			;
			auto fTestResult = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetResultDeferredException)().f_CallSync(g_Timeout);
				}
			;

			DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
			DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
		}
		{
			DMibTestPath("SetSubscription");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto TestActor = fg_ConcurrentActor();
			TCSharedPointer<TCAtomic<mint>> pTestValue = fg_Construct();
			auto &TestValue = *pTestValue;

			auto fSubscription = [&]() -> CActorSubscription
				{
					return g_ActorSubscription(TestActor) / [pTestValue]
						{
							pTestValue->f_Exchange(1);
						}
					;
				}
			;
			auto fSetSubscription = [&](bool _bException)
				{
					Actor.f_CallActor(&CDistributedActor::f_SetSubscription)(fSubscription(), _bException).f_CallSync(g_Timeout);
				}
			;
			auto fSetSubscriptionNonPublished = [&](bool _bException)
				{
					Actor.f_CallActor(&CDistributedActor::f_SetSubscriptionNonPublished)(fSubscription(), _bException).f_CallSync(g_Timeout);
				}
			;
			{
				DMibTestPath("Normal");
				fSetSubscription(false);
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 0);

				Actor.f_CallActor(&CDistributedActor::f_ClearSubscription)().f_CallSync(g_Timeout);
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 1);

				TestValue.f_Exchange(0);
			}
			{
				DMibTestPath("Exception");
				DMibExpectException(fSetSubscription(true), DMibErrorInstance("Error"));
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 1);

				TestValue.f_Exchange(0);
			}
			if (_bRemote)
			{
				DMibTestPath("ExceptionBeforeCall");
				DMibExpectException(fSetSubscriptionNonPublished(true), DMibErrorInstance("Function does not exist on remote actor"));
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 1);

				TestValue.f_Exchange(0);
			}
		}
		{
			DMibTestPath("GetSubscription");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto TestActor = fg_ConcurrentActor();
			auto &TestValue = g_TestValueGetSubscription;
			auto fGetSubscription = [&]()
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetSubscription)().f_CallSync(g_Timeout);
				}
			;
			{
				DMibTestPath("Normal");
				auto Subscription = fGetSubscription();
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 0);

				Subscription->f_Destroy().f_CallSync(g_Timeout);
				Subscription.f_Clear();
				DMibExpect(TestValue.f_Load(), ==, 1);

				TestValue.f_Exchange(0);
			}
		}

		auto fGetRegisterActorFunctors = [&]
			(
				TCDistributedActor<CDistributedActor> const &_Actor
				, TCActor<> const &_TestActor
				, TCSharedPointer<TCAtomic<mint>> const &_pSubscription0Called
				, TCSharedPointer<TCAtomic<mint>> const &_pSubscription1Called
			)
			{
				return _Actor.f_CallActor(&CDistributedActor::f_RegisterActorFunctors)
					(
						g_ActorFunctor(_TestActor)
						(
							g_ActorSubscription(_TestActor) / [_pSubscription0Called]
							{
								_pSubscription0Called->f_Exchange(1);
							}
						)
						/ [](CStr const &_Message) -> TCFuture<CStr>
						{
							if (_Message == "TestMessageException")
								co_return DMibErrorInstance("Test exception 0");
							co_return _Message + "0";
						}
						, g_ActorFunctor(_TestActor)
						(
							g_ActorSubscription(_TestActor) / [_pSubscription1Called]
							{
								_pSubscription1Called->f_Exchange(1);
							}
						)
						/ [](CStr const &_Message) -> TCFuture<CStr>
						{
							if (_Message == "TestMessageException")
								co_return DMibErrorInstance("Test exception 1");
							co_return _Message + "1";
						}
					).f_CallSync(g_Timeout)
				;
			}
		;
		auto fGetRegisterActorFunctorsVector = [&]
			(
				TCDistributedActor<CDistributedActor> const &_Actor
				, TCActor<> const &_TestActor
				, TCSharedPointer<TCAtomic<mint>> const &_pSubscription0Called
				, TCSharedPointer<TCAtomic<mint>> const &_pSubscription1Called
			)
			{
				TCVector<TCActorFunctorWithID<TCFuture<CStr> (CStr const &_Test)>> Functors;
				Functors.f_Insert() = g_ActorFunctor(_TestActor)
					(
						g_ActorSubscription(_TestActor) / [_pSubscription0Called]
						{
							_pSubscription0Called->f_Exchange(1);
						}
					)
					/ [](CStr const &_Message) -> TCFuture<CStr>
					{
						if (_Message == "TestMessageException")
							co_return DMibErrorInstance("Test exception 0");
						co_return _Message + "0";
					}
				;
				Functors.f_Insert() = g_ActorFunctor(_TestActor)
					(
						g_ActorSubscription(_TestActor) / [_pSubscription1Called]
						{
							_pSubscription1Called->f_Exchange(1);
						}
					)
					/ [](CStr const &_Message) -> TCFuture<CStr>
					{
						if (_Message == "TestMessageException")
							co_return DMibErrorInstance("Test exception 1");
						co_return _Message + "1";
					}
				;
				Functors[0].f_SetID(0);
				Functors[1].f_SetID(1);
				return _Actor.f_CallActor(&CDistributedActor::f_RegisterActorFunctorsVector)(fg_Move(Functors)).f_CallSync(g_Timeout);
			}
		;

		auto fTestMultipleActorFunctors = [&](auto _fRegister)
			{
				TCDistributedActor<CDistributedActor> Actor = _fGetActor();
				auto TestActor = fg_ConcurrentActor();

				TCSharedPointer<TCAtomic<mint>> pSubscription0Called = fg_Construct();
				TCSharedPointer<TCAtomic<mint>> pSubscription1Called = fg_Construct();

				auto Subscriptions = _fRegister(Actor, TestActor, pSubscription0Called, pSubscription1Called);

				auto fCallFunctor = [&](mint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_CallActorFunctor)(_iFunctor, "Test").f_CallSync(g_Timeout);
					}
				;
				auto fGetCleared = [&](mint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_GetClearedActorFunctors)(_iFunctor).f_CallSync(g_Timeout);
					}
				;
				auto fClearActorFunctor = [&](mint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_ClearActorFunctor)(_iFunctor).f_CallSync(g_Timeout);
					}
				;

				DMibExpect(fCallFunctor(0), ==, "Test0");
				DMibExpect(fCallFunctor(1), ==, "Test1");

				{
					DMibTestPath("Clear0");
					Subscriptions.f_Subscription(0).f_Clear();
					if (_bRemote)
						DMibExpectException(fCallFunctor(0), DMibErrorInstance("Subscription has been removed"));
					DMibExpect(fCallFunctor(1), ==, "Test1");

					DMibExpectTrue(fGetCleared(0));
					DMibExpectFalse(fGetCleared(1));
				}
				{
					DMibTestPath("Clear1");
					Subscriptions.f_Subscription(1).f_Clear();
					if (_bRemote)
					{
						DMibExpectException(fCallFunctor(0), DMibErrorInstance("Subscription has been removed"));
						DMibExpectException(fCallFunctor(1), DMibErrorInstance("Subscription has been removed"));
					}

					DMibExpectTrue(fGetCleared(0));
					DMibExpectTrue(fGetCleared(1));
				}
				{
					DMibTestPath("ClearFunctor0");

					DMibExpectFalse(pSubscription0Called->f_Load());
					DMibExpectFalse(pSubscription1Called->f_Load());

					fClearActorFunctor(0);
					fg_Dispatch(TestActor, []{}).f_CallSync(g_Timeout);

					DMibExpectTrue(pSubscription0Called->f_Load());
				}
				{
					DMibTestPath("ClearFunctor1");

					DMibExpectTrue(pSubscription0Called->f_Load());
					DMibExpectFalse(pSubscription1Called->f_Load());

					fClearActorFunctor(1);
					fg_Dispatch(TestActor, []{}).f_CallSync(g_Timeout);

					DMibExpectTrue(pSubscription1Called->f_Load());
				}

				Actor.f_CallActor(&CDistributedActor::f_ResetClearedActorFunctors)().f_CallSync(g_Timeout);
			}
		;
		{
			DMibTestPath("MultipleActorFunctors");
			fTestMultipleActorFunctors(fGetRegisterActorFunctors);
		}
		{
			DMibTestPath("MultipleActorFunctorsVector");
			fTestMultipleActorFunctors(fGetRegisterActorFunctorsVector);
		}
		{
			DMibTestPath("GetActorFunctor");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto TestActor = fg_ConcurrentActor();
			TCSharedPointer<TCAtomic<mint>> pTestValue = fg_Construct();
			[[maybe_unused]] auto &TestValue = *pTestValue;

			[[maybe_unused]] auto fSubscription = [&]() -> CActorSubscription
				{
					return g_ActorSubscription(TestActor) / [pTestValue]
						{
							pTestValue->f_Exchange(1);
						}
					;
				}
			;
			auto fGetActorSubscriptionWithoutSubscription = [&]()
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetActorFunctorWithoutSubscription)().f_CallSync(g_Timeout);
				}
			;
			auto fGetActorSubscription = [&]()
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetActorFunctor)().f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
			{
				DMibTestPath("WithoutSubscription");
				DMibExpectException
					(
						fGetActorSubscriptionWithoutSubscription()
						, DMibErrorInstance("Invalid set of parameter and return types: No matching subscription for actor functors or interfaces")
					)
				;
			}
			{
				DMibTestPath("WithSubscription");
				auto ActorFunctor = fGetActorSubscription();
				CStr ReturnValue = ActorFunctor("Test").f_CallSync(g_Timeout);
				DMibExpect(ReturnValue, ==, "TestWithSubscription");

				ActorFunctor.f_GetSubscription().f_Clear();
				bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_GetActorFunctorSubscriptionCalled)().f_CallSync(g_Timeout);
				DMibExpectTrue(bSubscriptionCalled);

				if (_bRemote)
					DMibExpectException(ActorFunctor("Test").f_CallSync(g_Timeout), DMibErrorInstance("Subscription has been removed"));

				ActorFunctor.f_Clear();
			}
		}
		{
			DMibTestPath("Return Async Generator");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(TestActor);

			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto Generator = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGenerator)().f_CallSync(g_Timeout);

			auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);
			DMibExpect(*iIterator, ==, 5);
			(++iIterator).f_CallSync(g_Timeout);
			DMibExpect(*iIterator, ==, 6);
			(++iIterator).f_CallSync(g_Timeout);
			DMibExpect(*iIterator, ==, 7);
			(++iIterator).f_CallSync(g_Timeout);
			DMibExpect(*iIterator, ==, 8);
			(++iIterator).f_CallSync(g_Timeout);
			DMibExpectFalse(!!iIterator);
		}
		{
			DMibTestPath("Return Async Generator Functor");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(TestActor);

			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto Functor = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctor)().f_CallSync(g_Timeout);

			for (mint i = 0; i < 2; ++i)
			{
				DMibTestPath("Iteration {}"_f << i);
				auto Generator = Functor(i).f_CallSync();

				auto iIterator = fg_Move(Generator).f_GetIterator().f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 5);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 6);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 7);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, 8);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpect(*iIterator, ==, i);
				(++iIterator).f_CallSync(g_Timeout);
				DMibExpectFalse(!!iIterator);
				fg_Move(Generator).f_Destroy().f_CallSync(g_Timeout);
			}
			fg_Move(Functor).f_Destroy().f_CallSync(g_Timeout);
		}
		{
			DMibTestPath("Send Async Generator");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(TestActor);

			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto Value = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorSend)
				(
					(
						g_Dispatch / []() -> TCAsyncGenerator<int32>
						{
							co_yield 1;
							co_yield 2;
							co_yield 3;
							co_yield 4;
							co_return {};
						}
					)
					.f_CallSync(g_Timeout)
				).f_CallSync(g_Timeout)
			;

			DMibExpect(Value, ==, 10);
		}
		{
			DMibTestPath("Send Async Generator Functor");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(TestActor);

			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto Value = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctorSend)
				(
					g_ActorFunctor(g_ActorSubscription / [] {}) / [](int32 const &_Value) -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						co_yield 2;
						co_yield 3;
						co_yield 4;
						co_yield _Value;
						co_return {};
					}
				).f_CallSync(g_Timeout)
			;

			DMibExpect(Value, ==, 32);
		}
		{
			DMibTestPath("GetInterface");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto Interface = Actor.f_CallActor(&CDistributedActor::f_GetInterface)().f_CallSync(g_Timeout);

			auto fCallInterface = [&]
				{
					return Interface.f_CallActor(&CDistributedActorInterface::f_Test)().f_CallSync(g_Timeout);
				}
			;

			DMibExpect(fCallInterface(), ==, 5);

			Interface.f_GetSubscription().f_Clear();
			bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_GetInterfaceSubscriptionCalled)().f_CallSync(g_Timeout);
			DMibExpectTrue(bSubscriptionCalled);

			if (_bRemote)
				DMibExpectException(fCallInterface(), DMibErrorInstance("Remote actor no longer exists"));

			Interface.f_Clear();
		}
		{
			DMibTestPath("SetInterface");

			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto TestActor = fg_ConcurrentActor();

			auto InterfaceActor = fg_ConstructDistributedActor<CDistributedActorInterface>(fg_GetDistributionManager());

			TCSharedPointer<TCAtomic<mint>> pSubscriptionCalled = fg_Construct();
			TCDistributedActorInterfaceWithID<CDistributedActorInterface> Interface
				{
					InterfaceActor->f_ShareInterface<CDistributedActorInterface>()
					, g_ActorSubscription(TestActor) / [pSubscriptionCalled]
					{
						pSubscriptionCalled->f_Exchange(1);
					}
				}
			;

			auto Subscription = Actor.f_CallActor(&CDistributedActor::f_SetInterface)(fg_Move(Interface)).f_CallSync(g_Timeout);

			auto fCallInterface = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_CallSetInterface)().f_CallSync(g_Timeout);
				}
			;

			DMibExpect(fCallInterface(), ==, 5);

			Subscription.f_Clear();
			bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_SetInterfaceSubscriptionCalled)().f_CallSync(g_Timeout);
			DMibExpectTrue(bSubscriptionCalled);

			DMibExpectFalse(pSubscriptionCalled->f_Load());

			if (_bRemote)
				DMibExpectException(fCallInterface(), DMibErrorInstance("Remote actor no longer exists"));

			Actor.f_CallActor(&CDistributedActor::f_ClearSetInterface)().f_CallSync(g_Timeout);
			fg_Dispatch(TestActor, []{}).f_CallSync(g_Timeout);

			DMibExpectTrue(pSubscriptionCalled->f_Load());

			Interface.f_Clear();
		}
		{
			DMibTestPath("Callbacks");
			TCDistributedActor<CDistributedActor> Actor = _fGetActor();
			auto TestActor = fg_ConcurrentActor();

			struct CState
			{
				CMutual m_Lock;
				CEventAutoReset m_Event;
				CStr m_Message;
			};

			TCSharedPointer<CState> pState = fg_Construct();

			auto fCallBack = [pState](CStr const &_Message) -> TCFuture<CStr>
				{
					TCPromise<CStr> Promise;
					DMibLock(pState->m_Lock);
					pState->m_Message = _Message;
					pState->m_Event.f_Signal();

					return Promise <<= _Message;
				}
			;

			auto fWaitForMessage = [pState]() -> NStr::CStr
				{
					CStr ReceivedMessage;
					bool bTimedOut = false;
					while (!bTimedOut)
					{
						{
							DMibLock(pState->m_Lock);
							if (!pState->m_Message.f_IsEmpty())
							{
								ReceivedMessage = fg_Move(pState->m_Message);
								break;
							}
						}
						bTimedOut = pState->m_Event.f_WaitTimeout(10.0);
					}

					return ReceivedMessage;
				}
			;

			auto Subscription = Actor.f_CallActor(&CDistributedActor::f_RegisterCallback)(g_ActorFunctor(TestActor) / fCallBack).f_CallSync(g_Timeout);

			bool bCallbacksEmpty = Actor.f_CallActor(&CDistributedActor::f_CallbacksEmpty)().f_CallSync(g_Timeout);

			DMibExpectFalse(bCallbacksEmpty);

			Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallback)("TestMessage").f_CallSync(g_Timeout);

			DMibExpect(fWaitForMessage(), ==, "TestMessage");

			CStr CallbackWithReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbackWithReturn)("TestMessage").f_CallSync(g_Timeout);

			DMibExpect(CallbackWithReturn, ==, "TestMessage");

			Subscription.f_Clear();

			bCallbacksEmpty = Actor.f_CallActor(&CDistributedActor::f_CallbacksEmpty)().f_CallSync(g_Timeout);
			DMibExpectTrue(bCallbacksEmpty);

			auto fCallback0 = [AllowDestroy = g_AllowWrongThreadDestroy](CStr const &_Message) -> TCFuture<CStr>
				{
					TCPromise<CStr> Promise;
					if (_Message == "TestMessageException")
						return Promise <<= DMibErrorInstance("Test exception 0");
					return Promise <<= _Message + "0";
				}
			;

			auto fCallback1 = [AllowDestroy = g_AllowWrongThreadDestroy](CStr const &_Message) -> TCFuture<CStr>
				{
					TCPromise<CStr> Promise;
					if (_Message == "TestMessageException")
						return Promise <<= DMibErrorInstance("Test exception 1");
					return Promise <<= _Message + "1";
				}
			;

			TCActor<> TestActor0 = fg_ConstructActor<CActor>();
			TCActor<> TestActor1 = fg_ConstructActor<CActor>();

			{
				auto MultipleSubscription0 = Actor.f_CallActor(&CDistributedActor::f_RegisterCallbacks)(g_ActorFunctor(TestActor0) / fCallback0).f_CallSync(g_Timeout);
				auto MultipleSubscription1 = Actor.f_CallActor(&CDistributedActor::f_RegisterCallbacks)(g_ActorFunctor(TestActor1) / fCallback1).f_CallSync(g_Timeout);

				TCVector<TCAsyncResult<CStr>> MultipleReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbacksWithReturn)("TestMessage").f_CallSync(g_Timeout);
				DMibExpect(*MultipleReturn[0], ==, "TestMessage0");
				DMibExpect(*MultipleReturn[1], ==, "TestMessage1");

				MultipleReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbacksWithReturn)("TestMessageException").f_CallSync(g_Timeout);
				DMibExpectException(*MultipleReturn[0], DMibErrorInstance("Test exception 0"));
				DMibExpectException(*MultipleReturn[1], DMibErrorInstance("Test exception 1"));
			}


			auto fRegisterWithError = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterWithError)(TestActor0, fCallback0).f_CallSync(g_Timeout);
				}
			;
			DMibExpectException(fRegisterWithError(), DMibErrorInstance("Register failed"));

			auto fRegisterWithoutSubscription = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoSubscription)(TestActor0, fCallback0).f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterWithoutSubscription(), DMibErrorInstance("Invalid set of parameter and return types"));

			auto fCallNoSubscription = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallNoSubscription)("TestMessage").f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallNoSubscription(), DMibErrorInstance("Subscription has been removed"));

			auto fRegisterNoActor = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoActor)(fCallback0).f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterNoActor(), DMibErrorInstance("You must have one and only one actor in the function arguments if you include a functor"));

			auto fRegisterNoFunction = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoFunction)(TestActor0).f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterNoFunction(), DMibErrorInstance("You must have at least one function in arguments if you include an actor"));

			auto Subscription0 = Actor.f_CallActor(&CDistributedActor::f_RegisterManualCallback)(0, TestActor0, fCallback0).f_CallSync(g_Timeout);
			auto Subscription1 = Actor.f_CallActor(&CDistributedActor::f_RegisterManualCallback)(1, TestActor1, fCallback1).f_CallSync(g_Timeout);

			CStr ReturnMessage0 = Actor.f_CallActor(&CDistributedActor::f_CallCallback)(0, "TestMessage").f_CallSync(g_Timeout);
			DMibExpect(ReturnMessage0, ==, "TestMessage0");

			CStr ReturnMessage1 = Actor.f_CallActor(&CDistributedActor::f_CallCallback)(1, "TestMessage").f_CallSync(g_Timeout);
			DMibExpect(ReturnMessage1, ==, "TestMessage1");

			auto fCallWrong = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallWrong)("TestMessage").f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallWrong(), DMibErrorInstance("Trying to call function on wrong dispatch actor"));

			auto fCallWithoutDispatch = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallbackWithoutDispatch)(0, "TestMessage").f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallWithoutDispatch(), DMibErrorInstance("This functions needs to be dispatched on a remote actor"));

			Subscription0.f_Clear();

			auto fCallRemoved = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallback)(0, "TestMessage").f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallRemoved(), DMibErrorInstance("Subscription has been removed"));

			TestActor1->f_BlockDestroy();

			auto fCallDestroyed = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallback)(1, "TestMessage").f_CallSync(g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallDestroyed(), DMibErrorInstance("Actor 'NMib::NConcurrency::CActor' called has been deleted"));

			TestActor1.f_Clear();

			if (_bRemote)
				DMibExpectException(fCallDestroyed(), DMibErrorInstance("Remote dispatch actor no longer exists"));

			Subscription1.f_Clear();

			auto SubscriptionDual = Actor.f_CallActor(&CDistributedActor::f_RegisterDual)(TestActor0, fCallback0, fCallback1).f_CallSync(g_Timeout);

			CStr ReturnMessageDual = Actor.f_CallActor(&CDistributedActor::f_CallDual)("TestMessage").f_CallSync(g_Timeout);
			DMibExpect(ReturnMessageDual, ==, "TestMessage0TestMessage1");
		};
	}

	void fp_BasicTests(NStr::CStr const &_Address)
	{
		CStr ServerHostID = NCryptography::fg_RandomID();
		CStr ClientHostID = NCryptography::fg_RandomID();
		TCActor<CActorDistributionManager> ServerManager = fg_ConstructActor<CActorDistributionManager>(CActorDistributionManagerInitSettings{ServerHostID, {}});
		TCActor<CActorDistributionManager> ClientManager = fg_ConstructActor<CActorDistributionManager>(CActorDistributionManagerInitSettings{ClientHostID, {}});

		CActorDistributionCryptographySettings ServerCryptography{ServerHostID};
		ServerCryptography.f_GenerateNewCert(fg_CreateVector<CStr>(_Address), CDistributedActorTestKeySettings{});

		NWeb::NHTTP::CURL ConnectAddress;

		if (_Address == "localhost")
		{
			NNetwork::CNetAddressTCPv4 Address;
			Address.m_Port = 31393;
			ConnectAddress = fg_Format("wss://{}:31393/", _Address);
		}
		else
		{
			ConnectAddress = fg_Format("wss://[{}]/", _Address);
		}

		CActorDistributionListenSettings ListenSettings{fg_CreateVector(ConnectAddress)};
		ListenSettings.f_SetCryptography(ServerCryptography);
		ListenSettings.m_bRetryOnListenFailure = false;
		ListenSettings.m_ListenFlags = NNetwork::ENetFlag_None;
		CDistributedActorListenReference ListenReference = ServerManager(&CActorDistributionManager::f_Listen, ListenSettings).f_CallSync(g_Timeout);

		TCDistributedActor<CDistributedActor> PublishedActor = ServerManager->f_ConstructActor<CDistributedActor>();

		auto ActorPublication = PublishedActor->f_Publish<CDistributedActorBase>("Test").f_CallSync(g_Timeout);

		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = ConnectAddress;
		ConnectionSettings.m_PublicServerCertificate = ListenSettings.m_CACertificate;
		CActorDistributionCryptographySettings ClientCryptography{ClientHostID};
		ClientCryptography.f_GenerateNewCert(fg_CreateVector<CStr>(_Address), CDistributedActorTestKeySettings{});
		auto CertificateRequest = ClientCryptography.f_GenerateRequest();

		auto SignedRequest = ServerCryptography.f_SignRequest(CertificateRequest);

		ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, ServerCryptography.m_PublicCertificate, SignedRequest);

		ConnectionSettings.f_SetCryptography(ClientCryptography);
		ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		ConnectionSettings.m_bRetryConnectOnFailure = false;

		NThread::CMutual RemoteLock;
		NThread::CEventAutoReset RemoteEvent;
		TCDistributedActor<CDistributedActorBase> RemoteActor;
		CActorSubscription RemoteActorsSubscription;
		mint RemoteEvents = 0;
		bool bRemoved = false;

		auto &ConcurrentActor = fg_ConcurrentActor();

		ClientManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, fg_CreateVector<CStr>("Test")
				, ConcurrentActor
				, [&](CAbstractDistributedActor &&_NewActor)
				{
					DMibLock(RemoteLock);
					RemoteActor = _NewActor.f_GetActor<CDistributedActorBase>();
					RemoteEvent.f_Signal();
					++RemoteEvents;
				}
				, [&](CDistributedActorIdentifier const &_RemovedActor)
				{
					DMibLock(RemoteLock);
					if (_RemovedActor == RemoteActor)
					{
						RemoteActor.f_Clear();
						bRemoved = true;
					}
					RemoteEvent.f_Signal();
					++RemoteEvents;
				}
			)
			> ConcurrentActor / [&](TCAsyncResult<CActorSubscription> &&_Result)
			{
				DMibLock(RemoteLock);
				RemoteActorsSubscription = fg_Move(*_Result);
				RemoteEvent.f_Signal();
				++RemoteEvents;
			}
		;

		CDistributedActorConnectionReference ClientConnectionReference
			= ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(g_Timeout).m_ConnectionReference
		;

		bool bConnectionSuccessful = true; // If not we would have had exceptions above

		DMibExpectTrue(bConnectionSuccessful);

		bool bTimedOutWatingForActor = false;
		while (!bTimedOutWatingForActor)
		{
			{
				DMibLock(RemoteLock);
				if (RemoteEvents >= 2)
					break;
			}
			bTimedOutWatingForActor = RemoteEvent.f_WaitTimeout(60.0);
		}

		{
			DMibLock(RemoteLock);

			DMibAssertFalse(bTimedOutWatingForActor);
			DMibAssert(RemoteEvents, ==, 2);
			DMibAssertTrue(RemoteActor);
			DMibAssertTrue(RemoteActorsSubscription);
		}

		RemoteActor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(g_Timeout);
		uint32 Result = RemoteActor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(g_Timeout);
		DMibExpect(Result, ==, 5);

		CStr RemoteCallingHostID = (RemoteActor.f_CallActor(&CDistributedActorBase::f_GetCallingHostID)()).f_CallSync(g_Timeout);
		DMibExpect(RemoteCallingHostID, ==, ClientHostID);

		ActorPublication.f_Destroy().f_CallSync();

		bool bTimedOutWatingForUnPublish = false;
		while (!bTimedOutWatingForUnPublish)
		{
			{
				DMibLock(RemoteLock);
				if (RemoteEvents >= 3)
					break;
			}
			bTimedOutWatingForUnPublish = RemoteEvent.f_WaitTimeout(60.0);
		}

		DMibAssertFalse(bTimedOutWatingForUnPublish);
		DMibExpectTrue(bRemoved);

		ClientConnectionReference.f_Disconnect().f_CallSync(g_Timeout);
		DMibExpectExceptionType(ClientConnectionReference.f_Disconnect().f_CallSync(g_Timeout), NException::CException);

		ListenReference.f_Stop().f_CallSync(g_Timeout);
		DMibExpectExceptionType(ListenReference.f_Stop().f_CallSync(g_Timeout), NException::CException);
	}
	void fp_AnonymousClientTests()
	{
		uint16 Port = 31402;

		CDistributedActorTestHelperCombined TestHelper{Port};

		TestHelper.f_SeparateServerManager();
		TestHelper.f_InitServer();
		CDistributedActorSecurity Security;
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Anonymous/Test");
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Anonymous/Test2");
		Security.m_AllowedIncomingConnectionNamespaces.f_Insert("Test");
		TestHelper.f_GetServer().f_SetSecurity(Security);
		TestHelper.f_InitAnonymousClient(TestHelper);
		TestHelper.f_Publish<CDistributedActor, CDistributedActorBase>(TestHelper.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>(), "Anonymous/Test");
		TestHelper.f_Publish<CDistributedActor, CDistributedActorBase>(TestHelper.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>(), "Test");
		CStr Subscription = TestHelper.f_Subscribe("Anonymous/Test");
		auto Actor = TestHelper.f_GetRemoteActor<CDistributedActor>(Subscription);
		if (!Actor)
			DMibError("Failed to distributed actor environment");

		auto HostID = Actor.f_CallActor(&CDistributedActorBase::f_GetCallingHostID)().f_CallSync();
		DMibExpectTrue(HostID.f_StartsWith("Anonymous_"));

		{
			DMibTestPath("Anonymous cannot access normal publications");
			TestHelper.f_GetClient().f_SubscribeExpectFailure("Test");
		}

		TestHelper.f_GetClient().f_Publish<CDistributedActor, CDistributedActorBase>(TestHelper.f_GetClient().f_GetManager()->f_ConstructActor<CDistributedActor>(), "Anonymous/Test2");
		{
			DMibTestPath("Anonymous cannot publish");
			TestHelper.f_GetServer().f_SubscribeExpectFailure("Anonymous/Test2");
		}
	}

	void fp_OnDisconnectedTests()
	{
		uint16 Port = 31404;
		CDistributedActorTestHelperCombined TestState(Port);
		TestState.f_SeparateServerManager();
		TestState.f_Init();
		auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
		TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
		CStr SubscriptionID = TestState.f_Subscribe("Test");
		auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

		CStr HostID = Actor.f_CallActor(&CDistributedActor::f_TestOnDisconnect)().f_CallSync(g_Timeout);

		DMibExpect(HostID, != , "");
		bool bHasClient = Actor.f_CallActor(&CDistributedActor::f_HasOnDisconnect)(HostID).f_CallSync(g_Timeout);
		DMibExpectTrue(bHasClient);
		TestState.f_DisconnectClient(true);
		TestState.f_InitClient(TestState);
		SubscriptionID = TestState.f_Subscribe("Test");
		Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);
		bHasClient = LocalActor(&CDistributedActor::f_HasOnDisconnect, HostID).f_CallSync(g_Timeout);
		DMibExpectFalse(bHasClient);
	}

public:
	void f_FunctionalTests()
	{
		DMibTestSuite("Local")
		{
			struct CState
			{
				CDistributedActorTestHelperCombined m_TestState{31410};
			};

			TCSharedPointer<CState> pState;

			fp_RunTests
				(
					[&]
					{
						if (!pState)
						{
							pState = fg_Construct();
							pState->m_TestState.f_Init();
						}
						return pState->m_TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
					}
					, false
				)
			;
		};
		DMibTestCategory("Remote")
		{
			DMibTestSuite("Basics")
			{
				fp_BasicTests("localhost");
			};
			DMibTestSuite("Basics Unix Sockets")
			{
				fp_BasicTests("UNIX:" + NNetwork::fg_GetSafeUnixSocketPath("{}/TestDistributedActor.socket"_f << NMib::NFile::CFile::fs_GetProgramDirectory()));
			};
			DMibTestSuite("Anonymous client")
			{
				fp_AnonymousClientTests();
			};

			DMibTestSuite("On disconnected")
			{
				fp_OnDisconnectedTests();
			};

			TCSharedPointer<CDistributedActorTestHelperCombined> pTestState;
			CStr SubscriptionID;

			DMibTestSuite("Remote")
			{
				fp_RunTests
					(
						[&]
						{
							if (!pTestState)
							{
								uint16 Port = 31403;

								pTestState = fg_Construct(Port);

								pTestState->f_SeparateServerManager();
								pTestState->f_Init();
								pTestState->f_Publish<CDistributedActor, CDistributedActorBase>
									(
										pTestState->f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>()
										, "Test"
									)
								;
								SubscriptionID = pTestState->f_Subscribe("Test");
							}

							auto Actor = pTestState->f_GetRemoteActor<CDistributedActor>(SubscriptionID);

							if (!Actor)
								DMibError("Failed to distributed actor environment");

							return Actor;
						}
						, false
					)
				;
			};
		};
	}

	void f_DoTests()
	{
		f_FunctionalTests();
	}
};

DMibTestRegister(CDistributedActor_Tests, Malterlib::Concurrency);
