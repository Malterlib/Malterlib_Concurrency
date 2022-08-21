// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency::NPrivate
{
	void TCFutureCoroutineContext<void>::return_value(CVoidTag &&_Value)
	{
		m_pPromiseData->f_SetResultNoReport();
	}

	void TCFutureCoroutineContext<void>::return_value(CVoidTag const &_Value)
	{
		m_pPromiseData->f_SetResultNoReport();
	}
}

namespace NMib::NConcurrency
{
	auto CFutureCoroutineContext::initial_suspend() noexcept -> CInitialSuspend
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;

		for (auto &Scope : ThreadLocal.m_CrossActorStateScopes)
			Scope.f_InitialSuspend();

		return {};
	}

	CSuspendNever CFutureCoroutineContext::final_suspend() noexcept
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

#ifdef DMibSanitizerEnabled_Address
	inline_never bool CFutureCoroutineContext::CSupendNeverWorkaround::await_ready() const noexcept
	{
		return true;
	}

	inline_never void CFutureCoroutineContext::CSupendNeverWorkaround::await_suspend(TCCoroutineHandle<>) const noexcept
	{
	}

	inline_never void CFutureCoroutineContext::CSupendNeverWorkaround::await_resume() const noexcept
	{
	}
#endif

	void CActor::fp_AbortSuspendedCoroutines()
	{
		while (!mp_SuspendedCoroutines.f_IsEmpty())
			mp_SuspendedCoroutines.f_GetFirst()->f_Abort();
	}

	void CActor::f_SuspendCoroutine(CFutureCoroutineContext &_CoroutineContext)
	{
		if (self.m_pThis->mp_bIsAlwaysAlive)
			return;

		mp_SuspendedCoroutines.f_Insert(_CoroutineContext);
	}

	void CFutureCoroutineContext::f_Suspend(bool _bAddToActor)
#if DMibEnableSafeCheck <= 0
		noexcept
#endif
	{
		DMibFastCheck(this->m_nThreadLocalScopes == 0); // Oustanding thread local scopes cannot escape suspension point,
		// if needed convert thread local scope to CCoroutineThreadLocalHandler interface

		auto &ThreadLocal = **g_SystemThreadLocal;

		for (auto &Scope : ThreadLocal.m_CrossActorStateScopes)
		{
			auto fRestoreScope = Scope.f_StoreState(true);
			if (fRestoreScope)
				m_RestoreScopes.f_Insert(fg_Move(fRestoreScope));
		}

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

#if DMibEnableSafeCheck > 0
		if (m_Flags & ECoroutineFlag_UnsafeThisPointer)
			fp_CheckUnsafeThisPointer();
		else if (m_Flags & ECoroutineFlag_UnsafeReferenceParameters)
			fp_CheckUnsafeReferenceParams();
#endif
	}

	NContainer::TCVector<NFunction::TCFunctionMovable<void (bool _bException)>> CFutureCoroutineContext::f_Resume(bool &o_bAborted, bool _bException)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		m_Link.f_Unlink();

		DMibFastCheck(this->m_pPreviousCoroutineHandler == this);
		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler != this);
		this->m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;

		auto iHandler = this->m_ThreadLocalHandlers.f_GetIterator();
		try
		{
			for (; iHandler; ++iHandler)
				iHandler->f_Resume();

			auto RestoreScopes = fg_Move(m_RestoreScopes);

			for (auto &fRestoreScope : RestoreScopes)
				fRestoreScope(_bException);

			return RestoreScopes;
		}
		catch (...)
		{
			if (iHandler)
				--iHandler;
			for (; iHandler; --iHandler)
				iHandler->f_Suspend();
			f_ResumeException();
			f_Abort();

			o_bAborted = true;
			
			return {};
		}
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
		m_bIsCalledSafely = _bSafeCall;
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

				NException::CCallstack Callstack;
				Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(Callstack.m_Callstack, sizeof(Callstack.m_Callstack) / sizeof(Callstack.m_Callstack[0]));
				Callstack.f_Trace(4);

				DMibFastCheck(false);
			}
		}

		#if 0

		// If you hit this assert you have called a coroutine with reference parameters in an unsafe way.
		// Instead of calling coroutine directly do it through self:

		TCFuture<void> f_MyOtherCoroutineFunction(uint32 const &);

		TCFuture<void> f_MyCoroutineFunction()
		{
			uint32 ByReference = 5;
			co_await self(&CMyActor::f_MyOtherCoroutineFunction, ByReference);
			co_return {};
		}

		TCFuture<void> fg_MyOtherCoroutineFunction(uint32 const &);

		TCFuture<void> fg_MyCoroutineFunction()
		{
			uint32 ByReference = 5;
			co_await fg_CallSafe(&fg_MyOtherCoroutineFunction, ByReference);
			co_return {};
		}

		// If you are sure that the reference usage is safe you can instead use co_await ECoroutineFlag_AllowReferences:

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

		// Or change the parameter to be non reference:

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
				
				NException::CCallstack Callstack;
				Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(Callstack.m_Callstack, sizeof(Callstack.m_Callstack) / sizeof(Callstack.m_Callstack[0]));
				Callstack.f_Trace(4);

				DMibFastCheck(false);
			}
		}

		#if 0

		// If you hit this assert you have called a coroutine (probably a lamdba) in an unsafe way.
		// Instead of calling coroutine directly, dispatch it instead:
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
