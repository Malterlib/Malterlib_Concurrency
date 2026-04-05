// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_Manager.h"

namespace NMib::NConcurrency
{
	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &_Futures)
	{
		TCPromiseFuturePair<void> Promise;
		fg_AllDoneWrapped(_Futures).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &&_Futures)
	{
		return fg_AllDone(_Futures);
	}
}
