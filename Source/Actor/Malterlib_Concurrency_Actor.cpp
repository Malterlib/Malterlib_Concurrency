// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{
		CActor::CActor()
		{
		}

		CActor::~CActor()
		{
		}

		void CActor::fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach)
		{
			_fToDisptach();
		}
	
		void CActor::f_Dispatch(NFunction::TCFunctionMovable<void ()> &&_fToDisptach)
		{
			_fToDisptach();
		}
	
		void CActor::f_Construct()
		{
		}
		
		TCContinuation<void> CActor::f_Destroy()
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
	}
}
