// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Encoding/EJSON>

namespace NMib::NConcurrency
{
	struct CDistributedTrustDDPBridge : public CActor
	{
		CDistributedTrustDDPBridge(TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CDistributedTrustDDPBridge();
		
		struct CMethod
		{
			NStr::CStr m_Name;
			TCActorFunctor<TCFuture<NEncoding::CEJSONSorted> (NContainer::TCVector<NEncoding::CEJSONSorted> _Params)> m_fHandler;
		};
		
		TCFuture<CActorSubscription> f_RegisterMethods(NContainer::TCVector<CMethod> _Methods);

		TCFuture<void> f_Startup();
		
	private:
		struct CInternal;
		
		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
