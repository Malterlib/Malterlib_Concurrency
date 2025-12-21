// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CFirst>
	void fg_GetDistributedActorInheritanceHierarchy(NContainer::TCVector<uint32> &_Settings)
	{
		_Settings.f_Insert(DMibConstantTypeHash(tf_CFirst));
		fg_GetDistributedActorInheritanceHierarchy<NTraits::TCGetBase<tf_CFirst>>(_Settings);
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
			CCallingHostInfoScope *m_pCurrentCallingHostInfoScope = nullptr;
			CAuthenticationHandlerIDScope *m_pCurrentAuthenticationHandlerIDScope = nullptr;
			CPriorityScope *m_pCurrentPriorityScope = nullptr;
			uint32 m_CurrentAuthenticationHandlerID = 0;
			uint8 m_CurrentPriority = 128;
		};

		NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
	};

	CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem();
}

#include "Malterlib_Concurrency_DistributedActor_Stream_Private.hpp"
