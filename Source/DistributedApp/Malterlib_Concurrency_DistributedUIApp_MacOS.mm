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
		TCFuture<NContainer::CIOByteVector> f_Test(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CIOByteVector, NPrivate::CDistributedActorStreamContext);
	};

	void CDistributedUIApp::fsp_RunMain()
	{
		CTesting Testing;
		NPrivate::CDistributedActorStreamContext Temp (5, true);
		(void)Testing.f_Test(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>(), NContainer::CIOByteVector(), Temp);


		static_assert
		(
			NTraits::TCIsCallableWith
			<
				TCFuture<NContainer::CIOByteVector> (*)(NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CIOByteVector, NPrivate::CDistributedActorStreamContext)
				, void (NStorage::TCSharedPointer<NPrivate::CDistributedActorData>, NContainer::CIOByteVector, NPrivate::CDistributedActorStreamContext)
			>::mc_Value
		);


		ch8	const *Dummy[] = {nullptr};
		NSApplicationMain(0, Dummy); // Will never return
	}
}
