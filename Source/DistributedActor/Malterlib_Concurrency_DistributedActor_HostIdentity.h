// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	// The user and computer names of this process, looked up once for the whole process
	struct CLocalHostIdentity
	{
		NStr::CStr f_UserAtComputer() const;

		NStr::CStr m_UserName;
		NStr::CStr m_ComputerName;
	};

	// Starts the lookup on a blocking actor unless it has already started. Called as early as a
	// process that will need the names can manage, so the lookup, which on some hosts loads the
	// name service libraries, overlaps the rest of the start-up on a thread nothing waits for
	void fg_PrefetchLocalHostIdentity();

	// Resolves with the names once the lookup has finished, starting it if it has not
	TCFuture<CLocalHostIdentity> fg_GetLocalHostIdentity();

	// The names now: the looked up ones when the lookup has finished, otherwise looked up on the
	// calling thread
	CLocalHostIdentity fg_GetLocalHostIdentityNow();
}
