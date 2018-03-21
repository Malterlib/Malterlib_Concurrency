// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Process/ProcessLaunchActor>

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceLaunchActor : public NProcess::CProcessLaunchActor
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

		CDistributedAppInterfaceLaunchActor
			(
				NHTTP::CURL const &_Address
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
				, FOnUseTicket &&_fOnUseTicket
			 	, TCActorFunctor<TCContinuation<void> (NStr::CStr const &_Error)> &&_fOnLaunchError
				, NStr::CStr const &_Description
				, bool _bDelegateTrust 
			)
		;
		~CDistributedAppInterfaceLaunchActor();

	private:
		bool fp_WillFilterOutput() override;
		void fp_FilterOutput(NProcess::EProcessLaunchOutputType _OutputType, NStr::CStr &o_Output) override;
		void fp_ModifyLaunch(CLaunch &o_Launch) override;

		void fp_HandleTicketRequest();

		struct CHandleRequest
		{
			CActorSubscription m_NotificationsSubscription;
		};
		
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		NHTTP::CURL mp_Address;
		NStr::CStrSecure mp_RequestTicketMagic;
		NStr::CStr mp_Description;
		FOnUseTicket mp_fOnUseTicket;
		TCActorFunctor<TCContinuation<void> (NStr::CStr const &_Error)> mp_fOnLaunchError;
		NContainer::TCMap<NStr::CStr, CHandleRequest> mp_HandleRequests;
		bool mp_bDelegateTrust = false;
	};
}
