// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib
{
	namespace NConcurrency
	{
		CActorDistributionManager::CActorDistributionManager(NStr::CStr const &_HostID)
			: mp_pInternal(fg_Construct(this, _HostID))
		{
		}

		void CActorDistributionManager::CInternal::CConnection::f_Reset()
		{
			m_Link.f_Unlink();
			m_ConnectionSubscription.f_Clear();
			if (m_Connection)
				m_Connection->f_Destroy();
			m_Connection.f_Clear();
			
			if (m_pHost && m_pHost->m_pLastSendConnection == this)
				m_pHost->m_pLastSendConnection = nullptr;
		}
		
		void CActorDistributionManager::CInternal::CConnection::f_Destroy()
		{
			f_Reset();
			m_pSSLContext.f_Clear();
		}

		void CActorDistributionManager::CInternal::CClientConnection::f_Reset()
		{
			CConnection::f_Reset();
			m_bConnected = false;
		}

		void CActorDistributionManager::CInternal::CClientConnection::f_Destroy()
		{
			CConnection::f_Destroy();
			m_bConnected = false;
		}

		CActorDistributionManager::~CActorDistributionManager()
		{
			auto &Internal = *mp_pInternal;
			
			for (auto &pConnection : Internal.m_ClientConnections)
			{
				++pConnection->m_ConnectionSequence;
				pConnection->f_Destroy();
			}
			for (auto &pConnection : Internal.m_ServerConnections)
				pConnection->f_Destroy();
		}
		
		CActorDistributionManager::CInternal::CInternal(CActorDistributionManager *_pThis, NStr::CStr const &_HostID)
			: m_pThis(_pThis)
			, m_HostID(_HostID) 
			, m_ExecutionID(NCryptography::fg_RandomID())
		{
			
		}
		
		CActorDistributionManager::CInternal::~CInternal()
		{
			while (auto *pHost = m_Hosts.f_FindAny())
				fp_DestroyHost(**pHost, nullptr);
		}
		
		
		NStr::CStr CActorDistributionManager::fs_GetCertificateHostID(NContainer::TCVector<uint8> const &_Certificate)
		{
			auto Extensions = NNet::CSSLContext::fs_GetCertificateExtensions(_Certificate);
			
			auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
			if (pHostIDExtension && pHostIDExtension->f_GetLen() == 1)
				return (*pHostIDExtension)[0].m_Value;
			
			return fg_Default();
		}
	}
}
