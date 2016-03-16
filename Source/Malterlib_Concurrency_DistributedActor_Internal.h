// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Web/WebSocket>
#include <Mib/Network/SSL>

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CActorDistributionManager::CInternal
		{
			CInternal(CActorDistributionManager *_pThis)
				: m_pThis(_pThis)
			{
			}
			TCActor<NWeb::CWebSocketServerActor> m_WebsocketServer;
			CActorCallback m_ListenCallbackSubscription;

			TCActor<NWeb::CWebSocketClientActor> m_WebsocketClientConnector;
			
			struct CConnection
			{
				virtual ~CConnection()
				{
				}
				
				TCActor<NWeb::CWebSocketActor> m_Connection;
				CActorCallback m_ConnectionSubscription;
				NPtr::TCSharedPointer<NNet::CSSLContext> m_pSSLContext;
			};

			struct CClientConnection : public CConnection 
			{
				NHTTP::CURL m_ServerURL;
				mint m_ConnectionSequence = 0;
			};

			struct CServerConnection : public CConnection 
			{
			};
			
			NContainer::TCLinkedList<NPtr::TCSharedPointer<CClientConnection>> m_ClientConnections;
			NContainer::TCLinkedList<NPtr::TCSharedPointer<CServerConnection>> m_ServerConnections;
			
			CActorDistributionManager *m_pThis;
			void fp_ScheduleReconnect(NPtr::TCSharedPointer<CClientConnection> const &_pConnection, TCContinuation<void> &_Continuation, bool _bRetry, mint _Sequence);
			void fp_Reconnect(NPtr::TCSharedPointer<CClientConnection> const &_pConnection, TCContinuation<void> &_Continuation, bool _bRetry);
			void fp_ServerConnectionClosed(NPtr::TCSharedPointer<CServerConnection> const &_pConnection);
			void fp_Listen(CActorDistributionListenSettings const &_Settings, TCContinuation<void> &_Continuation);
		};
	}
}
