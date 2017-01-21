// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"

namespace NMib::NConcurrency
{
	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address() = default;
	CDistributedActorTrustManager_Address::~CDistributedActorTrustManager_Address() = default;
	
	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address(NHTTP::CURL const &_URL)
		: m_URL{_URL}
	{
	}
	
	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address(NHTTP::CURL &&_URL)
		: m_URL{fg_Move(_URL)}
	{
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
