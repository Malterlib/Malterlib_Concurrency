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
		
		NStr::CStr CActorDistributionManager::fs_GetCertificateHostID(NContainer::TCVector<uint8> const &_Certificate)
		{
			auto Extensions = NNet::CSSLContext::fs_GetCertificateExtensions(_Certificate);
			
			auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
			if (pHostIDExtension && pHostIDExtension->f_GetLen() == 1)
				return (*pHostIDExtension)[0].m_Value;
			
			return fg_Default();
		}
		
		namespace NActorDistributionManagerInternal
		{
			CHost::~CHost()
			{
				f_Clear(nullptr);
			}
			void CHost::f_Clear(CActorDistributionManager::CInternal *_pInternal)
			{
				m_bDeleted = true;
				m_ActiveConnections.f_Clear();
				for (auto Call : m_OutstandingCalls)
					Call.f_SetException(DMibErrorInstance("Remote host no longer running"));
				m_OutstandingCalls.f_Clear();
				for (auto &RemoteActor : m_RemoteActors)
				{
					if (RemoteActor.m_Link.f_IsInList())
					{
						DMibFastCheck(_pInternal);
						auto pRemoteNamespace = _pInternal->m_RemoteNamespaces.f_FindEqual(RemoteActor.m_Namespace);
						DMibFastCheck(pRemoteNamespace);
						pRemoteNamespace->m_RemoteActors.f_Remove(RemoteActor);
						if (pRemoteNamespace->m_RemoteActors.f_IsEmpty())
							_pInternal->m_RemoteNamespaces.f_Remove(pRemoteNamespace);
					}
				}
				m_RemoteActors.f_Clear();
				m_AllowedNamespaces.f_Clear();
				m_Incoming_ReceivedPackets.f_DeleteAll();
				m_Incoming_QueuedPackets.f_DeleteAll();
				m_Outgoing_QueuedPackets.f_DeleteAll();
				m_Outgoing_SentPackets.f_DeleteAll();
			}
		}
	}
}
