// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"

namespace NMib::NConcurrency
{
	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &_Futures)
	{
		TCPromise<void> Promise{CPromiseConstructNoConsume()};
		fg_AllDoneWrapped(_Futures).f_OnResultSet
			(
				[Promise](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &&_Futures)
	{
		return fg_AllDone(_Futures);
	}

	TCFuture<void> fg_AllDone(TCFutureVector<void> &_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	TCFuture<void> fg_AllDone(TCFutureVector<void> &&_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}
}
