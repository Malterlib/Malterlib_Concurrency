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
					, NContainer::CByteVector const &_CertificateRequest
				)
			>
		;
		
		CDistributedAppInProcessActor
			(
				NWeb::NHTTP::CURL const &_Address
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
				, FOnUseTicket &&_fOnUseTicket
				, NStr::CStr const &_Description
				, bool _bDelegateTrust
			)
		;
		~CDistributedAppInProcessActor();

		TCContinuation<NStr::CStr> f_Launch(NStr::CStr const &_HomeDirectory, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory);

#if DMibConfig_Tests_Enable
		TCContinuation<NEncoding::CEJSON> f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
		TCContinuation<uint32> f_RunCommandLine
			(
				CCallingHostInfo const &_CallingHost
				, NStr::CStr const &_Command
				, NEncoding::CEJSON const &_Params
				, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			)
		;
#endif

	private:
		TCContinuation<CDistributedActorTrustManager::CTrustTicket> fp_HandleTicketRequest();

		TCContinuation<void> fp_Destroy();

		struct CHandleRequest
		{
			CActorSubscription m_NotificationsSubscription;
		};
		
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		NWeb::NHTTP::CURL mp_Address;
		NStr::CStr mp_Description;
		FOnUseTicket mp_fOnUseTicket;
		NContainer::TCMap<NStr::CStr, CHandleRequest> mp_HandleRequests;
		TCActor<CDistributedAppActor> mp_DistributedApp;
		bool mp_bDelegateTrust = false;
	};
}
