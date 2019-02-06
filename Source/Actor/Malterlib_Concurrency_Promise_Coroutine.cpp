// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency::NPrivate
{
	void TCFutureCoroutineContextValue<void>::return_value(CVoidSentinel &&_Value)
	{
		m_pPromiseData->f_SetResult();
	}
}

namespace NMib::NConcurrency
{
	void CFutureCoroutineContext::f_Suspend()
	{
		DMibFastCheck(this->m_nThreadLocalScopes == 0); // Oustanding thread local scopes cannot escape suspension point,
		// if needed convert thread local scope to CCoroutineThreadLocalHandler interface

		auto iHandler = this->m_ThreadLocalHandlers.f_GetIterator();
		iHandler.f_Reverse(this->m_ThreadLocalHandlers);

		for (; iHandler; --iHandler)
			iHandler->f_Suspend();

		auto &ThreadLocal = **g_SystemThreadLocal;
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
		DMibFastCheck(this->m_pPreviousCoroutineHandler != this);

		ThreadLocal.m_pCurrentCoroutineHandler = this->m_pPreviousCoroutineHandler;
		this->m_pPreviousCoroutineHandler = this;
	}

	void CFutureCoroutineContext::f_Resume()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		DMibFastCheck(this->m_pPreviousCoroutineHandler == this);
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler != this);
		this->m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;

		for (auto &Handler : this->m_ThreadLocalHandlers)
			Handler.f_Resume();
	}

#if DMibEnableSafeCheck > 0
	void CFutureCoroutineContext::fp_CheckUnsafeReferenceParams()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack
				, sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack) / sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[0])
			)
		;
#		if defined(DCompiler_MSVC)
#			if defined(DConfig_Release) || defined(DConfig_ReleaseTesting) || defined(DConfig_Optimized)
#				if defined(DArchitecture_x86)
					mint DequeueCallstackLocation = 5;
#				elif defined(DArchitecture_x64)
					mint DequeueCallstackLocation = 7;
#				else
#					error "Implement this"
#				endif
#			elif defined(DConfig_DebugInlined)
				mint DequeueCallstackLocation = 8;
#			else
				mint DequeueCallstackLocation = 10;
#			endif
#		elif defined (DCompiler_clang)
			mint DequeueCallstackLocation = 9;
#		else
#			error "Implement this"
#		endif

		bool bSafeCall = ThreadLocal.m_bExpectCoroutineCall && ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[DequeueCallstackLocation] == ThreadLocal.m_CurrentActorCallParent;

		if (!bSafeCall)
		{
			int x = 0;
			++x;
		}

		DMibSafeCheck
			(
				bSafeCall
				, "Unsafe call to coroutine with reference parameters"
			)
		;

		// If you hit this assert you have called a coroutine with reference parameters in an unsafe way.
		// Instead of calling coroutine directly do it through self:

		#if 0
			TCFuture<void> f_MyOtherCoroutineFunction(uint32 const &);

			TCFuture<void> f_MyCoroutineFunction()
			{
				uint32 ByReference = 5;
				co_await self(&CMyActor::f_MyOtherCoroutineFunction, ByReference);
				co_return {};
			}
		#endif

		// If you are sure that the reference usage is safe you can instead use TCFutureAllowReferences:

		#if 0
			TCFutureAllowReferences<void> f_MyOtherCoroutineFunction(uint32 const &);

			TCFuture<void> f_MyCoroutineFunction()
			{
				uint32 ByReference = 5;
				co_await f_MyOtherCoroutineFunction(ByReference);
				co_return {};
			}
		#endif

		// Or change the parameter to be non reference:

		#if 0
			TCFutureAllowReferences<void> f_MyOtherCoroutineFunction(uint32);

			TCFuture<void> f_MyCoroutineFunction()
			{
				uint32 ByReference = 5;
				co_await f_MyOtherCoroutineFunction(ByReference);
				co_return {};
			}
		#endif
	}

	void CFutureCoroutineContext::fp_CheckUnsafeThisPointer()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		if (!(m_Flags & EFutureCoroutineContextFlag_UnsafeReferenceParameters))
		{
			ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace
				(
					ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack
					, sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack) / sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[0])
				)
			;
		}

#		if defined(DCompiler_MSVC)
#			if defined(DConfig_Release) || defined(DConfig_ReleaseTesting) || defined(DConfig_Optimized)
#				if defined(DArchitecture_x86)
					mint DequeueCallstackLocation = 7;
#				elif defined(DArchitecture_x64)
					mint DequeueCallstackLocation = 9;
#				else
#					error "Implement this"
#				endif
#			elif defined(DConfig_DebugInlined)
				mint DequeueCallstackLocation = 10;
#			else
				mint DequeueCallstackLocation = 13;
#			endif
#		elif defined (DCompiler_clang)
			mint DequeueCallstackLocation = 12;
#		else
#			error "Implement this"
#		endif

		bool bSafeCall = ThreadLocal.m_bExpectCoroutineCall
			&&
			(
				ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[DequeueCallstackLocation] == ThreadLocal.m_CurrentActorCallParent
				|| ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[DequeueCallstackLocation+1] == ThreadLocal.m_CurrentActorCallParent
			)
		;

		if (!bSafeCall)
		{
			int x = 0;
			++x;
		}

		DMibSafeCheck
			(
				bSafeCall
				, "Unsafe call to coroutine with reference this pointer"
			)
		;

		// If you hit this assert you have called a coroutine (probably a lamdba) in an unsafe way.
		// Instead of calling coroutine directly, dispatch it instead:
		#if 0
			g_Dispatch / [=] () -> TCFuture<void>
				{
					co_await self(&CMyActor::f_MyOtherCoroutineFunction);
					co_return {};
				}
				> [](TCAsyncResult<void> &&_Result)
				{
					// Handle error here
				}
			;

			// Or
			co_await
				(
					g_Dispatch / [=] () -> TCFuture<void>
					{
						co_await self(&CMyActor::f_MyOtherCoroutineFunction);
						co_return {};
					}
				)
			;
		#endif
	}
#endif
}
