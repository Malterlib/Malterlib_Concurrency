// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"

namespace NMib::NConcurrency
{
	TCFuture<void> fg_AllDone(TCFutureVector<void> &_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	TCFuture<void> fg_AllDone(TCFutureVector<void> &&_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}
}
