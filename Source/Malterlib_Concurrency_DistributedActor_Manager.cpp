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

		void CActorDistributionManager::CInternal::CConnection::fs_LogClose(TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> const &_Result)
		{
			if (_Result)
			{
				if (_Result->m_Status != NWeb::EWebSocketStatus_AlreadyClosed)
					DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Closed connection: {} - {}", _Result->m_Status, _Result->m_Reason);
			}
			else
				DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Closed connection with error: {}", _Result.f_GetExceptionStr());
		}

		TCDispatchedActorCall<void> CActorDistributionManager::CInternal::CConnection::f_Disconnect()
		{
			return fg_ConcurrentDispatch
				(
					[Connection = fg_Move(m_Connection)]
					{
						TCContinuation<void> Continuation;
						if (!Connection)
						{
							Continuation.f_SetResult();
							return Continuation;
						}
						Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, "Connection reset", 5.0)
							> fg_ConcurrentActor() / [Connection, Continuation](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result)
							{
								fs_LogClose(_Result);
								if (_Result)
									Continuation.f_SetResult();
								else
									Continuation.f_SetException(fg_Move(_Result));
							}
						;
						return Continuation;
					}
				)
			;
		}

		void CActorDistributionManager::CInternal::CConnection::f_Reset(bool _bResetHost)
		{
			m_Link.f_Unlink();
			if (_bResetHost)
				m_HostLink.f_Unlink();
			m_ConnectionSubscription.f_Clear();
			if (m_Connection)
			{
				m_Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, "Connection reset", 5.0)
					> fg_ConcurrentActor() / [Connection = m_Connection](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result)
					{
						fs_LogClose(_Result);
					}
				;
				m_Connection.f_Clear();
			}
			
			if (m_pHost && m_pHost->m_pLastSendConnection == this)
				m_pHost->m_pLastSendConnection = nullptr;
			if (_bResetHost)
				m_pHost.f_Clear();
		}
		
		void CActorDistributionManager::CInternal::CConnection::f_Destroy(NStr::CStr const &_Error)
		{
			f_Reset(true);
			m_pSSLContext.f_Clear();
			m_LastError = _Error;
		}

		void CActorDistributionManager::CInternal::CClientConnection::f_Reset(bool _bResetHost)
		{
			CConnection::f_Reset(_bResetHost);
			m_bConnected = false;
		}

		void CActorDistributionManager::CInternal::CClientConnection::f_Destroy(NStr::CStr const &_Error)
		{
			CConnection::f_Destroy(_Error);
			m_bConnected = false;
		}

		NStr::CStr CActorDistributionManager::CInternal::CClientConnection::f_GetConnectionID() const
		{
			return m_ConnectionID;
		}

		CActorDistributionManager::CInternal::CServerConnection::CServerConnection(mint _ConnectionID)
			: m_ConnectionID(_ConnectionID)
		{
		}
		
		NStr::CStr CActorDistributionManager::CInternal::CServerConnection::f_GetConnectionID() const
		{
			return NStr::fg_Format("{}", m_ConnectionID);
		}
		
		CActorDistributionManager::~CActorDistributionManager()
		{
			auto &Internal = *mp_pInternal;
			
			for (auto &pConnection : Internal.m_ClientConnections)
			{
				++pConnection->m_ConnectionSequence;
				pConnection->f_Destroy("");
			}
			for (auto &pConnection : Internal.m_ServerConnections)
				pConnection->f_Destroy("");
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
		
		NStr::CStr CActorDistributionManager::fs_GetCertificateRequestHostID(NContainer::TCVector<uint8> const &_Certificate)
		{
			auto Extensions = NNet::CSSLContext::fs_GetCertificateRequestExtensions(_Certificate);
			
			auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
			if (pHostIDExtension && pHostIDExtension->f_GetLen() == 1)
				return (*pHostIDExtension)[0].m_Value;
			
			return fg_Default();
		}
	}
}
