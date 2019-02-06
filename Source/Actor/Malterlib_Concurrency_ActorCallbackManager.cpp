// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_ActorCallbackManager.h"

namespace NMib::NConcurrency
{
	CCombinedCallbackReference::CCombinedCallbackReference() = default;
	CCombinedCallbackReference::~CCombinedCallbackReference() = default;
	
	TCFuture<void> CCombinedCallbackReference::f_Destroy()
	{
		TCActorResultVector<void> Results;
		for (auto &Reference : m_References)
			Reference->f_Destroy() > Results.f_AddResult();
		
		m_References.f_Clear();
	
		TCPromise<void> Promise;
		Results.f_GetResults() > [Promise](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
			{
				if (!fg_CombineResults(Promise, fg_Move(_Results)))
					return;
				Promise.f_SetResult();
			}
		;
		
		return Promise.f_MoveFuture();
	}
}
