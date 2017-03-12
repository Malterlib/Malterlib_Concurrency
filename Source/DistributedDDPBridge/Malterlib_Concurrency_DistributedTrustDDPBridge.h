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
			NFunction::TCFunctionMutable<TCContinuation<NEncoding::CEJSON> (NContainer::TCVector<NEncoding::CEJSON> const &_Params)> m_fHandler;
		};
		
		TCContinuation<CActorSubscription> f_RegisterMethods
			(
				TCWeakActor<> const &_Actor
				, NContainer::TCVector<CMethod> &&_Methods
			)
		;

		TCContinuation<void> f_Startup();
		
	private:
		struct CInternal;
		
		NPtr::TCUniquePointer<CInternal> mp_pInternal;
	};
}
