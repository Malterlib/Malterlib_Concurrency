// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	extern template struct TCPromise<void>;
	extern template struct TCPromise<NContainer::TCVector<TCAsyncResult<void>>>;
	extern template struct TCFutureVector<void>;

	// Only the constructor and destructor: CActorSubscription can't be copied, so the members that copy the result can't be instantiated
	extern template TCPromise<CActorSubscription>::TCPromise(CConcurrencyThreadLocal &_ThreadLocal);
	extern template TCPromise<CActorSubscription>::~TCPromise();
}
namespace NMib::NConcurrency::NPrivate
{
	extern template TCPromiseData<CActorSubscription>::TCPromiseData
		(
			CPromiseThreadLocal &_ThreadLocal
			, int32 _RefCount
#if DMibEnableSafeCheck > 0
			, EConsumeFutureOnResultSetFlag _DebugFlags
#endif
		)
	;
	extern template TCPromiseData<CActorSubscription>::~TCPromiseData();
}
