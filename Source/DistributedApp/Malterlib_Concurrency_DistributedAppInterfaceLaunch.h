// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
				TCFuture<void>
				(
					NStr::CStr _HostID
					, CCallingHostInfo _HostInfo
					, NContainer::CByteVector _CertificateRequest
				)
			>
		;

		CDistributedAppInterfaceLaunchActor
			(
				NWeb::NHTTP::CURL const &_Address
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
				, FOnUseTicket &&_fOnUseTicket
				, TCActorFunctor<TCFuture<void> (NStr::CStr _Error)> &&_fOnLaunchError
				, NStr::CStr const &_Description
				, NStr::CStr const &_LaunchID
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
		NWeb::NHTTP::CURL mp_Address;
		NStr::CStrSecure mp_RequestTicketMagic;
		NStr::CStr mp_Description;
		NStr::CStr mp_LaunchID;
		FOnUseTicket mp_fOnUseTicket;
		TCActorFunctor<TCFuture<void> (NStr::CStr _Error)> mp_fOnLaunchError;
		NContainer::TCMap<NStr::CStr, CHandleRequest> mp_HandleRequests;
		bool mp_bDelegateTrust = false;
	};
}
