// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	struct CLocalHostIdentity
	{
		NStr::CStr f_UserAtComputer() const;

		NStr::CStr m_UserName;
		NStr::CStr m_ComputerName;
	};

	void fg_PrefetchLocalHostIdentity();
	void fg_PrefetchLocalHostIdentity(NStorage::TCSharedPointer<CBlockingActorCheckout> const &_pBlockingActorCheckout);

	TCFuture<CLocalHostIdentity> fg_GetLocalHostIdentity();

	CLocalHostIdentity fg_GetLocalHostIdentityNow();
}
