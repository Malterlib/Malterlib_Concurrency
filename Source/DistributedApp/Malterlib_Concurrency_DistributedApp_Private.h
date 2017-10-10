// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

