// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CDistributedAppActor::CDistributedAppInterfaceClientImplementation : public CDistributedAppInterfaceClient 
	{
		NConcurrency::TCFuture<void> f_GetAppStartResult() override;
		NConcurrency::TCFuture<void> f_PreUpdate() override;
		NConcurrency::TCFuture<void> f_PreStop() override;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_StartBackup
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> &&_BackupInterface
				, NConcurrency::CActorSubscription &&_ManifestFinished
				, NStr::CStr const &_BackupRoot
			) override
		;
		
		CDistributedAppActor *m_pThis;
#		ifdef DMibDebug
			CEmpty self; // Hide dangerous self
#		endif
	};
}
