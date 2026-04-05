// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CSubSystem_Concurrency_DistributedApp : public CSubSystem
	{
		CSubSystem_Concurrency_DistributedApp();
		~CSubSystem_Concurrency_DistributedApp();

		NThread::TCThreadLocal<CDistributedAppThreadLocal> m_ThreadLocal;
	};

	CSubSystem_Concurrency_DistributedApp &fg_DistributedAppSubSystem();
}

