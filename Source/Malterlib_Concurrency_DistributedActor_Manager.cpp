// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include <Mib/Concurrency/Actor/Timer>

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
			m_HostLink.f_Unlink();
			m_ConnectionSubscription.f_Clear();
			if (m_Connection)
			{
				m_Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, "Connection reset", 5.0)
					> fg_ConcurrentActor() / [Connection = m_Connection](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result)
					{
						if (_Result)
						{
							if (_Result->m_Status != NWeb::EWebSocketStatus_AlreadyClosed)
								DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Closed connection: {} - {}", _Result->m_Status, _Result->m_Reason);
						}
						else
							DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Closed connection with error: {}", _Result.f_GetExceptionStr());
					}
				;
				m_Connection.f_Clear();
			}
			
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
