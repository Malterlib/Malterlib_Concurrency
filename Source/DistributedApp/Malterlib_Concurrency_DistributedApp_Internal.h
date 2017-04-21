// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CDistributedAppActor::CDistributedAppInterfaceClientImplementation : public CDistributedAppInterfaceClient 
	{
		NConcurrency::TCContinuation<void> f_GetAppStartResult() override;
		NConcurrency::TCContinuation<void> f_PreUpdate() override;
		NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_StartBackup
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> &&_BackupInterface
			) override
		;
		
		CDistributedAppActor *m_pThis;
	};
}
