// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/AsyncGenerator>

namespace NMib::NConcurrency
{
	CCurrentlyProcessingActorScope::CCurrentlyProcessingActorScope(CConcurrencyThreadLocal &_ThreadLocal, CActorHolder *_pActor)
		: mp_ThreadLocal(_ThreadLocal)
	{
		mp_pLastActor = _ThreadLocal.m_pCurrentlyProcessingActorHolder;
		_ThreadLocal.m_pCurrentlyProcessingActorHolder = _pActor;

		mp_bLastProcessing = _ThreadLocal.m_bCurrentlyProcessingInActorHolder;
		_ThreadLocal.m_bCurrentlyProcessingInActorHolder = true;
	}

	CCurrentlyProcessingActorScope::CCurrentlyProcessingActorScope(CConcurrencyThreadLocal &_ThreadLocal, TCActor<CActor> const &_Actor)
		: mp_ThreadLocal(_ThreadLocal)
	{
		mp_pLastActor = _ThreadLocal.m_pCurrentlyProcessingActorHolder;
		_ThreadLocal.m_pCurrentlyProcessingActorHolder = _Actor.f_Get();

		mp_bLastProcessing = _ThreadLocal.m_bCurrentlyProcessingInActorHolder;
		_ThreadLocal.m_bCurrentlyProcessingInActorHolder = true;
	}

	CCurrentlyProcessingActorScope::~CCurrentlyProcessingActorScope()
	{
		mp_ThreadLocal.m_pCurrentlyProcessingActorHolder = mp_pLastActor;
		mp_ThreadLocal.m_bCurrentlyProcessingInActorHolder = mp_bLastProcessing;
	}

	CActor::CActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		DMibFastCheck(ThreadLocal.m_pCurrentlyConstructingActor == this); // You can only construct actors through concurrency manager

		ThreadLocal.m_pCurrentlyProcessingActorHolder->mp_pActorUnsafe.f_Store(this, NAtomic::EMemoryOrder_Relaxed);

		self.m_pThis = ThreadLocal.m_pCurrentlyProcessingActorHolder;
	}

	CActor::~CActor()
	{
		DMibFastCheck(mp_SuspendedCoroutines.f_IsEmpty());
	}

#if DMibEnableSafeCheck > 0
	CActorInternal::CActorInternal()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pThis = ThreadLocal.m_pCurrentlyConstructingActor;
		DMibFastCheck(!!mp_pThis);
	}

	CActorInternal::~CActorInternal()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		// The lifetime of this object needs to be tied to the containing actor
		DMibFastCheck(ThreadLocal.m_pCurrentlyDestructingActor == mp_pThis);
	}
#endif

	bool CActor::f_IsDestroyed() const
	{
		return !!self.m_pThis.f_GetBits();
	}

	TCFuture<void> CActor::f_Dispatch(NFunction::TCFunctionMovable<void ()> _fToDisptach)
	{
		_fToDisptach();

		co_return {};
	}

	void CActor::fp_Construct()
	{
	}

	NException::CExceptionPointer CActor::fp_CheckDestroyed()
	{
		if (f_IsDestroyed())
			return NException::fg_MakeException(DMibImpExceptionInstance(CExceptionActorIsBeingDestroyed, "Actor is being destroyed"));
		return nullptr;
	}

	CFutureCoroutineContextOnResumeScopeAwaiter CActor::f_CheckDestroyedOnResume() const
	{
		return fg_OnResume
			(
				[this]() -> NException::CExceptionPointer
				{
					if (f_IsDestroyed())
						return DMibImpExceptionInstance(CExceptionActorIsBeingDestroyed, "Shutting down");

					return {};
				}
			)
		;
	}

	TCFuture<void> CActor::fp_DestroyInternal()
	{
		if (f_IsDestroyed())
			co_return DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed");

		self.m_pThis.f_SetBits(1);

		co_await fp_Destroy();

		if (mp_SuspendedCoroutines.f_IsEmpty())
			co_return {};

		// Give co-routines a chance to finish after fp_Destroy has been called
		co_await g_Yield;

		if (mp_SuspendedCoroutines.f_IsEmpty())
			co_return {};

		auto Future = fp_AbortSuspendedCoroutinesWithAsyncDestroy();
		if (Future.f_IsValid())
			co_await fg_Move(Future);

		co_return {};
	}

	TCFuture<void> CActor::fp_Destroy()
	{
		return g_Void;
	}

#if DMibEnableSafeCheck > 0
	void CActor::fp_CheckDestroy()
	{
	}
#endif

	NPrivate::CThisActor::operator TCActor<> () const
	{
		TCActorInternal<CActor> *pActor = (TCActorInternal<CActor> *)m_pThis.f_Get();
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCActor<>{fg_Explicit(pActor)};
	}

	CCanDestroyTracker::CCanDestroyTracker() = default;

	CCanDestroyTracker::~CCanDestroyTracker()
	{
		if (!m_Promise.f_IsSet())
			m_Promise.f_SetResult();
	}

	CCanDestroyTracker::CCanDestroyResultFunctor CCanDestroyTracker::f_Track()
	{
		return CCanDestroyResultFunctor{fg_Explicit(this)};
	}

	TCFuture<void> CCanDestroyTracker::f_Future() const
	{
		return m_Promise.f_Future();
	}

	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(CCanDestroyResultFunctor &&) = default;
	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(CCanDestroyResultFunctor const &) = default;

	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(NStorage::TCSharedPointer<CCanDestroyTracker> &&_pThis)
		: m_pThis(fg_Move(_pThis))
	{
	}

	void CCanDestroyTracker::CCanDestroyResultFunctor::operator () (TCAsyncResult<void> &&)
	{
	}

	template <>
	auto TCPromise<void>::f_ReceiveAny() const & -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>
	{
		return NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>{*this};
	}

	template <>
	auto TCPromise<void>::f_ReceiveAnyUnwrap() const & -> NPrivate::CPromiseReceiveAnyUnwrapFunctor
	{
		return NPrivate::CPromiseReceiveAnyUnwrapFunctor{*this};
	}

	template <>
	auto TCPromise<void>::f_ReceiveAny() && -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>
	{
		return NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>{fg_Move(*this)};
	}

	template <>
	auto TCPromise<void>::f_ReceiveAnyUnwrap() && -> NPrivate::CPromiseReceiveAnyUnwrapFunctor
	{
		return NPrivate::CPromiseReceiveAnyUnwrapFunctor{fg_Move(*this)};
	}

	constexpr CDispatchHelper g_DispatchInit{};
	CDispatchHelper const &g_Dispatch = g_DispatchInit;

	constexpr CConcurrentDispatchHelper g_ConcurrentDispatchInit{};
	CConcurrentDispatchHelper const &g_ConcurrentDispatch = g_ConcurrentDispatchInit;

	constexpr CDirectDispatchHelper g_DirectDispatchInit{};
	CDirectDispatchHelper const &g_DirectDispatch = g_DirectDispatchInit;

	CCoroutineTransferOwnership fg_ContinueRunningOnActor(TCActor<> const &_Actor)
	{
		return CCoroutineTransferOwnership(fg_TempCopy(_Actor));
	}

	CCoroutineTransferOwnership fg_ContinueRunningOnActor(CBlockingActorCheckout const &_Checkout)
	{
		return CCoroutineTransferOwnership(fg_TempCopy(_Checkout.f_Actor()));
	}

	CReportLocalState::CReportLocalState() = default;
	CReportLocalState::~CReportLocalState() = default;

	void CReportLocalState::f_StoreCallStates(CSystemThreadLocal &_ThreadLocal)
	{
		for (auto &Scope : _ThreadLocal.m_CrossActorStateScopes)
		{
			auto State = Scope.f_StoreState(false);
			if (State)
				m_RestoreStates.f_Insert(fg_Move(State));
		}
	}

	NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> CReportLocalState::f_RestoreCallStates()
	{
		NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> Return = fg_Move(m_RestoreStates);

		for (auto &fRestoreState : Return)
			fRestoreState();

		return Return;
	}

#if DMibConfig_Concurrency_DebugActorCallstacks
	void CReportLocalState::f_CaptureCallstack()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (ThreadLocal.m_pCallstacks)
			m_Callstacks = *ThreadLocal.m_pCallstacks;

		auto &Callstack = m_Callstacks.f_InsertFirst();
		Callstack.f_Capture();
#	ifdef DMibDebug
		mint nFramesToRemove = 6; // fg_System_GetStackTrace + f_Capture + Constructor + f_Call + operator > + f_CaptureCallstack()
#		ifdef DCompiler_clang
			nFramesToRemove += 1; // Two frames for constructor
#		endif
#	else
		mint nFramesToRemove = 0; // We don't know what inlining will do
#	endif
		if (Callstack.m_CallstackLen > nFramesToRemove)
		{
			Callstack.m_CallstackLen -= nFramesToRemove;
			NMemory::fg_MemMove(Callstack.m_Callstack, Callstack.m_Callstack + nFramesToRemove, sizeof(Callstack.m_Callstack) - sizeof(Callstack.m_Callstack[0]) * nFramesToRemove);
		}

		if (m_Callstacks.f_GetLen() > 16)
			m_Callstacks.f_Remove(m_Callstacks.f_GetLast());
	}
#endif

	constinit CCoroutineYield g_Yield;
	constinit CAccessCoroutine g_AccessCoroutine;

	constinit CIsGeneratorAborted g_bShouldAbort;
	constinit CGetGeneratorPipelineLength g_GetPipelineLength;

	constinit CDiscardResult g_DiscardResult;
	constinit CDirectResult g_DirectResult;
}

namespace NMib::NConcurrency
{
	struct CShamActorHolder;

	CConcurrentActorImpl::~CConcurrentActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CConcurrentActorLowPrioImpl::~CConcurrentActorLowPrioImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CConcurrentActorHighCPUImpl::~CConcurrentActorHighCPUImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CThisConcurrentActorImpl::~CThisConcurrentActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CThisConcurrentActorLowPrioImpl::~CThisConcurrentActorLowPrioImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CThisConcurrentActorHighCPUImpl::~CThisConcurrentActorHighCPUImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	COtherConcurrentActorImpl::~COtherConcurrentActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	COtherConcurrentActorLowPrioImpl::~COtherConcurrentActorLowPrioImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	COtherConcurrentActorHighCPUImpl::~COtherConcurrentActorHighCPUImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CDynamicConcurrentActorImpl::~CDynamicConcurrentActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CDynamicConcurrentActorLowPrioImpl::~CDynamicConcurrentActorLowPrioImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CDynamicConcurrentActorHighCPUImpl::~CDynamicConcurrentActorHighCPUImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	CDirectCallActorImpl::~CDirectCallActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}
}
