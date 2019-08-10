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

	void TCFutureCoroutineContextValue<void>::return_value(CVoidTag const &_Value)
	{
		m_pPromiseData->f_SetResult();
	}

#if DMibEnableSafeCheck > 0
	void TCFutureCoroutineContextValue<void>::f_SetOwner(TCWeakActor<> const &_CoroutineOwner)
	{
		m_pPromiseData->m_CoroutineOwner = _CoroutineOwner;
	}
#endif
}

namespace NMib::NConcurrency
{
	CSuspendNever CFutureCoroutineContext::final_suspend()
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

	void CActor::fp_AbortSuspendedCoroutines()
	{
		while (!mp_SuspendedCoroutines.f_IsEmpty())
			mp_SuspendedCoroutines.f_GetFirst()->f_Abort();
	}

	void CActor::f_SuspendCoroutine(CFutureCoroutineContext &_CoroutineContext)
	{
		mp_SuspendedCoroutines.f_Insert(_CoroutineContext);
	}

	void CFutureCoroutineContext::f_Suspend(bool _bAddToActor)
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
		if (_bAddToActor)
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto pCurrentActor = ConcurrencyThreadLocal.m_pCurrentActor;

			// This can happen when trying to suspend a coroutine while destroying an actor.
			// In this case use fg_ContinueRunningOnActor to transfer control of the coroutine to another actor
			// Make sure that you don't access any state that is dangling after the resumtion of the coroutine

			DMibFastCheck(pCurrentActor && pCurrentActor != ConcurrencyThreadLocal.m_pCurrentlyDestructingActor);

			if (pCurrentActor)
				pCurrentActor->f_SuspendCoroutine(*this);
		}
		this->m_pPreviousCoroutineHandler = this;
	}

#if DMibEnableSafeCheck > 0
	NException::CExceptionPointer CFutureCoroutineContext::f_Resume()
#else
	void CFutureCoroutineContext::f_Resume()
#endif
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		m_Link.f_Unlink();

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

#if DMibConcurrencySupportCoroutineFreeFunctionDebug
	extern "C" void fg_MalterlibConcurrency_TCFutureFunctionEnter(void *_pThisFunction, void *_pCallSite)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		ThreadLocal.m_bExpectCoroutineCall = false;
	}
#endif

#if DMibEnableSafeCheck > 0

	bool CFutureCoroutineContext::ms_bDebugCoroutineSafeCheck = false;

	inline_never void CFutureCoroutineContext::fp_UpdateSafeCall(bool _bSafeCall)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
		if (m_Flags & ECoroutineFlag_UnsafeThisPointer)
			fp_UpdateUnsafeThisPointer(_bSafeCall);
		else if (m_Flags & ECoroutineFlag_UnsafeReferenceParameters)
			fp_UpdateUnsafeReferenceParams(_bSafeCall);
	}

	inline_never void CFutureCoroutineContext::fp_UpdateUnsafeReferenceParams(bool _bSafeCall)
	{
		m_Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				m_Callstack.m_Callstack
				, sizeof(m_Callstack.m_Callstack) / sizeof(m_Callstack.m_Callstack[0])
			)
		;

		m_bIsCalledSafely = _bSafeCall;

		if (!m_bIsCalledSafely)
		{
			int x = 0;
			++x;
		}
	}

	inline_never void CFutureCoroutineContext::fp_UpdateUnsafeThisPointer(bool _bSafeCall)
	{
		m_Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace
			(
				m_Callstack.m_Callstack
				, sizeof(m_Callstack.m_Callstack) / sizeof(m_Callstack.m_Callstack[0])
			)
		;

		m_bIsCalledSafely = _bSafeCall;

		if (!m_bIsCalledSafely)
		{
			int x = 0;
			++x;
		}
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
