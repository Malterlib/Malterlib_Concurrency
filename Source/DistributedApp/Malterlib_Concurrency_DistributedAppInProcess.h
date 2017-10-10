// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	struct CDistributedAppInProcessActor : public CActor
	{
		using FOnUseTicket = TCActorFunctor
			<
				TCContinuation<void> 
				(
					NStr::CStr const &_HostID
					, CCallingHostInfo const &_HostInfo
					, NContainer::TCVector<uint8> const &_CertificateRequest
				)
			>
		;
		
		CDistributedAppInProcessActor
			(
				NHTTP::CURL const &_Address
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
				, FOnUseTicket &&_fOnUseTicket
				, NStr::CStr const &_Description
				, bool _bDelegateTrust
			)
		;
		~CDistributedAppInProcessActor();

		TCContinuation<void> f_Launch(NStr::CStr const &_HomeDirectory, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory);

	private:
		TCContinuation<CDistributedActorTrustManager::CTrustTicket> fp_HandleTicketRequest();

		TCContinuation<void> fp_Destroy();

		struct CHandleRequest
		{
			CActorSubscription m_OnUseTicketSubscription;
		};
		
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		NHTTP::CURL mp_Address;
		NStr::CStr mp_Description;
		FOnUseTicket mp_fOnUseTicket;
		NContainer::TCMap<NStr::CStr, CHandleRequest> mp_HandleRequests;
		TCActor<CDistributedAppActor> mp_DistributedApp;
		bool mp_bDelegateTrust = false;
	};
}
