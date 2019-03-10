// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency::NPrivate
{
	void TCFutureCoroutineContextValue<void>::return_value(CVoidTag &&_Value)
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

	CMibCodeAddress CFutureCoroutineContext::msp_DequeueProcessAddress_ReferenceThis = nullptr;
	mint CFutureCoroutineContext::msp_DequeueProcessLocaction_ReferenceParam = 0;

	namespace NPrivate
	{
		volatile mint g_WrapActorCallUnoptimizer = 0;
		inline_never mint fg_WrapActorCall(NFunction::TCFunctionNoAlloc<void ()> const &_fDoDisptach)
		{
			mint Return = ++g_WrapActorCallUnoptimizer;
			_fDoDisptach();
			return Return;
		}
	}

	inline_never void CFutureCoroutineContext::fp_CheckUnsafeReferenceParams()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack
				, sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack) / sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[0])
			)
		;

		mint DequeueCallstackLocation = msp_DequeueProcessLocaction_ReferenceParam;
		if (DequeueCallstackLocation == 0)
		{
			uint8 *pDequeueProcessFunctionPointer = (uint8 *)(void *)&NPrivate::fg_WrapActorCall;
			smint BestOffset = TCLimitsInt<smint>::mc_Max;
			for (mint iCallstack = 0; iCallstack < ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen; ++iCallstack)
			{
				smint Offset = (uint8 *)ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[iCallstack] - pDequeueProcessFunctionPointer;
				if (Offset >= 0 && Offset < BestOffset)
				{
					BestOffset = Offset;
					msp_DequeueProcessLocaction_ReferenceParam = DequeueCallstackLocation = iCallstack;
					msp_DequeueProcessAddress_ReferenceThis = ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[iCallstack];
				}
			}
			DMibFastCheck(DequeueCallstackLocation != 0);
		}

		bool bSafeCall = ThreadLocal.m_bExpectCoroutineCall
			&& ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[DequeueCallstackLocation] == msp_DequeueProcessAddress_ReferenceThis
		;

#	if DMibEnableSafeCheck > 0
		if (!bSafeCall)
		{
			int x = 0;
			++x;
//			DMibConOut("DequeueCallstackLocation {}\n", DequeueCallstackLocation);
//			ThreadLocal.m_LastUnsafeCoroutineCallstack.f_Trace(4);
		}
#endif

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

	mint CFutureCoroutineContext::msp_WrapDispatchWithReturnLocaction_ReferenceThis = 0;
	CMibCodeAddress CFutureCoroutineContext::msp_WrapDispatchWithReturnAddress_ReferenceThis = nullptr;

	namespace NPrivate
	{
		volatile mint g_WrapDispatchWithReturnUnoptimizer = 0;
		inline_never mint fg_WrapDispatchWithReturn(NFunction::TCFunction<void ()> const &_fDoDisptach)
		{
			mint Return = ++g_WrapDispatchWithReturnUnoptimizer;
			_fDoDisptach();
			return Return;
		}
	}

	inline_never void CFutureCoroutineContext::fp_CheckUnsafeThisPointer()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack
				, sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack) / sizeof(ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[0])
			)
		;

		mint WrapDispatchWithReturnLocation = msp_WrapDispatchWithReturnLocaction_ReferenceThis;
		if (WrapDispatchWithReturnLocation == 0)
		{
			uint8 *pWrapDispatchWithReturnFunctionPointer = (uint8 *)(void *)&NPrivate::fg_WrapDispatchWithReturn;
			smint BestOffset = TCLimitsInt<smint>::mc_Max;
			for (mint iCallstack = 0; iCallstack < ThreadLocal.m_LastUnsafeCoroutineCallstack.m_CallstackLen; ++iCallstack)
			{
				smint Offset = (uint8 *)ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[iCallstack] - pWrapDispatchWithReturnFunctionPointer;
				if (Offset >= 0 && Offset < BestOffset)
				{
					BestOffset = Offset;
					msp_WrapDispatchWithReturnLocaction_ReferenceThis = WrapDispatchWithReturnLocation = iCallstack;
					msp_WrapDispatchWithReturnAddress_ReferenceThis = ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[iCallstack];
				}
			}
			DMibFastCheck(WrapDispatchWithReturnLocation != 0);
		}

		bool bSafeCall = ThreadLocal.m_bExpectCoroutineCall
			&& ThreadLocal.m_LastUnsafeCoroutineCallstack.m_Callstack[WrapDispatchWithReturnLocation] == msp_WrapDispatchWithReturnAddress_ReferenceThis
		;

#	if DMibEnableSafeCheck > 0
		if (!bSafeCall)
		{
			int x = 0;
			++x;
//			DMibConOut("WrapDispatchWithReturnLocation {}\n", WrapDispatchWithReturnLocation);
//			ThreadLocal.m_LastUnsafeCoroutineCallstack.f_Trace(4);
		}
#endif

		DMibSafeCheck
			(
				bSafeCall
				, "Unsafe call to coroutine with reference this pointer"
			)
		;

		// If you hit this assert you have called a coroutine (probably a lamdba) in an unsafe way.
		// Instead of calling coroutine directly, dispatch it instead:
		#if 0
			self / [=] () -> TCFuture<void>
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
					self / [=] () -> TCFuture<void>
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
