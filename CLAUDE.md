# CLAUDE.md - Concurrency Module

This file provides guidance to Claude Code (claude.ai/code) when working with the Concurrency module in the Malterlib framework.

## Module Overview

The Concurrency module provides a comprehensive actor-based concurrency framework with async/await support, distributed computing capabilities, and safe concurrent programming patterns. Key features include:

- **Actor Model** - Sequential execution within actors, safe concurrent execution across actors
- **Coroutines** - C++20 coroutines with `TCFuture`/`TCPromise` for async/await patterns
- **Distributed Actors** - Transparent remote actor communication across processes and hosts
- **Distributed Applications** - Framework for building distributed systems with authentication and trust management
- **Thread Management** - Run loops, thread pools, and work queues
- **Async Utilities** - AsyncResult, AsyncGenerator, futures, and promises
- **Safe Destruction** - Automatic cleanup and lifetime management

## Architecture

### Core Components

#### Actor System
- **CActor** - Base class for all actors
- **TCActor** - Template wrapper providing type-safe actor references
- **CActorHolder** - Internal management of actor lifetime and execution
- **CConcurrencyManager** - Central coordinator for actor execution and thread pools
- **CRunLoop** - Event loop for actor message processing
- **CRunQueue** - Queue for pending actor messages

#### Async/Await System
- **TCFuture** - Awaitable future type for async operations
- **TCPromise** - Promise type for setting async results
- **TCAsyncResult** - Result type containing value or exception
- **Coroutines** - C++20 coroutine support with safety checks
- **TCAsyncGenerator** - Async generator for streaming results

#### Distributed System
- **TCDistributedActor** - Remote actor reference
- **CDistributedActorManager** - Manages remote actor connections
- **CDistributedApp** - Base class for distributed applications
- **CDistributedTrustManager** - Authentication and trust for distributed actors
- **CRuntimeTypeRegistry** - Runtime type information for remote calls

### Design Principles

1. **Safe Concurrency** - Actors execute sequentially, preventing data races
2. **Always Get Result** - RAII ensures all async operations return a result or exception
3. **Safe Destruction** - Automatic cleanup when actors go out of scope
4. **Minimize Boilerplate** - Operator overloading and coroutines reduce code complexity
5. **Transparent Distribution** - Same API for local and remote actor calls
6. **Linear Scaling** - Lock-free operations and thread pool per core
7. **Low Overhead** - 100-300 cycles per actor call with saturated queue

## File Organization

```
Concurrency/
├── Include/Mib/Concurrency/       # Public headers
│   ├── Actor/                     # Actor-specific utilities
│   ├── *Actor*                    # Actor system headers
│   ├── Async*                     # Async utilities
│   ├── Coroutine                  # Coroutine support
│   ├── Distributed*               # Distributed computing
│   ├── RunLoop/RunQueue           # Execution infrastructure
│   └── ConcurrencyManager         # Central manager
├── Source/                        # Implementation files
│   ├── Actor/                     # Actor implementation
│   ├── DistributedActor/          # Distributed actor implementation
│   ├── DistributedApp/            # Distributed app framework
│   ├── DistributedTrust/          # Trust management
│   └── DistributedDDPBridge/      # DDP bridge implementation
├── Test/                          # Unit tests
├── Apps/                          # Example applications
└── Documentation/                 # Design documentation
```

## Usage Examples

### Basic Actor

```cpp
#include <Mib/Concurrency/ConcurrencyManager>

// Define an actor class
class CMyActor : public CActor
{
public:
	// Async function returning future
	TCFuture<int32> f_Calculate(int32 _Value)
	{
		// Perform async work
		co_await f_DoAsyncWork();

		// Access member data safely (single-threaded within actor)
		m_Counter += _Value;

		co_return m_Counter;
	}

	TCFuture<void> f_DoAsyncWork()
	{
		// Simulate async operation
		co_await fg_Delay(0.1);
		co_return {};
	}

private:
	int32 m_Counter = 0;
};

// Using the actor
TCFuture<void> f_UseActor()
{
	// Create actor instance
	TCActor<CMyActor> MyActor = fg_Construct<CMyActor>();

	// Call actor method (returns future)
	int32 Result = co_await MyActor(&CMyActor::f_Calculate, 5);

	DConOut("Result: {}\n", Result);

	co_return {};
}
```

### Actor with Callbacks

```cpp
class CEventActor : public CActor
{
public:
	// Register callback with subscription for lifetime management
	TCFuture<CActorSubscription> f_Subscribe(TCActorFunctor<void(int32)> _fCallback)
	{
		auto *pCallback = &m_Callbacks.f_Insert(fg_Move(_fCallback));

		// Return subscription to track lifetime
		co_return g_ActorSubscription / [this, pCallback]
			{
				m_Callbacks.f_Remove(pCallback);
			}
		;
	}

	TCFuture<void> f_TriggerEvent(int32 _Value)
	{
		for (auto &fCallback : m_Callbacks)
		{
			co_await fCallback(_Value);
		}
		co_return {};
	}

private:
	TCLinkedList<TCActorFunctor<void(int32)>> m_Callbacks;
};

// Using subscriptions
TCFuture<void> f_UseSubscription()
{
	TCActor<CEventActor> EventActor = fg_Construct<CEventActor>();

	// Subscribe with automatic cleanup
	CActorSubscription Subscription = co_await EventActor
		(
			&CEventActor::f_Subscribe
			, g_ActorFunctor / [](int32 _Value) -> TCFuture<void>
			{
				DConOut("Event: {}\n", _Value);
				co_return {};
			}
		)
	;

	// Trigger events
	co_await EventActor(&CEventActor::f_TriggerEvent, 42);

	// Subscription automatically cleaned up when it goes out of scope
	co_return {};
}
```

### Error Handling

```cpp
class CErrorActor : public CActor
{
public:
	TCFuture<int32> f_MayFail(bool _bShouldFail)
	{
		if (_bShouldFail)
			co_return DMibErrorInstance("Operation failed");

		co_return 42;
	}
};

TCFuture<void> f_HandleErrors()
{
	TCActor<CErrorActor> Actor = fg_Construct<CErrorActor>();

	// Method 1: Using TCAsyncResult
	TCAsyncResult<int32> Result = co_await Actor(&CErrorActor::f_MayFail, true).f_Wrap();

	if (!Result)
	{
		DConErrOut("Error: {}\n", Result.f_GetExceptionStr());
		co_return {};
	}

	DConOut("Success: {}\n", *Result);

	// Method 2: Let exception propagate, the TCFuture of this function will be resolved with the error
	int32 Value = co_await Actor(&CErrorActor::f_MayFail, true);
	DConOut("Value: {}\n", Value);

	co_return {};
}
```

### Distributed Actors

```cpp
// Define distributed interface
class CMyServiceInterface : public CActor
{
public:
	static constexpr ch8 const *mc_pDefaultNamespace = "com.company/MyService";

	enum : uint32
	{
		EProtocolVersion_Min = 0x101,
		EProtocolVersion_Current = 0x101
	};

	CMyServiceInterface()
	{
		// Publish functions for remote access
		DPublishActorFunction(CMyServiceInterface::f_ProcessData);
	}

	virtual TCFuture<CStr> f_ProcessData(CStr _Input) = 0;
};

// Implement service
class CMyService : public CActor
{
public:
	// Delegated implementation
	struct CServiceImpl : public CMyServiceInterface
	{
		TCFuture<CStr> f_ProcessData(CStr _Input) override
		{
			co_return m_pThis->fp_ProcessInternal(_Input);
		}

		DDelegatedActorImplementation(CMyService);
	};

	TCFuture<void> f_Publish()
	{
		// Publish service for remote access
		co_await m_ServiceInterface.f_Publish<CMyServiceInterface>(fg_GetDistributionManager(), this, 10.0);

		co_return {};
	}

private:
	CStr fp_ProcessInternal(CStr const &_Input)
	{
		return "Processed: {}"_f << _Input;
	}

	TCDistributedActorInstance<CServiceImpl> m_ServiceInterface;
};

// Client subscribing to service
TCFuture<void> f_UseRemoteService()
{
	// Subscribe to remote services
	auto RemoteServices = co_await g_TrustManager->f_SubscribeTrustedActors<CMyServiceInterface>();

	co_await RemoteServices.f_OnActor(g_ActorFunctor / [](TCDistributedActor<CMyServiceInterface> _Service, CTrustedActorInfo _Info) -> TCFuture<void>
		{
			// Call remote service
			CStr Result = co_await _Service.f_CallActor(&CMyServiceInterface::f_ProcessData)("Hello");

			DConOut("Remote result: {}\n", Result);
			co_return {};
		}
		, g_ActorFunctor / [](TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _Info) -> TCFuture<void>
		{
			DConOut("Service disconnected\n");
			co_return {};
		}
	);

	co_return {};
}
```

### Parallel Execution

```cpp
// Dispatch work to thread pool
TCFuture<void> f_ParallelWork()
{
	// Get blocking actor for I/O operations
	auto BlockingActor = fg_BlockingActor();

	// Dispatch blocking operation
	CStr FileContent = co_await
		(
			g_Dispatch(BlockingActor) / []() -> CStr
			{
				// This runs on thread pool, safe for blocking
				return CFile::fs_ReadStringFromFile("/path/to/file");
			}
		)
	;

	// Parallel foreach
	TCVector<int32> Data = {1, 2, 3, 4, 5};
	co_await fg_ParallelForEach
		(
			Data
			, [](int32 &_Value) -> TCFuture<void>
			{
				_Value *= 2;
				co_return {};
			}
		)
	;

	co_return {};
}
```

### Actor Destruction

```cpp
class CCleanupActor : public CActor
{
protected:
	// Override for async cleanup
	TCFuture<void> fp_Destroy() override
	{
		DConOut("Cleaning up...\n");

		// Perform async cleanup
		co_await fp_SaveState();
		co_await fp_CloseConnections();

		co_return {};
	}

private:
	TCFuture<void> fp_SaveState()
	{
		// Save state before destruction
		co_return {};
	}

	TCFuture<void> fp_CloseConnections()
	{
		// Close network connections
		co_return {};
	}
};

TCFuture<void> f_ManualDestroy()
{
	TCActor<CCleanupActor> Actor = fg_Construct<CCleanupActor>();

	// Manual destruction
	auto Result = co_await fg_Move(Actor).f_Destroy().f_Wrap();
	if (!Result)
		DConErrOut("Destroy failed: {}\n", Result.f_GetExceptionStr());

	// Actor also destroyed automatically when TCActor goes out of scope
	co_return {};
}
```

## Common Patterns

### Actor Call Syntax

```cpp
// Basic call with result
int32 Value = co_await MyActor(&CMyActor::f_Function, Param1, Param2);

// Call with error handling
TCAsyncResult<int32> Result = co_await MyActor(&CMyActor::f_Function, Param).f_Wrap();
if (!Result)
{
	// Handle error
}

// Discard result
MyActor(&CMyActor::f_Function).f_DiscardResult();

// With timeout
int32 Value = co_await MyActor(&CMyActor::f_Function).f_Timeout(5.0, "Operation timed out");

// Direct callback (without coroutines)
MyActor(&CMyActor::f_Function) > [](TCAsyncResult<int32> _Result)
	{
		// Handle result
	}
;
```

### Promise/Future Patterns

```cpp
// Creating promise/future pair
TCPromise<int32> Promise;
TCFuture<int32> Future = Promise.f_GetFuture();

// Setting result
Promise.f_SetResult(42);
// or
Promise.f_SetException(DMibErrorInstance("Failed"));

// Promise with callback
MyActor(&CMyActor::f_Function) > Promise / [Promise](int32 _Value)
	{
		// Exception handling done automatically
		Promise.f_SetResult(_Value * 2);
	}
;
```

### Actor Functors

```cpp
// Create actor functor
auto Functor = g_ActorFunctor / [this](int32 _Value) -> TCFuture<void>
	{
		DConOut("Received: {}\n", _Value);
		co_return {};
	}
;

// With subscription for cleanup
auto Functor = g_ActorFunctor
	(
		g_ActorSubscription / [this]() -> TCFuture<void>
		{
			// Called when functor goes out of scope
			co_await fp_Cleanup();
			co_return {};
		}
	)
	/ [this](int32 _Value) -> TCFuture<void>
	{
		co_await fp_HandleValue(_Value);
		co_return {};
	}
;
```

## Safety Considerations

### Coroutine Safety

```cpp
// UNSAFE: Reference parameters after suspension
TCUnsafeFuture<void> f_Unsafe(CStr const &_Str)
{
	co_await fg_Delay(1.0);
	DConOut("{}\n", _Str); // _Str may be invalid!
	co_return {};
}

// SAFE: Copy before suspension
TCFuture<void> f_Safe(CStr _Str) // Pass by value
{
	co_await fg_Delay(1.0);
	DConOut("{}\n", _Str); // Safe
	co_return {};
}

// SAFE: Store before first suspension
TCUnsafeFuture<void> f_SafeStore(CStr const &_Str)
{
	CStr Copy = _Str; // Store before suspension
	co_await fg_Delay(1.0);
	DConOut("{}\n", Copy); // Safe
	co_return {};
}
```

### Thread Safety

```cpp
// UNSAFE: Accessing actor internals outside actor
TCActor<CMyActor> Actor;
// Actor->m_Value = 5; // Compile error - good!

// SAFE: All access through actor calls
co_await Actor(&CMyActor::f_SetValue, 5);

// UNSAFE: Capturing 'this' in blocking dispatch
co_await
	(
		g_Dispatch(fg_BlockingActor()) / [this]()
		{
			m_Value++; // Race condition!
		}
	)
;

// SAFE: No shared state in blocking dispatch
int32 LocalCopy = m_Value;
int32 Result = co_await
	(
		g_Dispatch(fg_BlockingActor()) / [LocalCopy]()
		{
			return LocalCopy + 1; // Safe
		}
	)
;
m_Value = Result;
```

### Error Handling with Coroutines

```cpp
// INCORRECT: co_await in catch block (compilation error)
try
{
	Json = CEJsonSorted::fs_FromString(Input);
}
catch (NException::CException const &_Exception)
{
	co_await _pCommandLine->f_StdErr("Error: {}\n"_f << _Exception); // ERROR!
	co_return 1;
}

// CORRECT: Use g_CaptureExceptions
{
	auto CaptureScope = co_await (g_CaptureExceptions % "Error parsing JSON");
	Json = CEJsonSorted::fs_FromString(Input);
}

// INCORRECT: Wrong operator precedence with % and co_await
CStr Result = co_await
	(
		g_Dispatch(fg_BlockingActor()) / [Path]() -> CStr
		{
			return CFile::fs_ReadStringFromFile(Path);
		}
	) % "Error reading file"  // ERROR: % applied after co_await
;

// CORRECT: Parenthesize the % operator
CStr Result = co_await
	(
		g_Dispatch(fg_BlockingActor()) / [Path]() -> CStr
		{
			return CFile::fs_ReadStringFromFile(Path);
		}
		% "Error reading file"  // Correct: % applied to future before co_await
	)
;

```

## Testing

```bash
# Run all concurrency tests
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Concurrency*"]'

# Run specific test suites
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Concurrency/Actor*"]'
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Concurrency/Coroutines*"]'
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Concurrency/DistributedActor*"]'
```

## Dependencies

The Concurrency module depends on:
- **Core** - Basic types and utilities
- **Thread** - Threading primitives
- **Storage** - Memory management and smart pointers
- **Function** - Function wrappers and delegates
- **Exception** - Exception handling
- **Container** - Data structures
- **Database** - For distributed app persistence (indirect)
- **Web** - For distributed communication (indirect)
- **Cryptography** - For secure distributed communication (indirect)

## Configuration

Build configuration properties (UserSettings.MSettings):
```cpp
// Enable debug features
MalterlibConcurrency_DebugActorCallstacks true  // Track call stacks
MalterlibConcurrency_DebugBlockDestroy true     // Debug block destruction
MalterlibConcurrency_DebugFutures true          // Future reference debugging
MalterlibConcurrency_DebugUnobservedException true // Unobserved exceptions
MalterlibConcurrency_DebugSubscriptions true    // Subscription logging
```

## Common Pitfalls

1. **Blocking in Actors**: Never block in actor code - use `fg_BlockingActor()` for I/O
2. **Dangling References**: Use value semantics or ensure lifetime with subscriptions
3. **Unobserved Results**: Always observe async results to catch exceptions
4. **Coroutine References**: Avoid reference parameters in coroutines
5. **Cross-Actor Access**: Never access actor internals directly
6. **Forgotten co_return**: Always end coroutines with `co_return {};` for void
7. **Race Conditions**: Don't share mutable state between actors without synchronization
8. **Deadlocks**: Avoid circular actor dependencies
9. **co_await in catch blocks**: Cannot use `co_await` in catch handlers - use `g_CaptureExceptions` instead
10. **Exception message operator precedence**: When using `% "message"` with `co_await`, parenthesize properly: `co_await ((...) % "message")`

## Advanced Features

### Weak Actor References
```cpp
TCWeakActor<CMyActor> WeakRef = Actor;
if (auto StrongRef = WeakRef.f_Lock())
	co_await StrongRef(&CMyActor::f_Function);
```

### Async Generators
```cpp
TCAsyncGenerator<int32> f_GenerateNumbers()
{
	for (int32 i = 0; i < 10; ++i)
	{
		co_yield i;
		co_await fg_Delay(0.1);
	}
}

TCFuture<void> f_ConsumeNumbers()
{
	for (auto iIterator = co_await f_GenerateNumbers().f_GetIterator(); iIterator;  co_await ++iIterator)
	{
		DConOut("Number: {}\n", *iIterator);
	}

	co_return {};
}
```

### Performance Optimization
- Use `f_DiscardResult()` when you don't need the result
- Batch operations when possible to reduce actor calls
- Use `TCFutureVector` for parallel operations
- Consider actor granularity - too many actors add overhead
