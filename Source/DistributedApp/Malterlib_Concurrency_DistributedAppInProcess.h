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
				TCFuture<void>
				(
					NStr::CStr _HostID
					, CCallingHostInfo _HostInfo
					, NContainer::CByteVector _CertificateRequest
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

		TCFuture<NStr::CStr> f_Launch
			(
				NStr::CStr _HomeDirectory
				, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> _fDistributedAppFactory
				, NContainer::TCVector<NStr::CStr> _Params = {}
			)
		;

#if DMibConfig_Tests_Enable
		TCFuture<NEncoding::CEJSONSorted> f_Test_Command(NStr::CStr _Command, NEncoding::CEJSONSorted const _Params);
		TCFuture<uint32> f_RunCommandLine
			(
				CCallingHostInfo _CallingHost
				, NStr::CStr _Command
				, NEncoding::CEJSONSorted _Params
				, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
			)
		;
#endif

	private:
		TCFuture<CDistributedActorTrustManager::CTrustTicket> fp_HandleTicketRequest();

		TCFuture<void> fp_Destroy();

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
