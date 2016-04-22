// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib
{
	namespace NConcurrency
	{
		CActorDistributionManager::CActorDistributionManager()
			: mp_pInternal(fg_Construct(this))
		{
		}

		CActorDistributionManager::~CActorDistributionManager()
		{
			auto &Internal = *mp_pInternal;
			
			for (auto &pConnection : Internal.m_ClientConnections)
			{
				pConnection->m_ConnectionSubscription.f_Clear();
				pConnection->m_Connection.f_Clear();
				pConnection->m_pSSLContext.f_Clear();
				++pConnection->m_ConnectionSequence;
			}
			for (auto &pConnection : Internal.m_ServerConnections)
			{
				pConnection->m_ConnectionSubscription.f_Clear();
				pConnection->m_Connection.f_Clear();
				pConnection->m_pSSLContext.f_Clear();
			}
		}
		
		CActorDistributionManager::CInternal::CHost::~CHost()
		{
			m_Incoming_ReceivedPackets.f_DeleteAll();
			m_Incoming_QueuedPackets.f_DeleteAll();
			m_Outgoing_QueuedPackets.f_DeleteAll();
			m_Outgoing_SentPackets.f_DeleteAll();
		}
		
		NStr::CStr const &CActorDistributionManager::CInternal::CHost::f_GetHostID() const
		{
			return NContainer::TCMap<NStr::CStr, CHost>::fs_GetKey(*this);
		}
	}
}
