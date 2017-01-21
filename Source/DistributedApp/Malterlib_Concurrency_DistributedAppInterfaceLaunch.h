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
				, NStr::CStr const &_Description
			)
		;
		~CDistributedAppInterfaceLaunchActor();

	private:
		void f_FilterOutput(NProcess::EProcessLaunchOutputType _OutputType, NStr::CStr &o_Output) override;
		void f_ModifyLaunch(CLaunch &o_Launch) override;

		void fp_HandleTicketRequest();
		
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		NHTTP::CURL mp_Address;
		NStr::CStr mp_RequestTicketMagic;
		NStr::CStr mp_RequestTicketMagicLine;
		NStr::CStr mp_Description;
		FOnUseTicket mp_fOnUseTicket;
	};
}
