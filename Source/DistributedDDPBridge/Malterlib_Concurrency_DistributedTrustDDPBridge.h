// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Encoding/EJson>

namespace NMib::NConcurrency
{
	struct CDistributedTrustDDPBridge : public CActor
	{
		CDistributedTrustDDPBridge(TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CDistributedTrustDDPBridge();

		struct CMethod
		{
			NStr::CStr m_Name;
			TCActorFunctor<TCFuture<NEncoding::CEJsonSorted> (NContainer::TCVector<NEncoding::CEJsonSorted> _Params)> m_fHandler;
		};

		TCFuture<CActorSubscription> f_RegisterMethods(NContainer::TCVector<CMethod> _Methods);

		TCFuture<void> f_Startup();

	private:
		struct CInternal;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
