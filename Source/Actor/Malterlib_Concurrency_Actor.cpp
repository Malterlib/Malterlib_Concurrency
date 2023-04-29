// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/AsyncGenerator>

namespace NMib::NConcurrency
{
	CCurrentlyProcessingActorScope::CCurrentlyProcessingActorScope(CActor const *_pActor)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pLastActor = ThreadLocal.m_pCurrentActor;
		ThreadLocal.m_pCurrentActor = const_cast<CActor *>(_pActor);
#if DMibEnableSafeCheck > 0
		mp_pLastOverriddenProcessingActorHolder = ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder;
		ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder = (CActorHolder *)_pActor->self.m_pThis;
#endif
	}

	CCurrentlyProcessingActorScope::CCurrentlyProcessingActorScope(TCActor<CActor> const &_Actor)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pLastActor = ThreadLocal.m_pCurrentActor;
		ThreadLocal.m_pCurrentActor = NPrivate::fg_GetInternalActor(_Actor);
#if DMibEnableSafeCheck > 0
		mp_pLastOverriddenProcessingActorHolder = ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder;
		ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder = (CActorHolder *)ThreadLocal.m_pCurrentActor->self.m_pThis;
#endif
	}

	CCurrentlyProcessingActorScope::~CCurrentlyProcessingActorScope()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		ThreadLocal.m_pCurrentActor = mp_pLastActor;
#if DMibEnableSafeCheck > 0
		ThreadLocal.m_pCurrentlyOverridenProcessingActorHolder = mp_pLastOverriddenProcessingActorHolder;
#endif
	}

	CActor::CActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		DMibFastCheck(ThreadLocal.m_pCurrentlyConstructingActor == this); // You can only construct actors through concurrency manager

		ThreadLocal.m_pCurrentlyProcessingActorHolder->mp_pActorUnsafe.f_Store(this, NAtomic::EMemoryOrder_Relaxed);
		self.m_pThis = ThreadLocal.m_pCurrentlyProcessingActorHolder;
		ThreadLocal.m_pCurrentActor = this;
	}

	CActor::~CActor()
	{
		DMibFastCheck(mp_SuspendedCoroutines.f_IsEmpty());
	}

#if DMibEnableSafeCheck > 0
	CActorInternal::CActorInternal()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pThis = ThreadLocal.m_pCurrentActor;
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

	void CActor::fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach)
	{
		_fToDisptach();
	}

	void CActor::f_Dispatch(NFunction::TCFunctionMovable<void ()> &&_fToDisptach)
	{
		_fToDisptach();
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

	CFutureCoroutineContext::COnResumeScopeAwaiter CActor::f_CheckDestroyedOnResume()
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
			return TCPromise<void>() <<= DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed");

		self.m_pThis.f_SetBits(1);

		return fp_Destroy();
	}

	TCFuture<void> CActor::fp_Destroy()
	{
		return TCPromise<void>() <<= g_Void;
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

	CReportLocalState::CReportLocalState() = default;
	CReportLocalState::~CReportLocalState() = default;

	void CReportLocalState::f_StoreCallStates()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		for (auto &Scope : ThreadLocal.m_CrossActorStateScopes)
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

	constinit CCoroutineYield g_Yield;

	constinit CIsGeneratorAborted g_bShouldAbort;
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

	NPrivate::CDirectResultActorImpl::~CDirectResultActorImpl()
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

	COtherConcurrentActorImpl::~COtherConcurrentActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}

	COtherConcurrentActorLowPrioImpl::~COtherConcurrentActorLowPrioImpl()
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

	CDirectCallActorImpl::~CDirectCallActorImpl()
	{
		DMibFastCheck(f_ConcurrencyManager().f_DestroyingAlwaysAliveActors());
	}
}
