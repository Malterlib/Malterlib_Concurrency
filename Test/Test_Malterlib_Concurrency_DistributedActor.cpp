// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
using namespace NMib::NNetwork;
using namespace NMib::NTime;

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

template <typename t_CMemberFunctionPointer>
consteval bool fg_Test()
{
	t_CMemberFunctionPointer x = nullptr;

	return x == x;
}

struct CTestiii
{
	void f_Test2()
	{
	}

	void f_Test3()
	{
	}

	virtual void f_Test4()
	{
	}

	virtual void f_Test5()
	{
	}
};

struct CTestiii2 : public CTestiii
{
	void f_Test4() override
	{
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

	TCFuture<> f_SetSubscriptionNonPublished(CActorSubscription _Subscription, bool _bError)
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

constinit TCAtomic<umint> g_TestValueGetSubscription{0};

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

		TCActorFunctor<TCFuture<CStr> (CStr _Test)> m_fCallback;
		TCSharedPointer<bool> m_pDeleted = fg_Construct(false);
	};

	uint32 m_Value = 0;
	TCMap<CStr, CActorSubscription> m_CallingHostTests;
	TCActorFunctor<TCFuture<CStr> (CStr _Test)> m_fCallback;
	TCLinkedList<CCallback> m_Callbacks;

	TCSharedPointer<TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)>> m_Callback[2];
	TCActor<CActor> m_Actor[2];

	TCSharedPointer<TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)>> m_pCallbackNoSub = fg_Construct();
	TCActor<CActor> m_ActorNoSub;

	TCDistributedActor<CDistributedActorInterface> m_Interface;

	CActorSubscription m_Subscription;

	struct CActorFunctor
	{
		TCActorFunctor<TCFuture<CStr> (CStr _Test)> m_ActorFunctor;
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
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorSendPipelined);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorFunctor);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorFunctorSend);
		DMibPublishActorFunction(CDistributedActor::f_TestAsyncGeneratorFunctorSendPipelined);

		DMibPublishActorFunction(CDistributedActor::f_TestPacketSizeLimit);
		DMibPublishActorFunction(CDistributedActor::f_TestGeneral);
		DMibPublishActorFunction(CDistributedActor::f_TestReturnPacketSize);
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

	TCActorFunctorWithID<TCAsyncGenerator<int32> (int32 _Value), gc_SubscriptionNotRequired> f_TestAsyncGeneratorFunctor()
	{
		return g_ActorFunctor
			(
				g_ActorSubscription / []
				{
				}
			)
			/
			[
				this
				, Exit = g_OnScopeExit / []
				{
				}
			] (int32 _Value) -> TCAsyncGenerator<int32>
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

	TCFuture<int32> f_TestAsyncGeneratorSend(TCAsyncGenerator<int32> _Generator)
	{
		int32 Return = 0;
		for (auto iValue = co_await fg_Move(_Generator).f_GetSimpleIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		co_return Return;
	}

	TCFuture<int32> f_TestAsyncGeneratorFunctorSend(TCActorFunctorWithID<TCAsyncGenerator<int32> (int32 _Value), gc_SubscriptionNotRequired> _fGenerator)
	{
		int32 Return = 0;
		for (auto iValue = co_await (co_await _fGenerator(5)).f_GetSimpleIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		for (auto iValue = co_await (co_await _fGenerator(7)).f_GetSimpleIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		co_return Return;
	}

	TCFuture<int32> f_TestAsyncGeneratorSendPipelined(TCAsyncGenerator<int32> _Generator)
	{
		int32 Return = 0;
		for (auto iValue = co_await fg_Move(_Generator).f_GetPipelinedIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		co_return Return;
	}

	TCFuture<int32> f_TestAsyncGeneratorFunctorSendPipelined(TCActorFunctorWithID<TCAsyncGenerator<int32> (int32 _Value), gc_SubscriptionNotRequired> _fGenerator)
	{
		int32 Return = 0;
		for (auto iValue = co_await (co_await _fGenerator(5)).f_GetPipelinedIterator(); iValue; co_await ++iValue)
			Return += *iValue;
		for (auto iValue = co_await (co_await _fGenerator(7)).f_GetPipelinedIterator(); iValue; co_await ++iValue)
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

	TCFuture<> f_SetSubscription(CActorSubscription _Subscription, bool _bError)
	{
		if (_bError)
			co_return DMibErrorInstance("Error");

		m_Subscription = fg_Move(_Subscription);
		co_return {};
	}

	CMultipleSubscriptions f_RegisterActorFunctors
		(
			TCActorFunctorWithID<TCFuture<CStr> (CStr _Test), 0> &&_Callback0
			, TCActorFunctorWithID<TCFuture<CStr> (CStr _Test), 1> &&_Callback1
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
			TCVector<TCActorFunctorWithID<TCFuture<CStr> (CStr _Test), 0>> &&_Callbacks
		)
	{
		CMultipleSubscriptionsVector Subscriptions;

		m_ActorFunctors.f_Clear();
		umint iFunctor = 0;
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

	TCFuture<void> f_ClearActorFunctor(uint32 _iCallback)
	{
		auto ActorFunctor = fg_Move(m_ActorFunctors[_iCallback].m_ActorFunctor);
		co_await fg_Move(ActorFunctor).f_Destroy();

		co_return {};
	}

	void f_ResetClearedActorFunctors()
	{
		m_ActorFunctors.f_Clear();
	}

	TCFuture<CStr> f_CallActorFunctor(uint32 _iCallback, CStr _Message)
	{
		co_return co_await m_ActorFunctors[_iCallback].m_ActorFunctor(_Message);
	}

	TCActorFunctorWithID<TCFuture<CStr> (CStr _Test)> f_GetActorFunctorWithoutSubscription()
	{
		return g_ActorFunctor / [] (CStr _Test) -> TCFuture<CStr>
			{
				co_return _Test + "WithoutSubscription";
			}
		;
	}

	TCActorFunctorWithID<TCFuture<CStr> (CStr _Test)> f_GetActorFunctor()
	{
		return g_ActorFunctor
			(
				g_ActorSubscription / [this]
				{
					m_bReturnedFunctionActorSubscriptionCalled = true;
				}
			)
			/ [] (CStr _Test) -> TCFuture<CStr>
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

	TCFuture<void> f_ClearSubscription()
	{
		if (m_Subscription)
			co_await fg_Exchange(m_Subscription, nullptr)->f_Destroy();

		co_return {};
	}

	TCActorSubscriptionWithID<> f_RegisterCallback(TCActorFunctorWithID<TCFuture<CStr> (CStr _Test)> &&_fCallback)
	{
		m_fCallback = fg_Move(_fCallback);

		return g_ActorSubscription / [this]() -> TCFuture<void>
			{
				co_await fg_Move(m_fCallback).f_Destroy();

				co_return {};
			}
		;
	}

	TCActorSubscriptionWithID<> f_RegisterCallbacks(TCActorFunctorWithID<TCFuture<CStr> (CStr _Test)> &&_fCallback)
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

	CActorSubscription f_RegisterManualCallback(uint32 _iCallback, TCActor<CActor> const &_Actor, TCFunction<TCFuture<CStr> (CStr &&_Test)> &&_Callback)
	{
		*m_Callback[_iCallback] = fg_Move(_Callback);
		m_Actor[_iCallback] = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	CActorSubscription f_RegisterDual
		(
			TCActor<CActor> const &_Actor
			, TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)> &&_Callback0
			, TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)> &&_Callback1
		)
	{
		*m_Callback[0] = fg_Move(_Callback0);
		*m_Callback[1] = fg_Move(_Callback1);
		m_Actor[0] = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	TCFuture<CStr> f_CallDual(CStr _Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[0]] mark_no_coroutine_debug
				{
					return (*pCallback)(fg_TempCopy(_Message));
				}
			)
			+ fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[1]] mark_no_coroutine_debug
				{
					return (*pCallback)(fg_TempCopy(_Message));
				}
			)
			> Result / [Result](CStr &&_Result0, CStr &&_Result1)
			{
				Result.f_SetResult(_Result0 + _Result1);
			}
		;
		co_return co_await Result.f_MoveFuture();
	}

	TCFuture<CActorSubscription> f_RegisterWithError(TCActor<CActor> _Actor, TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)> _Callback)
	{
		co_return DMibErrorInstance("Register failed");
	}

	void f_RegisterNoSubscription(TCActor<CActor> const &_Actor, TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)> &&_Callback)
	{
		*m_pCallbackNoSub = fg_Move(_Callback);
		m_ActorNoSub = _Actor;
	}

	CActorSubscription f_RegisterNoActor(TCFunctionMovable<TCFuture<CStr> (CStr &&_Test)> &&_Callback)
	{
		*m_pCallbackNoSub = fg_Move(_Callback);
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	CActorSubscription f_RegisterNoFunction(TCActor<CActor> const &_Actor)
	{
		m_ActorNoSub = _Actor;
		return fg_Construct<CActorSubscriptionReferenceDummy>();
	}

	TCFuture<CStr> f_CallNoSubscription(CStr _Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_ActorNoSub
				, [_Message, pCallback = m_pCallbackNoSub] mark_no_coroutine_debug
				{
					return (*pCallback)(fg_TempCopy(_Message));
				}
			)
			> fg_TempCopy(Result)
		;
		co_return co_await Result.f_MoveFuture();
	}

	TCFuture<CStr> f_CallCallback(uint32 _iCallback, CStr _Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[_iCallback]
				, [_Message, pCallback = m_Callback[_iCallback]] mark_no_coroutine_debug
				{
					return (*pCallback)(fg_TempCopy(_Message));
				}
			)
			> fg_TempCopy(Result)
		;
		co_return co_await Result.f_MoveFuture();
	}

	TCFuture<CStr> f_CallCallbackWithoutDispatch(uint32 _iCallback, CStr _Message)
	{
		return (*m_Callback[_iCallback])(fg_TempCopy(_Message));
	}

	TCFuture<CStr> f_CallWrong(CStr _Message)
	{
		TCPromise<CStr> Result;
		fg_Dispatch
			(
				m_Actor[0]
				, [_Message, pCallback = m_Callback[1]] mark_no_coroutine_debug
				{
					return (*pCallback)(fg_TempCopy(_Message));
				}
			)
			> fg_TempCopy(Result)
		;
		co_return co_await Result.f_MoveFuture();
	}

	void f_ClearCallback(uint32 _iCallback)
	{
		m_Callback[_iCallback].f_Clear();
		m_Actor[_iCallback].f_Clear();
	}

	void f_CallRegisteredCallback(CStr const &_Message)
	{
		m_fCallback.f_CallDiscard(_Message);
	}

	TCFuture<CStr> f_CallRegisteredCallbackWithReturn(CStr _Message)
	{
		co_return co_await m_fCallback(_Message);
	}

	TCFuture<TCVector<TCAsyncResult<CStr>>> f_CallRegisteredCallbacksWithReturn(CStr _Message)
	{
		TCFutureVector<CStr> Results;
		for (auto &Callback : m_Callbacks)
		{
			Callback.m_fCallback(_Message) > Results;
		}

		co_return co_await fg_AllDoneWrapped(Results);
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

	void f_TestPacketSizeLimit(CByteVector const &_Data) const
	{
	}

	void f_TestGeneral() const
	{
	}

	CByteVector f_TestReturnPacketSize(uint32 _Size) const
	{
		CByteVector BigData;
		BigData.f_SetLen(_Size);

		return BigData;
	}

	TCFuture<CStr> f_TestOnDisconnect()
	{
		TCPromise<CStr> Promise;

		auto &HostInfo = CActorDistributionManager::fs_GetCallingHostInfo();
		CStr HostID = HostInfo.f_GetRealHostID();

		if (HostID.f_IsEmpty())
			co_return CStr();

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

		co_return co_await Promise.f_MoveFuture();
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
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntDeferred(uint32 _Value)
	{
		TCPromise<void> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Promise](TCAsyncResult<void> &&_Result)
			{
				Promise.f_SetResult(fg_Move(_Result));
			}
		;
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_GetResultException()
	{
		TCPromise<uint32> Promise;
		Promise.f_SetException(DMibErrorInstance("Test"));
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntException(uint32 _Value)
	{
		TCPromise<void> Promise;
		Promise.f_SetException(DMibErrorInstance("Test"));
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_GetResultDeferredException()
	{
		TCPromise<uint32> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_GetResult) > [Promise](TCAsyncResult<uint32> &&_Result)
			{
				Promise.f_SetException(DMibErrorInstance("Test"));
			}
		;
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<void> f_AddIntDeferredException(uint32 _Value)
	{
		TCPromise<void> Promise;
		fg_ThisActor(this)(&CDistributedActor::f_AddInt, _Value) > [Promise](TCAsyncResult<void> &&_Result)
			{
				Promise.f_SetException(DMibErrorInstance("Test"));
			}
		;
		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<uint32> f_SharedPointer(TCSharedPointer<CStr> _pValue)
	{
		co_return _pValue->f_ToInt();
	}

	TCFuture<uint32> f_UniquePointer(TCUniquePointer<CStr> _pValue)
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

	struct CRunTestsState
	{
		CIntrusiveRefCount m_RefCount;

		virtual ~CRunTestsState() = default;

		virtual TCDistributedActor<CDistributedActor> f_CreateActor() = 0;
	};

	void fp_RunTests(TCFunction<TCSharedPointer<CRunTestsState> (CActorRunLoopTestHelper &_Helper)> const &_fCreateState, bool _bRemote, CStr const &_Path)
	{
		CActorRunLoopTestHelper RunLoopHelper;

		{
			// Init distribution manager
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / _Path;
			fg_TestAddCleanupPath(SocketPath);
			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_InitServer();
		}

		auto pState = _fCreateState(RunLoopHelper);
		{
			DMibTestPath("Direct");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActor::f_GetResult)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result, ==, 5);
		}
/*		{
			DMibTestPath("DirectTemplate");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			uint32 Result1 = Actor.f_CallActor(&CDistributedActor::f_GetResultTemplated<NTest1::CTest>)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result1, ==, 5);

			Actor.f_CallActor(&CDistributedActor::f_AddInt)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			uint32 Result2 = Actor.f_CallActor(&CDistributedActor::f_GetResultTemplated<NTest2::CTest>)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result2, ==, 5);
		}*/ // Not supported in clang
		{
			DMibTestPath("Pointers");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();

			TCFuture<uint32> SharedPointerFuture = Actor.f_CallActor(&CDistributedActor::f_SharedPointer)(fg_Construct(CStr("5")));
			uint32 SharedPointerResult = fg_Move(SharedPointerFuture).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(SharedPointerResult, ==, 5);

			TCFuture<uint32> UniquePointerFuture = Actor.f_CallActor(&CDistributedActor::f_UniquePointer)(fg_Construct(CStr("5")));
			uint32 UniquePointerResult = fg_Move(UniquePointerFuture).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(UniquePointerResult, ==, 5);
		}
		{
			DMibTestPath("Virtual");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			Actor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("VirtualActor");
			TCDistributedActor<CDistributedActorBase> Actor = pState->f_CreateActor();

			Actor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("Deferred");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			Actor.f_CallActor(&CDistributedActor::f_AddIntDeferred)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			uint32 Result = Actor.f_CallActor(&CDistributedActor::f_GetResultDeferred)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(Result, ==, 5);
		}
		{
			DMibTestPath("Exception");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto fTestCall = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_AddIntException)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			auto fTestResult = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetResultException)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;

			DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
			DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
		}
		{
			DMibTestPath("DeferredException");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto fTestCall = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_AddIntDeferredException)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			auto fTestResult = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetResultDeferredException)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;

			DMibExpectException(fTestCall(), DMibErrorInstance("Test"));
			DMibExpectException(fTestResult(), DMibErrorInstance("Test"));
		}
		{
			DMibTestPath("SetSubscription");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto TestActor = fg_ConcurrentActor();
			TCSharedPointer<TCAtomic<umint>> pTestValue = fg_Construct();
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
					Actor.f_CallActor(&CDistributedActor::f_SetSubscription)(fSubscription(), _bException).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			auto fSetSubscriptionNonPublished = [&](bool _bException)
				{
					Actor.f_CallActor(&CDistributedActor::f_SetSubscriptionNonPublished)(fSubscription(), _bException).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			{
				DMibTestPath("Normal");
				fSetSubscription(false);
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 0);

				Actor.f_CallActor(&CDistributedActor::f_ClearSubscription)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
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
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto TestActor = fg_ConcurrentActor();
			auto &TestValue = g_TestValueGetSubscription;
			auto fGetSubscription = [&]()
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetSubscription)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			{
				DMibTestPath("Normal");
				auto Subscription = fGetSubscription();
				NSys::fg_Thread_Sleep(0.1f);
				DMibExpect(TestValue.f_Load(), ==, 0);

				Subscription->f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				Subscription.f_Clear();
				DMibExpect(TestValue.f_Load(), ==, 1);

				TestValue.f_Exchange(0);
			}
		}

		auto fGetRegisterActorFunctors = [&]
			(
				TCDistributedActor<CDistributedActor> const &_Actor
				, TCActor<> const &_TestActor
				, TCSharedPointer<TCAtomic<umint>> const &_pSubscription0Called
				, TCSharedPointer<TCAtomic<umint>> const &_pSubscription1Called
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
						/ [](CStr _Message) -> TCFuture<CStr>
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
						/ [](CStr _Message) -> TCFuture<CStr>
						{
							if (_Message == "TestMessageException")
								co_return DMibErrorInstance("Test exception 1");
							co_return _Message + "1";
						}
					).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				;
			}
		;
		auto fGetRegisterActorFunctorsVector = [&]
			(
				TCDistributedActor<CDistributedActor> const &_Actor
				, TCActor<> const &_TestActor
				, TCSharedPointer<TCAtomic<umint>> const &_pSubscription0Called
				, TCSharedPointer<TCAtomic<umint>> const &_pSubscription1Called
			)
			{
				TCVector<TCActorFunctorWithID<TCFuture<CStr> (CStr _Test)>> Functors;
				Functors.f_Insert() = g_ActorFunctor(_TestActor)
					(
						g_ActorSubscription(_TestActor) / [_pSubscription0Called]
						{
							_pSubscription0Called->f_Exchange(1);
						}
					)
					/ [](CStr _Message) -> TCFuture<CStr>
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
					/ [](CStr _Message) -> TCFuture<CStr>
					{
						if (_Message == "TestMessageException")
							co_return DMibErrorInstance("Test exception 1");
						co_return _Message + "1";
					}
				;
				Functors[0].f_SetID(0);
				Functors[1].f_SetID(1);
				return _Actor.f_CallActor(&CDistributedActor::f_RegisterActorFunctorsVector)(fg_Move(Functors)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			}
		;

		auto fTestMultipleActorFunctors = [&](auto _fRegister)
			{
				TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
				auto TestActor = fg_ConcurrentActor();

				TCSharedPointer<TCAtomic<umint>> pSubscription0Called = fg_Construct();
				TCSharedPointer<TCAtomic<umint>> pSubscription1Called = fg_Construct();

				auto Subscriptions = _fRegister(Actor, TestActor, pSubscription0Called, pSubscription1Called);

				auto fCallFunctor = [&](umint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_CallActorFunctor)(_iFunctor, "Test").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					}
				;
				auto fGetCleared = [&](umint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_GetClearedActorFunctors)(_iFunctor).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
					}
				;
				auto fClearActorFunctor = [&](umint _iFunctor)
					{
						return Actor.f_CallActor(&CDistributedActor::f_ClearActorFunctor)(_iFunctor).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
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

					DMibExpectTrue(pSubscription0Called->f_Load());
				}
				{
					DMibTestPath("ClearFunctor1");

					DMibExpectTrue(pSubscription0Called->f_Load());
					DMibExpectFalse(pSubscription1Called->f_Load());

					fClearActorFunctor(1);

					DMibExpectTrue(pSubscription1Called->f_Load());
				}

				Actor.f_CallActor(&CDistributedActor::f_ResetClearedActorFunctors)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
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
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto TestActor = fg_ConcurrentActor();
			TCSharedPointer<TCAtomic<umint>> pTestValue = fg_Construct();
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
					return Actor.f_CallActor(&CDistributedActor::f_GetActorFunctorWithoutSubscription)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			auto fGetActorSubscription = [&]()
				{
					return Actor.f_CallActor(&CDistributedActor::f_GetActorFunctor)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
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
				CStr ReturnValue = ActorFunctor("Test").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(ReturnValue, ==, "TestWithSubscription");

				ActorFunctor.f_GetSubscription().f_Clear();
				bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_GetActorFunctorSubscriptionCalled)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpectTrue(bSubscriptionCalled);

				if (_bRemote)
					DMibExpectException(ActorFunctor("Test").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), DMibErrorInstance("Subscription has been removed"));

				ActorFunctor.f_Clear();
			}
		}
		{
			DMibTestPath("Return Async Generator");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Generator = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGenerator)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			auto iIterator = fg_Move(Generator).f_GetSimpleIterator().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 5);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 6);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 7);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 8);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpectFalse(!!iIterator);
		}
		{
			DMibTestPath("Return Async Generator Functor");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Functor = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctor)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			for (umint i = 0; i < 2; ++i)
			{
				DMibTestPath("Iteration {}"_f << i);
				auto Generator = Functor(i).f_CallSync();

				auto iIterator = fg_Move(Generator).f_GetSimpleIterator().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 5);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 6);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 7);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 8);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, i);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpectFalse(!!iIterator);
				fg_Move(Generator).f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			}
			fg_Move(Functor).f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		}
		{
			DMibTestPath("Return Async Generator Pipelined");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Generator = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGenerator)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			auto iIterator = fg_Move(Generator).f_GetPipelinedIterator().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 5);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 6);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 7);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(*iIterator, ==, 8);
			(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpectFalse(!!iIterator);
		}
		{
			DMibTestPath("Return Async Generator Functor Pipelined");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Functor = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctor)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			for (umint i = 0; i < 2; ++i)
			{
				DMibTestPath("Iteration {}"_f << i);
				auto Generator = Functor(i).f_CallSync();

				auto iIterator = fg_Move(Generator).f_GetPipelinedIterator().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 5);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 6);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 7);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, 8);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpect(*iIterator, ==, i);
				(++iIterator).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpectFalse(!!iIterator);
				fg_Move(Generator).f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			}
			fg_Move(Functor).f_Destroy().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		}
		{
			DMibTestPath("Send Async Generator");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
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
					.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
			;

			DMibExpect(Value, ==, 10);
		}
		{
			DMibTestPath("Send Async Generator Functor");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Value = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctorSend)
				(
					g_ActorFunctor(g_ActorSubscription / [] {}) / [](int32 _Value) -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						co_yield 2;
						co_yield 3;
						co_yield 4;
						co_yield _Value;
						co_return {};
					}
				).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
			;

			DMibExpect(Value, ==, 32);
		}
		{
			DMibTestPath("Send Async Generator Pipelined");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Value = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorSendPipelined)
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
					.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
			;

			DMibExpect(Value, ==, 10);
		}
		{
			DMibTestPath("Send Async Generator Functor Pipelined");
			auto TestActor = fg_ConcurrentActor();
			CCurrentActorScope CurrentActorScope(fg_ConcurrencyThreadLocal(), TestActor);

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Value = Actor.f_CallActor(&CDistributedActor::f_TestAsyncGeneratorFunctorSendPipelined)
				(
					g_ActorFunctor(g_ActorSubscription / [] {}) / [](int32 _Value) -> TCAsyncGenerator<int32>
					{
						co_yield 1;
						co_yield 2;
						co_yield 3;
						co_yield 4;
						co_yield _Value;
						co_return {};
					}
				).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
			;

			DMibExpect(Value, ==, 32);
		}
		{
			DMibTestPath("GetInterface");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto Interface = Actor.f_CallActor(&CDistributedActor::f_GetInterface)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			auto fCallInterface = [&]
				{
					return Interface.f_CallActor(&CDistributedActorInterface::f_Test)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;

			DMibExpect(fCallInterface(), ==, 5);

			Interface.f_GetSubscription().f_Clear();
			bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_GetInterfaceSubscriptionCalled)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpectTrue(bSubscriptionCalled);

			if (_bRemote)
				DMibExpectException(fCallInterface(), DMibErrorInstance("Remote actor no longer exists"));

			Interface.f_Clear();
		}
		{
			DMibTestPath("SetInterface");

			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto TestActor = fg_ConcurrentActor();

			auto InterfaceActor = fg_ConstructDistributedActor<CDistributedActorInterface>(fg_GetDistributionManager());

			TCSharedPointer<TCAtomic<umint>> pSubscriptionCalled = fg_Construct();
			TCDistributedActorInterfaceWithID<CDistributedActorInterface> Interface
				{
					InterfaceActor->f_ShareInterface<CDistributedActorInterface>()
					, g_ActorSubscription(TestActor) / [pSubscriptionCalled]
					{
						pSubscriptionCalled->f_Exchange(1);
					}
				}
			;

			auto Subscription = Actor.f_CallActor(&CDistributedActor::f_SetInterface)(fg_Move(Interface)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			auto fCallInterface = [&]
				{
					return Actor.f_CallActor(&CDistributedActor::f_CallSetInterface)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;

			DMibExpect(fCallInterface(), ==, 5);

			Subscription.f_Clear();
			bool bSubscriptionCalled = Actor.f_CallActor(&CDistributedActor::f_SetInterfaceSubscriptionCalled)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpectTrue(bSubscriptionCalled);

			DMibExpectFalse(pSubscriptionCalled->f_Load());

			if (_bRemote)
				DMibExpectException(fCallInterface(), DMibErrorInstance("Remote actor no longer exists"));

			Actor.f_CallActor(&CDistributedActor::f_ClearSetInterface)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			fg_Dispatch(TestActor, []{}).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			DMibExpectTrue(pSubscriptionCalled->f_Load());

			Interface.f_Clear();
		}
		{
			DMibTestPath("Callbacks");
			TCDistributedActor<CDistributedActor> Actor = pState->f_CreateActor();
			auto TestActor = fg_ConcurrentActor();

			struct CState
			{
				CMutual m_Lock;
				CEventAutoReset m_Event;
				CStr m_Message;
			};

			TCSharedPointer<CState> pState = fg_Construct();

			auto fCallBack = [pState](CStr _Message) -> TCFuture<CStr>
				{
					DMibLock(pState->m_Lock);
					pState->m_Message = _Message;
					pState->m_Event.f_Signal();

					co_return _Message;
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

			auto Subscription = Actor.f_CallActor(&CDistributedActor::f_RegisterCallback)(g_ActorFunctor(TestActor) / fCallBack).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			bool bCallbacksEmpty = Actor.f_CallActor(&CDistributedActor::f_CallbacksEmpty)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			DMibExpectFalse(bCallbacksEmpty);

			Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallback)("TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			DMibExpect(fWaitForMessage(), ==, "TestMessage");

			CStr CallbackWithReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbackWithReturn)("TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			DMibExpect(CallbackWithReturn, ==, "TestMessage");

			Subscription.f_Clear();

			bCallbacksEmpty = Actor.f_CallActor(&CDistributedActor::f_CallbacksEmpty)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpectTrue(bCallbacksEmpty);

			auto fCallback0 = [AllowDestroy = g_AllowWrongThreadDestroy](CStr _Message) -> TCFuture<CStr>
				{
					if (_Message == "TestMessageException")
						co_return DMibErrorInstance("Test exception 0");
					co_return _Message + "0";
				}
			;

			auto fCallback1 = [AllowDestroy = g_AllowWrongThreadDestroy](CStr _Message) -> TCFuture<CStr>
				{
					if (_Message == "TestMessageException")
						co_return DMibErrorInstance("Test exception 1");
					co_return _Message + "1";
				}
			;

			TCActor<> TestActor0 = fg_ConstructActor<CActor>();
			TCActor<> TestActor1 = fg_ConstructActor<CActor>();

			{
				auto MultipleSubscription0 = Actor.f_CallActor(&CDistributedActor::f_RegisterCallbacks)(g_ActorFunctor(TestActor0) / fCallback0)
					.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				;
				auto MultipleSubscription1 = Actor.f_CallActor(&CDistributedActor::f_RegisterCallbacks)(g_ActorFunctor(TestActor1) / fCallback1)
					.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				;

				TCVector<TCAsyncResult<CStr>> MultipleReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbacksWithReturn)("TestMessage")
					.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
				;
				DMibExpect(*MultipleReturn[0], ==, "TestMessage0");
				DMibExpect(*MultipleReturn[1], ==, "TestMessage1");

				MultipleReturn = Actor.f_CallActor(&CDistributedActor::f_CallRegisteredCallbacksWithReturn)("TestMessageException").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				DMibExpectException(*MultipleReturn[0], DMibErrorInstance("Test exception 0"));
				DMibExpectException(*MultipleReturn[1], DMibErrorInstance("Test exception 1"));
			}


			auto fRegisterWithError = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterWithError)(TestActor0, fCallback0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			DMibExpectException(fRegisterWithError(), DMibErrorInstance("Register failed"));

			auto fRegisterWithoutSubscription = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoSubscription)(TestActor0, fCallback0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterWithoutSubscription(), DMibErrorInstance("Invalid set of parameter and return types"));

			auto fCallNoSubscription = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallNoSubscription)("TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallNoSubscription(), DMibErrorInstance("Subscription has been removed"));

			auto fRegisterNoActor = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoActor)(fCallback0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterNoActor(), DMibErrorInstance("You must have one and only one actor in the function arguments if you include a functor"));

			auto fRegisterNoFunction = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_RegisterNoFunction)(TestActor0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fRegisterNoFunction(), DMibErrorInstance("You must have at least one function in arguments if you include an actor"));

			auto Subscription0 = Actor.f_CallActor(&CDistributedActor::f_RegisterManualCallback)(0, TestActor0, fCallback0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			auto Subscription1 = Actor.f_CallActor(&CDistributedActor::f_RegisterManualCallback)(1, TestActor1, fCallback1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			CStr ReturnMessage0 = Actor.f_CallActor(&CDistributedActor::f_CallCallback)(0, "TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(ReturnMessage0, ==, "TestMessage0");

			CStr ReturnMessage1 = Actor.f_CallActor(&CDistributedActor::f_CallCallback)(1, "TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(ReturnMessage1, ==, "TestMessage1");

			auto fCallWrong = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallWrong)("TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallWrong(), DMibErrorInstance("Trying to call function on wrong dispatch actor"));

			auto fCallWithoutDispatch = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallbackWithoutDispatch)(0, "TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallWithoutDispatch(), DMibErrorInstance("This functions needs to be dispatched on a remote actor"));

			Subscription0.f_Clear();

			auto fCallRemoved = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallback)(0, "TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallRemoved(), DMibErrorInstance("Subscription has been removed"));

			TestActor1->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());

			auto fCallDestroyed = [&]
				{
					Actor.f_CallActor(&CDistributedActor::f_CallCallback)(1, "TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				}
			;
			if (_bRemote)
				DMibExpectException(fCallDestroyed(), DMibErrorInstance("Actor 'NMib::NConcurrency::CActor' called has been deleted"));

			TestActor1.f_Clear();

			if (_bRemote)
				DMibExpectException(fCallDestroyed(), DMibErrorInstance("Remote dispatch actor no longer exists"));

			Subscription1.f_Clear();

			auto SubscriptionDual = Actor.f_CallActor(&CDistributedActor::f_RegisterDual)(TestActor0, fCallback0, fCallback1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

			CStr ReturnMessageDual = Actor.f_CallActor(&CDistributedActor::f_CallDual)("TestMessage").f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			DMibExpect(ReturnMessageDual, ==, "TestMessage0TestMessage1");
		};
	}

	void fp_BasicTests(NStr::CStr const &_Address)
	{
		CActorRunLoopTestHelper RunLoopHelper;

		CStr ServerHostID = NCryptography::fg_RandomID();
		CStr ClientHostID = NCryptography::fg_RandomID();

		CActorDistributionManagerInitSettings ServerSettings{ServerHostID, {}};
		ServerSettings.m_ReconnectDelay = 1_ms;
		CActorDistributionManagerInitSettings ClientSettings{ClientHostID, {}};
		ClientSettings.m_ReconnectDelay = 1_ms;

		TCActor<CActorDistributionManager> ServerManager = fg_ConstructActor<CActorDistributionManager>(ServerSettings);
		TCActor<CActorDistributionManager> ClientManager = fg_ConstructActor<CActorDistributionManager>(ClientSettings);

		CActorDistributionCryptographySettings ServerCryptography{ServerHostID};
		ServerCryptography.f_GenerateNewCert(fg_CreateVector<CStr>(_Address), CDistributedActorTestKeySettings{});

		NWeb::NHTTP::CURL ConnectAddress;

		if (_Address == "localhost")
		{
			CNetAddressTCPv4 Address;
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
		ListenSettings.m_ListenFlags = ENetFlag_None;
		CDistributedActorListenReference ListenReference = ServerManager(&CActorDistributionManager::f_Listen, ListenSettings).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

		TCDistributedActor<CDistributedActor> PublishedActor = ServerManager->f_ConstructActor<CDistributedActor>();

		auto ActorPublication = PublishedActor->f_Publish<CDistributedActorBase>("Test", g_Timeout / 2.0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

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
		umint RemoteEvents = 0;
		bool bRemoved = false;

		auto &ConcurrentActor = fg_ConcurrentActor();

		ClientManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, "Test"
				, ConcurrentActor
				, [&](CAbstractDistributedActor _NewActor) -> TCFuture<void>
				{
					DMibLock(RemoteLock);
					RemoteActor = _NewActor.f_GetActor<CDistributedActorBase>();
					RemoteEvent.f_Signal();
					++RemoteEvents;

					co_return {};
				}
				, [&](CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
				{
					DMibLock(RemoteLock);
					if (_RemovedActor == RemoteActor)
					{
						RemoteActor.f_Clear();
						bRemoved = true;
					}
					RemoteEvent.f_Signal();
					++RemoteEvents;

					co_return {};
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
			= ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout).m_ConnectionReference
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

		RemoteActor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		uint32 Result = RemoteActor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpect(Result, ==, 5);

		CStr RemoteCallingHostID = (RemoteActor.f_CallActor(&CDistributedActorBase::f_GetCallingHostID)()).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
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

		ClientConnectionReference.f_Disconnect().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpectExceptionType(ClientConnectionReference.f_Disconnect().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), NException::CException);

		ListenReference.f_Stop().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpectExceptionType(ListenReference.f_Stop().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout), NException::CException);
	}

	void fp_AnonymousClientTests()
	{
		CActorRunLoopTestHelper RunLoopHelper;

		CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorAnon";

		CDistributedActorTestHelperCombined TestHelper{SocketPath, RunLoopHelper.m_pRunLoop};

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
		CActorRunLoopTestHelper RunLoopHelper;

		CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorDisconnect";

		CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
		TestState.f_SeparateServerManager();
		TestState.f_Init();
		auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
		TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
		CStr SubscriptionID = TestState.f_Subscribe("Test");
		auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

		CStr HostID = Actor.f_CallActor(&CDistributedActor::f_TestOnDisconnect)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

		DMibExpect(HostID, != , "");
		bool bHasClient = Actor.f_CallActor(&CDistributedActor::f_HasOnDisconnect)(HostID).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpectTrue(bHasClient);
		TestState.f_DisconnectClient(true);
		DMibTestMark;
		TestState.f_InitClient(TestState);
		DMibTestMark;
		SubscriptionID = TestState.f_Subscribe("Test");
		DMibTestMark;
		Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);
		DMibTestMark;
		bHasClient = LocalActor(&CDistributedActor::f_HasOnDisconnect, HostID).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpectFalse(bHasClient);
	}

	void fp_InvalidStateTests()
	{
		CActorRunLoopTestHelper RunLoopHelper;

		{
			DMibTestPath("Send");
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorInvalidStateSend";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(false);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends);

			DMibExpectException
				(
					Actor.f_CallActor(&CDistributedActor::f_TestGeneral)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
					, DMibErrorInstance
					(
						"Remote host '{} [TestHelperServer]' no longer running: Invalid connection: Debug fail send"_f << TestState.f_GetServerHostID()
					)
				)
			;
		}
		{
			DMibTestPath("Send Recovery");
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorInvalidStateSendRecovery";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(true);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends);

			Actor.f_CallActor(&CDistributedActor::f_TestGeneral)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		}
		{
			DMibTestPath("Send remote");
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorInvalidStateSendRemote";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(false);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			TestState.f_BreakServerConnections(fp64::fs_Inf(), ESocketDebugFlag_FailSends);

			DMibExpectException
				(
					Actor.f_CallActor(&CDistributedActor::f_TestGeneral)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
					, DMibErrorInstance("Remote host '{} [TestHelperServer]' no longer running: Last active client connection disconnected"_f << TestState.f_GetServerHostID())
				)
			;
		}
		{
			DMibTestPath("Send remote recovery");
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorInvalidStateSendRemoteRecovery";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(true);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			TestState.f_BreakServerConnections(fp64::fs_Inf(), ESocketDebugFlag_FailSends);

			Actor.f_CallActor(&CDistributedActor::f_TestGeneral)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		}
		{
			DMibTestPath("Send remote recovery after client disconnect");

			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorInvalidStateSendRemoteRecoveryDisconnect";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(true);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			// Queue up packet that can't be received
			TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_StopProcessingReceive);
			auto Future0 = Actor.f_CallActor(&CDistributedActor::f_TestGeneral)();

			// Force connection to reconnect and delay close so server still has the old connection when client reconnects
			TestState.f_BreakClientConnection(fp64::fs_Inf(), ESocketDebugFlag_FailSends | ESocketDebugFlag_DelayClose);
			auto Future1 = Actor.f_CallActor(&CDistributedActor::f_TestGeneral)();

			fg_Move(Future0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			fg_Move(Future1).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		}
		{
			DMibTestPath("Message Size");
			CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorMessageSize";

			CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
			TestState.f_SeparateServerManager();
			TestState.f_Init(false);
			auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
			TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");
			CStr SubscriptionID = TestState.f_Subscribe("Test");
			auto Actor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

			umint CallOverhead = CActorDistributionManager::mc_RemoteCallOverhead + 4;
			umint ReturnOverhead = CActorDistributionManager::mc_RemoteCallReturnOverhead + 4;

			{
				CByteVector BigData;
				BigData.f_SetLen(CActorDistributionManager::mc_MaxMessageSize - CallOverhead + 1);

				DMibExpectException
					(
						Actor.f_CallActor(&CDistributedActor::f_TestPacketSizeLimit)(fg_Move(BigData)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
						, DMibErrorInstance("Remote call size was larger than the max allowed packet size")
					)
				;

				DMibExpectException
					(
						Actor.f_CallActor(&CDistributedActor::f_TestReturnPacketSize)(CActorDistributionManager::mc_MaxMessageSize - ReturnOverhead + 1)
						.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
						, DMibErrorInstance("Reply was larger than max allowed packet size")
					)
				;
			}
			{
				// Check that overhead is exactly correct
				CByteVector BigData;
				BigData.f_SetLen(CActorDistributionManager::mc_MaxMessageSize - CallOverhead);

				Actor.f_CallActor(&CDistributedActor::f_TestPacketSizeLimit)(fg_Move(BigData)).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
				Actor.f_CallActor(&CDistributedActor::f_TestReturnPacketSize)(CActorDistributionManager::mc_MaxMessageSize - ReturnOverhead).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
			}
		}
	}

	void fp_RepublishTests()
	{
		CActorRunLoopTestHelper RunLoopHelper;

		CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorRepublish";

		CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
		TestState.f_SeparateServerManager();
		TestState.f_Init(false);

		auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
		CStr PublicationID = TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");

		CStr SubscriptionID = TestState.f_Subscribe("Test");
		auto RemoteActor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

		DMibAssertTrue(RemoteActor);

		// Call the actor to verify it works
		RemoteActor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(5).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		uint32 Result = RemoteActor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpect(Result, ==, 5);

		// Track republish events - use shared pointer for thread safety in case of timeout
		struct CState
		{
			NThread::CMutual m_Lock;
			umint m_nRemoteEvents = 0;
			bool m_bRemoved = false;
			bool m_bAdded = false;
		};
		TCSharedPointer<CState> pState = fg_Construct();

		auto &ConcurrentActor = fg_ConcurrentActor();

		// Subscribe to actor changes to detect republish
		auto ChangeSubscription = TestState.f_GetClient().f_GetManager()
			(
				&CActorDistributionManager::f_SubscribeActors
				, "Test"
				, ConcurrentActor
				, [pState](CAbstractDistributedActor _NewActor) -> TCFuture<void>
				{
					DMibLock(pState->m_Lock);
					pState->m_bAdded = true;
					++pState->m_nRemoteEvents;
					co_return {};
				}
				, [pState](CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
				{
					DMibLock(pState->m_Lock);
					pState->m_bRemoved = true;
					++pState->m_nRemoteEvents;
					co_return {};
				}
			)
			.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
		;

		{
			DMibTestPath("Before");
			DMibLock(pState->m_Lock);
			DMibExpectFalse(pState->m_bRemoved);
			DMibExpectTrue(pState->m_bAdded);
		}

		// Reset tracking
		{
			DMibLock(pState->m_Lock);
			pState->m_bRemoved = false;
			pState->m_bAdded = false;
			pState->m_nRemoteEvents = 0;
		}

		// Call republish with timeout - this should wait for both unpublish and publish to complete
		CStr ClientHostID = TestState.f_GetClientHostID();
		TestState.f_Republish(PublicationID, ClientHostID, g_Timeout);

		{
			DMibTestPath("After");
			DMibLock(pState->m_Lock);
			DMibExpectTrue(pState->m_bRemoved);
			DMibExpectTrue(pState->m_bAdded);
		}

		// Verify actor still works after republish
		RemoteActor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(10).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		Result = RemoteActor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpect(Result, ==, 10);
	}

	void fp_RepublishNonWaitingTests()
	{
		CActorRunLoopTestHelper RunLoopHelper;

		CStr SocketPath = NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActorRepublishNonWaiting";

		CDistributedActorTestHelperCombined TestState(SocketPath, RunLoopHelper.m_pRunLoop);
		TestState.f_SeparateServerManager();
		TestState.f_Init(false);

		auto LocalActor = TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
		CStr PublicationID = TestState.f_Publish<CDistributedActor, CDistributedActorBase>(LocalActor, "Test");

		CStr SubscriptionID = TestState.f_Subscribe("Test");
		auto RemoteActor = TestState.f_GetRemoteActor<CDistributedActor>(SubscriptionID);

		DMibAssertTrue(RemoteActor);

		// Track republish events - use shared pointer for thread safety in case of timeout
		struct CState
		{
			NThread::CMutual m_Lock;
			NThread::CEventAutoReset m_RemoteEvent;
			NThread::CEvent m_AllowCallbacksEvent;
			umint m_nRemoteEvents = 0;
			bool m_bRemoved = false;
			bool m_bAdded = false;
		};
		TCSharedPointer<CState> pState = fg_Construct();

		// Signal initially so the first subscription callback can complete
		pState->m_AllowCallbacksEvent.f_SetSignaled();

		// Ensure event is signaled on scope exit to prevent deadlock if test fails
		auto SignalOnExit = g_OnScopeExit / [&]
			{
				pState->m_AllowCallbacksEvent.f_SetSignaled();
			}
		;

		auto &ConcurrentActor = fg_ConcurrentActor();

		// Subscribe to actor changes to detect republish
		// Callbacks wait on m_AllowCallbacksEvent before completing to verify f_Republish(timeout=0) doesn't wait
		auto ChangeSubscription = TestState.f_GetClient().f_GetManager()
			(
				&CActorDistributionManager::f_SubscribeActors
				, "Test"
				, ConcurrentActor
				, [pState](CAbstractDistributedActor _NewActor) -> TCFuture<void>
				{
					pState->m_AllowCallbacksEvent.f_Wait();
					DMibLock(pState->m_Lock);
					pState->m_bAdded = true;
					++pState->m_nRemoteEvents;
					pState->m_RemoteEvent.f_Signal();
					co_return {};
				}
				, [pState](CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
				{
					pState->m_AllowCallbacksEvent.f_Wait();
					DMibLock(pState->m_Lock);
					pState->m_bRemoved = true;
					++pState->m_nRemoteEvents;
					pState->m_RemoteEvent.f_Signal();
					co_return {};
				}
			)
			.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
		;

		// Reset tracking (initial subscription triggers m_bAdded)
		{
			DMibLock(pState->m_Lock);
			pState->m_bRemoved = false;
			pState->m_bAdded = false;
			pState->m_nRemoteEvents = 0;
		}

		// Reset event so callbacks will block
		pState->m_AllowCallbacksEvent.f_ResetSignaled();

		// Call republish with timeout 0 - this should NOT wait for processing
		CStr ClientHostID = TestState.f_GetClientHostID();
		TestState.f_Republish(PublicationID, ClientHostID, 0.0);

		// Verify that we did NOT wait - callbacks are blocked so events should not have been processed
		{
			DMibTestPath("Immediately after non-waiting republish");
			DMibLock(pState->m_Lock);
			DMibExpectFalse(pState->m_bRemoved);
			DMibExpectFalse(pState->m_bAdded);
		}

		// Signal event to allow callbacks to complete
		pState->m_AllowCallbacksEvent.f_SetSignaled();

		// Wait manually for both events to occur
		bool bTimedOut = false;
		while (!bTimedOut)
		{
			{
				DMibLock(pState->m_Lock);
				if (pState->m_nRemoteEvents >= 2)
					break;
			}
			bTimedOut = pState->m_RemoteEvent.f_WaitTimeout(g_Timeout);
		}

		{
			DMibTestPath("After manual wait");
			DMibLock(pState->m_Lock);
			DMibAssertFalse(bTimedOut);
			DMibExpectTrue(pState->m_bRemoved);
			DMibExpectTrue(pState->m_bAdded);
		}

		// Verify actor still works after republish
		RemoteActor.f_CallActor(&CDistributedActorBase::f_AddIntVirtual)(10).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		uint32 Result = RemoteActor.f_CallActor(&CDistributedActorBase::f_GetResultVirtual)().f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);
		DMibExpect(Result, ==, 10);
	}

public:
	void f_FunctionalTests()
	{
		DMibTestSuite("Local")
		{
			struct CState : public CRunTestsState
			{
				CState(CActorRunLoopTestHelper &_Helper)
					: m_Helper(_Helper)
				{
				}

				TCDistributedActor<CDistributedActor> f_CreateActor() override
				{
					return m_TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>();
				}

				CActorRunLoopTestHelper &m_Helper;
				CDistributedActorTestHelperCombined m_TestState{NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActor4", m_Helper.m_pRunLoop};
			};

			fp_RunTests
				(
					[&](CActorRunLoopTestHelper &_Helper) -> TCSharedPointer<CRunTestsState>
					{
						TCSharedPointer<CState> pState;
						pState = fg_Construct(_Helper);
						pState->m_TestState.f_Init();
						return pState;
					}
					, false
					, "Sockets/DistributedActorLocal"
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
				fp_BasicTests("UNIX:" + fg_GetSafeUnixSocketPath("{}/TestDistributedActor.socket"_f << NMib::NFile::CFile::fs_GetProgramDirectory()));
			};
			DMibTestSuite("Anonymous client")
			{
				fp_AnonymousClientTests();
			};

			DMibTestSuite("On disconnected")
			{
				fp_OnDisconnectedTests();
			};
			DMibTestSuite("Invalid state")
			{
				fp_InvalidStateTests();
			};
			DMibTestSuite("Republish")
			{
				fp_RepublishTests();
			};
			DMibTestSuite("Republish non-waiting")
			{
				fp_RepublishNonWaitingTests();
			};

			DMibTestSuite("Remote")
			{
				struct CState : public CRunTestsState
				{
					CState(CActorRunLoopTestHelper &_Helper)
						: m_Helper(_Helper)
					{
					}

					TCDistributedActor<CDistributedActor> f_CreateActor() override
					{
						auto Actor = m_TestState.f_GetRemoteActor<CDistributedActor>(m_SubscriptionID);

						if (!Actor)
							DMibError("Failed to distributed actor environment");

						return Actor;
					}

					CActorRunLoopTestHelper &m_Helper;
					CDistributedActorTestHelperCombined m_TestState{NFile::CFile::fs_GetProgramDirectory() / "Sockets/DistributedActor5", m_Helper.m_pRunLoop};
					CStr m_SubscriptionID;
				};

				fp_RunTests
					(
						[&](CActorRunLoopTestHelper &_Helper) -> TCSharedPointer<CRunTestsState>
						{
							TCSharedPointer<CState> pTestState = fg_Construct(_Helper);

							pTestState->m_TestState.f_SeparateServerManager();
							pTestState->m_TestState.f_Init();
							pTestState->m_TestState.f_Publish<CDistributedActor, CDistributedActorBase>
								(
									pTestState->m_TestState.f_GetServer().f_GetManager()->f_ConstructActor<CDistributedActor>()
									, "Test"
								)
							;
							pTestState->m_SubscriptionID = pTestState->m_TestState.f_Subscribe("Test");

							return pTestState;
						}
						, false
						, "Sockets/DistributedActorRemote"
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
