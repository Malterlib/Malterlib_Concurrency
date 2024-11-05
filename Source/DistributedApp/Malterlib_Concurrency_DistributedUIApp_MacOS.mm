// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedUIApp.h"

#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Concurrency/OSMainRunLoop>
#include <Mib/Concurrency/ActorHolderOSMainRunLoop>

#include <AppKit/NSApplication.h>

namespace NMib::NConcurrency
{
	struct CTesting
	{
		TCFuture<NContainer::CSecureByteVector> f_Test(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CSecureByteVector, NPrivate::CDistributedActorStreamContext);
	};

	void CDistributedUIApp::fsp_RunMain()
	{
		CTesting Testing;
		NPrivate::CDistributedActorStreamContext Temp (5, true);
		(void)Testing.f_Test(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>(), NContainer::CSecureByteVector(), Temp);


		static_assert
		(
			NTraits::TCIsCallableWith
			<
				TCFuture<NContainer::CSecureByteVector> (*)(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CSecureByteVector, NPrivate::CDistributedActorStreamContext)
				, void (NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CSecureByteVector, NPrivate::CDistributedActorStreamContext)
			>::mc_Value
		);


		ch8	const *Dummy[] = {nullptr};
		NSApplicationMain(0, Dummy); // Will never return
	}
}
