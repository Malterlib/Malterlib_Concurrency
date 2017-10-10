// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CFirst>
	void fg_GetDistributedActorInheritanceHierarchy(NContainer::TCVector<uint32> &_Settings)
	{
		_Settings.f_Insert(fg_GetTypeHash<tf_CFirst>());
		fg_GetDistributedActorInheritanceHierarchy<typename NTraits::TCGetBase<tf_CFirst>::CType>(_Settings);
	}
	
	template <>
	inline_always_debug void fg_GetDistributedActorInheritanceHierarchy<CActor>(NContainer::TCVector<uint32> &_Settings)
	{
	}

	struct CSubSystem_Concurrency_DistributedActor : public CSubSystem
	{
		CSubSystem_Concurrency_DistributedActor();
		~CSubSystem_Concurrency_DistributedActor();

		struct CThreadLocal
		{
			CCallingHostInfo m_CallingHostInfo;
			CDistributedActorData *m_pCurrentRemoteDispatchActorData = nullptr;
		};
		
		NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
	};
	
	CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem();
}

#include "Malterlib_Concurrency_DistributedActor_Stream_Private.hpp"
