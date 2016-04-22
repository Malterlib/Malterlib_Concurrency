// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#define DMibRuntimeTypeRegistry

#include <Mib/Web/WebSocket>
#include <Mib/Network/SSL>
#include <Mib/Memory/Allocators/Secure>
#include <Mib/Cryptography/RandomID>
#include <Mib/Intrusive/AVLTree>
#include <Mib/Concurrency/WeakActor>
#include <Mib/Concurrency/ActorCallbackManager>

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CActorDistributionManager::CInternal
		{
			CInternal(CActorDistributionManager *_pThis, NStr::CStr const &_HostID)
				: m_pThis(_pThis)
				, m_HostID(_HostID) 
				, m_ExecutionID(NCryptography::fg_RandomID())
			{
				
			}
			
			struct CHost;
			
			struct CConnection
			{
				virtual ~CConnection()
				{
				}
				
				TCActor<NWeb::CWebSocketActor> m_Connection;
				CActorCallback m_ConnectionSubscription;
				NPtr::TCSharedPointer<NNet::CSSLContext> m_pSSLContext;
				DMibListLinkDS_Link(CConnection, m_Link);
				CHost *m_pHost;
			};

			struct CClientConnection : public CConnection 
			{
				NHTTP::CURL m_ServerURL;
				mint m_ConnectionSequence = 0;
			};

			struct CServerConnection : public CConnection 
			{
			};
			
			struct CPacket
			{
				CPacket()
				{
				}
				NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> m_pData; // Shared pointer because we need to keep this around and possibly resend it
				uint64 f_GetPacketID() const;

				DMibIntrusiveLink(CPacket, NIntrusive::TCAVLLinkAggregate<>, m_TreeLink);
				DMibListLinkDS_Link(CPacket, m_Link);
				
				struct CSortPacketID
				{
					inline_always_debug uint64 operator ()(CPacket const &_Entry)
					{
						return _Entry.f_GetPacketID();
					}
				};
			};

			struct CRemoteActor
			{
				NStr::CStr m_Namespace;
				TCDistributedActor<CActor> m_Actor;
				NContainer::TCVector<uint32> m_Hierarchy;
				
				NStr::CStr const &f_GetActorID() const
				{
					return NContainer::TCMap<NStr::CStr, CRemoteActor>::fs_GetKey(this);
				}
				
				DMibListLinkDS_Link(CRemoteActor, m_Link);
				
				~CRemoteActor()
				{
					DMibCheck(!m_Link.f_IsInList());
				}
			};			

			struct CRemoteNamespace
			{
				DMibListLinkDS_List(CRemoteActor, m_Link) m_RemoteActors;
			};
			
			struct CHost
			{
				DMibListLinkDS_List(CConnection, m_Link) m_ActiveConnections;
				CConnection *m_pLastSendConnection = nullptr;
				
				DMibListLinkDS_List(CPacket, m_Link) m_Incoming_ReceivedPackets;
				DMibListLinkDS_List(CPacket, m_Link) m_Incoming_QueuedPackets;
				uint64 m_Incoming_ReceivedPacketID = 1;
				uint64 m_Incoming_AckedPacketID = 0;

				DMibListLinkDS_List(CPacket, m_Link) m_Outgoing_QueuedPackets;
				NIntrusive::TCAVLTree<CPacket::CLinkTraits_m_TreeLink, CPacket::CSortPacketID> m_Outgoing_SentPackets;
				uint64 m_Outgoing_CurrentPacketID = 0;
				uint64 m_Outgoing_AckedPacketID = 0;
				
				NContainer::TCMap<uint32, TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>> m_OutstandingCalls;
				
				NStr::CStr m_LastExecutionID;
				
				NContainer::TCMap<NStr::CStr, CRemoteActor> m_RemoteActors;
				
				NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;
				bool m_bAllowAllNamespaces = false;
				
				bool m_bIncoming = false;
				bool m_bOutgoing = false;
				
				~CHost();
			
				NStr::CStr const &f_GetHostID() const;
				
			};
			
			struct CDistributedActorDataInternal : public NPrivate::CDistributedActorData
			{
				CHost *m_pHost;
			};
			
			struct CPublishedActor
			{
				TCWeakDistributedActor<CActor> m_Actor;
				NContainer::TCVector<uint32> m_Hierarchy;

				NStr::CStr const &f_GetActorID() const
				{
					return NContainer::TCMap<NStr::CStr, CPublishedActor>::fs_GetKey(this);
				}
				
				DMibIntrusiveLink(CPublishedActor, NIntrusive::TCAVLLink<>, m_TreeLink);
				
				struct CSortActorID
				{
					inline_always_debug auto &operator ()(CPublishedActor const &_Entry)
					{
						return _Entry.f_GetActorID();
					}
				};				
			};

			struct CLocalNamespace
			{
				NContainer::TCMap<NStr::CStr, CPublishedActor> m_Actors;

				NStr::CStr const &f_GetNamespace() const
				{
					return NContainer::TCMap<NStr::CStr, CLocalNamespace>::fs_GetKey(this);
				}
			};
			
			struct CActorSubscription
			{
				CActorSubscription(CActorDistributionManager *_pManager);
				TCActorCallbackManager<void (CAbstractDistributedActor &&_NewActor), true> m_fOnNewActor;
				TCActorCallbackManager<void (TCWeakDistributedActor<CActor> const &_RemovedActor), true> m_fOnRemovedActorActor;
			};
			
			NContainer::TCLinkedList<NPtr::TCSharedPointer<CClientConnection>> m_ClientConnections;
			NContainer::TCLinkedList<NPtr::TCSharedPointer<CServerConnection>> m_ServerConnections;
			
			NContainer::TCMap<NStr::CStr, CHost> m_Hosts;
			CActorDistributionManager *m_pThis;
			NStr::CStr m_ExecutionID;
			NStr::CStr m_HostID;
			
			NContainer::TCMap<NStr::CStr, CLocalNamespace> m_LocalNamespaces;
			NContainer::TCMap<NStr::CStr, CRemoteNamespace> m_RemoteNamespaces;

			NIntrusive::TCAVLTree<CPublishedActor::CLinkTraits_m_TreeLink, CPublishedActor::CSortActorID> m_PublishedActors;
			NContainer::TCMap<NStr::CStr, CActorSubscription> m_SubscribedActors;

			NContainer::TCSet<NStr::CStr> m_AllowedIncomingConnectionNamespaces;
			NContainer::TCSet<NStr::CStr> m_AllowedOutgoingConnectionNamespaces;
			NContainer::TCSet<NStr::CStr> m_AllowedBothNamespaces;
			
			TCActor<NWeb::CWebSocketServerActor> m_WebsocketServer;
			CActorCallback m_ListenCallbackSubscription;

			TCActor<NWeb::CWebSocketClientActor> m_WebsocketClientConnector;
			
			void fp_ScheduleReconnect(NPtr::TCSharedPointer<CClientConnection> const &_pConnection, TCContinuation<void> &_Continuation, bool _bRetry, mint _Sequence);
			void fp_Reconnect(NPtr::TCSharedPointer<CClientConnection> const &_pConnection, TCContinuation<void> &_Continuation, bool _bRetry);
			void fp_ServerConnectionClosed(NPtr::TCSharedPointer<CServerConnection> const &_pConnection);
			void fp_Listen(CActorDistributionListenSettings const &_Settings, TCContinuation<void> &_Continuation);
			template <typename tf_CCommand>
			void fp_QueueCommand(CHost *_pHost, tf_CCommand const &_Command);
			void fp_QueuePacket(CHost *_pHost, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_Data);
			void fp_SendPacketQueue(CHost *_pHost);
			void fp_ProcessPacketQueue(CConnection *_pConnection);
			bool fp_HandleProtocolIncoming(CConnection *_pConnection, NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage);
			void fp_Identify(CConnection *_pConnection);
			NContainer::TCSet<NStr::CStr> const &fp_GetAllowedNamespacesForHost(CHost *_pHost, bool &o_bAllowAll);
			void fp_NotifyNewActor(CHost *_pHost, CRemoteActor &_RemoteActor);
			void fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor);
			
			void fp_ReplyToRemoteCallWithException(CHost *_pHost, uint64 _PacketID, NException::CExceptionBase const &_Exception);
			void fp_ReplyToRemoteCallWithException(CHost *_pHost, uint64 _PacketID, CAsyncResult const &_Exception);
			void fp_ReplyToRemoteCall(CHost *_pHost, uint64 _PacketID, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data);
			bool fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			
		};
	}
}

#include "Malterlib_Concurrency_DistributedActor_Manager_PacketQueue.hpp"