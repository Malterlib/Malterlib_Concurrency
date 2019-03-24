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
	CSuspendAlways CFutureCoroutineContext::final_suspend()
	{
		if (m_pPreviousCoroutineHandler != this)
		{
			auto &ThreadLocal = **g_SystemThreadLocal;
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
			ThreadLocal.m_pCurrentCoroutineHandler = m_pPreviousCoroutineHandler;
			m_pPreviousCoroutineHandler = this;
		}
		return {};
	}

	void CFutureCoroutineContext::f_Suspend()
	{
		DMibFastCheck(this->m_nThreadLocalScopes == 0); // Oustanding thread local scopes cannot escape suspension point,
		// if needed convert thread local scope to CCoroutineThreadLocalHandler interface

		auto &ThreadLocal = **g_SystemThreadLocal;
		
		auto iHandler = this->m_ThreadLocalHandlers.f_GetIterator();
		iHandler.f_Reverse(this->m_ThreadLocalHandlers);

		for (; iHandler; --iHandler)
			iHandler->f_Suspend();

		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
		DMibFastCheck(this->m_pPreviousCoroutineHandler != this);

		ThreadLocal.m_pCurrentCoroutineHandler = this->m_pPreviousCoroutineHandler;
		this->m_pPreviousCoroutineHandler = this;
	}

#if DMibEnableSafeCheck > 0
	NException::CExceptionPointer CFutureCoroutineContext::f_Resume()
#else
	void CFutureCoroutineContext::f_Resume()
#endif
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		DMibFastCheck(this->m_pPreviousCoroutineHandler == this);
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler != this);
		this->m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;

		for (auto &Handler : this->m_ThreadLocalHandlers)
			Handler.f_Resume();

#if DMibEnableSafeCheck > 0
		try
		{
			if (m_Flags & ECoroutineFlag_UnsafeThisPointer)
				fp_CheckUnsafeThisPointer();
			else if (m_Flags & ECoroutineFlag_UnsafeReferenceParameters)
				fp_CheckUnsafeReferenceParams();
		}
		catch (NException::CDebugException const &_Exception)
		{
			return _Exception.f_ExceptionPointer();
		}
		return nullptr;
#endif
	}

#if DMibEnableSafeCheck > 0
	bool CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck = false;

	inline_never CSuspendNever CFutureCoroutineContext::initial_suspend() noexcept
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
		if (m_Flags & ECoroutineFlag_UnsafeThisPointer)
			fp_UpdateUnsafeThisPointer();
		else if (m_Flags & ECoroutineFlag_UnsafeReferenceParameters)
			fp_UpdateUnsafeReferenceParams();
		return {};
	}

	CMibCodeAddress CFutureCoroutineContext::msp_DequeueProcessAddress_ReferenceThis = nullptr;
	mint CFutureCoroutineContext::msp_DequeueProcessLocaction_ReferenceParam = 0;

	namespace NPrivate
	{
		inline_never void *fg_GetInstructionPointer()
		{
#if defined(DCompiler_clang)
			return __builtin_return_address(0);
#elif defined(DCompiler_MSVC)
			return _ReturnAddress();
#else
#			error "Implement this"
#endif
		}

		volatile mint g_WrapActorCallUnoptimizer = 0;
		uint8 *g_pDequeueProcessFunctionPointer = nullptr;

		inline_never mint fg_WrapActorCall(NFunction::TCFunctionNoAllocMovable<void ()> &&_fDoDisptach)
		{
			if (!g_pDequeueProcessFunctionPointer)
				g_pDequeueProcessFunctionPointer = (uint8 *)fg_GetInstructionPointer();

			NAtomic::fg_CompilerFence();

			mint Return = ++g_WrapActorCallUnoptimizer;
			_fDoDisptach();
			return Return;
		}
	}

	inline_never void CFutureCoroutineContext::fp_UpdateUnsafeReferenceParams()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		m_Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				m_Callstack.m_Callstack
				, sizeof(m_Callstack.m_Callstack) / sizeof(m_Callstack.m_Callstack[0])
			)
		;

		mint DequeueCallstackLocation = msp_DequeueProcessLocaction_ReferenceParam;
		if (DequeueCallstackLocation == 0)
		{
			smint BestOffset = TCLimitsInt<smint>::mc_Max;
			for (mint iCallstack = 0; iCallstack < m_Callstack.m_CallstackLen; ++iCallstack)
			{
				smint Offset = (uint8 *)m_Callstack.m_Callstack[iCallstack] - NPrivate::g_pDequeueProcessFunctionPointer;
				if (Offset >= 0 && Offset < BestOffset)
				{
					BestOffset = Offset;
					msp_DequeueProcessLocaction_ReferenceParam = DequeueCallstackLocation = iCallstack;
					msp_DequeueProcessAddress_ReferenceThis = m_Callstack.m_Callstack[iCallstack];
				}
			}
			DMibFastCheck(DequeueCallstackLocation != 0);
		}

		m_bIsCalledSafely = ThreadLocal.m_bExpectCoroutineCall
			&& m_Callstack.m_Callstack[DequeueCallstackLocation] == msp_DequeueProcessAddress_ReferenceThis
		;
		if (!m_bIsCalledSafely)
		{
			int x = 0;
			++x;
		}
		ThreadLocal.m_bExpectCoroutineCall = false;
	}

	mint CFutureCoroutineContext::msp_WrapDispatchWithReturnLocaction_ReferenceThis = 0;
	CMibCodeAddress CFutureCoroutineContext::msp_WrapDispatchWithReturnAddress_ReferenceThis = nullptr;

	namespace NPrivate
	{
		volatile mint g_WrapDispatchWithReturnUnoptimizer = 0;
		uint8 *g_pWrapDispatchWithReturnFunctionPointer = nullptr;

		inline_never mint fg_WrapDispatchWithReturn(NFunction::TCFunction<void ()> const &_fDoDisptach)
		{
			if (!g_pWrapDispatchWithReturnFunctionPointer)
				g_pWrapDispatchWithReturnFunctionPointer = (uint8 *)fg_GetInstructionPointer();

			NAtomic::fg_CompilerFence();

			mint Return = ++g_WrapDispatchWithReturnUnoptimizer;
			_fDoDisptach();
			return Return;
		}
	}

	inline_never void CFutureCoroutineContext::fp_UpdateUnsafeThisPointer()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		m_Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				m_Callstack.m_Callstack
				, sizeof(m_Callstack.m_Callstack) / sizeof(m_Callstack.m_Callstack[0])
			)
		;

		mint WrapDispatchWithReturnLocation = msp_WrapDispatchWithReturnLocaction_ReferenceThis;
		if (WrapDispatchWithReturnLocation == 0)
		{
			smint BestOffset = TCLimitsInt<smint>::mc_Max;
			for (mint iCallstack = 0; iCallstack < m_Callstack.m_CallstackLen; ++iCallstack)
			{
				smint Offset = (uint8 *)m_Callstack.m_Callstack[iCallstack] - NPrivate::g_pWrapDispatchWithReturnFunctionPointer;
				if (Offset >= 0 && Offset < BestOffset)
				{
					BestOffset = Offset;
					msp_WrapDispatchWithReturnLocaction_ReferenceThis = WrapDispatchWithReturnLocation = iCallstack;
					msp_WrapDispatchWithReturnAddress_ReferenceThis = m_Callstack.m_Callstack[iCallstack];
				}
			}
			DMibFastCheck(WrapDispatchWithReturnLocation != 0);
		}

		m_bIsCalledSafely = ThreadLocal.m_bExpectCoroutineCall
			&& m_Callstack.m_Callstack[WrapDispatchWithReturnLocation] == msp_WrapDispatchWithReturnAddress_ReferenceThis
		;
		if (!m_bIsCalledSafely)
		{
			int x = 0;
			++x;
		}
		ThreadLocal.m_bExpectCoroutineCall = false;
	}

	inline_never void CFutureCoroutineContext::fp_CheckUnsafeReferenceParams()
	{
		if (m_Flags & ECoroutineFlag_AllowReferences)
			return;

		if (!m_bIsCalledSafely)
		{
			if (ms_bDebugCoroutineSafeCheck)
			{
				DMibSafeCheck(false, "Unsafe call to coroutine with reference parameters");
			}
			else
			{
				NSys::fg_DebugOutput("Unsafe call to coroutine with reference parameters:\n");
				m_Callstack.f_Trace(4);
				DMibFastCheck(false);
			}
		}

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

		// If you are sure that the reference usage is safe you can instead use co_await ECoroutineFlag_AllowReferences:

		#if 0
			TCFuture<void> f_MyOtherCoroutineFunction(uint32 const &)
			{
				co_await ECoroutineFlag_AllowReferences;
				...
			}

			TCFuture<void> f_MyCoroutineFunction()
			{
				uint32 ByReference = 5;
				co_await f_MyOtherCoroutineFunction(ByReference);
				co_return {};
			}
		#endif

		// Or change the parameter to be non reference:

		#if 0
			TCFuture<void> f_MyOtherCoroutineFunction(uint32);

			TCFuture<void> f_MyCoroutineFunction()
			{
				uint32 ByReference = 5;
				co_await f_MyOtherCoroutineFunction(ByReference);
				co_return {};
			}
		#endif
	}

	inline_never void CFutureCoroutineContext::fp_CheckUnsafeThisPointer()
	{
		if (m_Flags & ECoroutineFlag_AllowReferences)
			return;

		if (!m_bIsCalledSafely)
		{
			if (ms_bDebugCoroutineSafeCheck)
			{
				DMibSafeCheck(false, "Unsafe call to coroutine with reference this pointer");
			}
			else
			{
				NSys::fg_DebugOutput("Unsafe call to coroutine with reference this pointer:\n");
				m_Callstack.f_Trace(4);
				DMibFastCheck(false);
			}
		}

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
