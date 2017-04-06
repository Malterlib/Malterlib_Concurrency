// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_ActorCallbackManager.h"

namespace NMib::NConcurrency
{
	CCombinedCallbackReference::CCombinedCallbackReference() = default;
	CCombinedCallbackReference::~CCombinedCallbackReference() = default;
	
	TCContinuation<void> CCombinedCallbackReference::f_Destroy()
	{
		TCActorResultVector<void> Results;
		for (auto &Reference : m_References)
			Reference->f_Destroy() > Results.f_AddResult();
		
		m_References.f_Clear();
	
		TCContinuation<void> Continuation;
		Results.f_GetResults() > [Continuation](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
			{
				if (!fg_CombineResults(Continuation, fg_Move(_Results)))
					return;
				Continuation.f_SetResult();
			}
		;
		
		return Continuation;
	}
}
