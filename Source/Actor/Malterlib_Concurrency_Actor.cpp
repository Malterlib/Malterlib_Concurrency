// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	CActor::CActor()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		DMibCheck(ThreadLocal.m_pCurrentlyConstructingActor == this); // You can only construct actors through concurrency manager
		
		ThreadLocal.m_pCurrentlyProcessingActorHolder->mp_pActor = fg_Explicit(this); 
		self.m_pThis = ThreadLocal.m_pCurrentlyProcessingActorHolder;
		mp_pConcurrencyManager = &ThreadLocal.m_pCurrentlyProcessingActorHolder->f_ConcurrencyManager();
		ThreadLocal.m_pCurrentActor = this;
	}

	CActor::~CActor()
	{
	}

	bool CActor::f_IsDestroyed() const
	{
		return mp_bDestroyed;
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
		if (mp_bDestroyed)
			return NException::fg_ExceptionPointer(DMibImpExceptionInstance(CExceptionActorIsBeingDestroyed, "Actor is being destroyed"));
		return nullptr;
	}

	TCContinuation<void> CActor::fp_DestroyInternal()
	{
		if (mp_bDestroyed)
			return DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor has already been destroyed");

		mp_bDestroyed = true;
		
		return fp_Destroy();
	}
	
	TCContinuation<void> CActor::fp_Destroy()
	{
		return TCContinuation<void>::fs_Finished();
	}
	
	NPrivate::CThisActor::operator TCActor<> () const
	{
		TCActorInternal<CActor> *pActor = (TCActorInternal<CActor> *)m_pThis;
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCActor<>{fg_Explicit(pActor)};
	}
	
	CCanDestroyTracker::CCanDestroyTracker() = default;
		
	CCanDestroyTracker::~CCanDestroyTracker()
	{
		if (!m_Continuation.f_IsSet())
			m_Continuation.f_SetResult();
	}
	
	CCanDestroyTracker::CCanDestroyResultFunctor CCanDestroyTracker::f_Track()
	{
		return CCanDestroyResultFunctor{fg_Explicit(this)};
	}
		
	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(CCanDestroyResultFunctor &&) = default;
	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(CCanDestroyResultFunctor const &) = default;
	
	CCanDestroyTracker::CCanDestroyResultFunctor::CCanDestroyResultFunctor(NPtr::TCSharedPointer<CCanDestroyTracker> &&_pThis)
		: m_pThis(fg_Move(_pThis))
	{
	}
	
	void CCanDestroyTracker::CCanDestroyResultFunctor::operator () (TCAsyncResult<void> &&)
	{
	}
	
	template <>
	auto TCContinuation<void>::f_ReceiveAny() const -> NPrivate::TCContinuationReceiveAnyFunctor<TCContinuation<void>>
	{
		return NPrivate::TCContinuationReceiveAnyFunctor<TCContinuation<void>>{*this};
	}

	constexpr CDispatchHelper g_DispatchInit{};
	CDispatchHelper const &g_Dispatch = g_DispatchInit;
	
	constexpr CConcurrentDispatchHelper g_ConcurrentDispatchInit{};
	CConcurrentDispatchHelper const &g_ConcurrentDispatch = g_ConcurrentDispatchInit;

	constexpr CDirectDispatchHelper g_DirectDispatchInit{};
	CDirectDispatchHelper const &g_DirectDispatch = g_DirectDispatchInit;
}
