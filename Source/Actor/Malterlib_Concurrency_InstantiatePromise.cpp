// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_Manager.h"

namespace NMib::NConcurrency
{
	template struct TCPromise<void>;
	template struct TCPromise<NContainer::TCVector<TCAsyncResult<void>>>;
	template struct TCFutureVector<void>;

	// Only the constructor and destructor: CActorSubscription can't be copied, so the members that copy the result can't be instantiated
	template TCPromise<CActorSubscription>::TCPromise(CConcurrencyThreadLocal &_ThreadLocal);
	template TCPromise<CActorSubscription>::~TCPromise();
}
namespace NMib::NConcurrency::NPrivate
{
	template TCPromiseData<CActorSubscription>::TCPromiseData
		(
			CPromiseThreadLocal &_ThreadLocal
			, int32 _RefCount
#if DMibEnableSafeCheck > 0
			, EConsumeFutureOnResultSetFlag _DebugFlags
#endif
		)
	;
	template TCPromiseData<CActorSubscription>::~TCPromiseData();
}
