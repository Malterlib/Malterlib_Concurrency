// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Log/Log>
#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogDestination_Internal;
}

DMibDefineSharedPointerType(NMib::NConcurrency::CDistributedAppLogDestination_Internal, false, false);

namespace NMib::NConcurrency
{
	struct CDistributedAppLogDestination
	{
		CDistributedAppLogDestination(TCActor<CDistributedAppActor> const &_DistributedLoggingApp, TCActor<CDistiributedAppLogActor> const &_LogActor);
		CDistributedAppLogDestination(CDistributedAppLogDestination &&_Other);
		CDistributedAppLogDestination(CDistributedAppLogDestination const &_Other);
		~CDistributedAppLogDestination();

		void operator()
			(
				umint _ThreadID
				, NTime::CTime const &_Time
				, NLog::ESeverity _Sev
				, NLog::CLogStr const &_Message
				, NContainer::TCVector<NStr::CStr> const &_Categories
				, NContainer::TCVector<NStr::CStr> const &_Operations
				, NLog::CLogLocationTag const& _Loc
			)
		;

	private:
		void fp_InitLogging(TCActor<CDistributedAppActor> const &_DistributedLoggingApp);

		NStorage::TCSharedPointerSupportWeak<CDistributedAppLogDestination_Internal> mp_pInternal;
	};
}

