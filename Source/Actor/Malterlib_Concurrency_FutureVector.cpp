// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
