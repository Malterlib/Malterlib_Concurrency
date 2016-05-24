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
		namespace NActorDistributionManagerInternal
		{
			struct CHost;
		}
	}
}
			
DMibDefineSharedPointerType(NMib::NConcurrency::NActorDistributionManagerInternal::CHost, true, false);

namespace NMib
{
	namespace NConcurrency
	{
		namespace NActorDistributionManagerInternal
		{
			struct CConnection
			{
				virtual ~CConnection()
				{
				}
				
				void f_Reset();
				void f_Destroy();
				
				TCActor<NWeb::CWebSocketActor> m_Connection;
				CActorCallback m_ConnectionSubscription;
				NPtr::TCSharedPointer<NNet::CSSLContext> m_pSSLContext;
				NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> m_pHost;
				DMibListLinkDS_Link(CConnection, m_Link);
				DMibListLinkDS_Link(CConnection, m_HostLink);
				bool m_bIncoming = false;
			};

			struct CClientConnection : public CConnection 
			{
				void f_Reset();
				void f_Destroy();
				
				NStr::CStr m_ConnectionID;
				NHTTP::CURL m_ServerURL;
				NMib::NNet::ENetAddressType m_PreferAddress;
				mint m_ConnectionSequence = 0;
				bool m_bConnected = false;
				NStr::CStr m_LastConnectionError;
			};

			struct CServerConnection : public CConnection 
			{
				CServerConnection(mint _ConnectionID)
					: m_ConnectionID(_ConnectionID)
				{
				}
				
				mint const m_ConnectionID;
			};
			
			struct CPacket
			{
				CPacket()
				{
				}
				NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> m_pData; // Shared pointer because we need to keep this around and possibly resend it
				uint64 f_GetPacketID() const;

				DMibIntrusiveLink(CPacket, NIntrusive::TCAVLLink<>, m_TreeLink);
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
				CHost *m_pHost = nullptr;			
				
				NStr::CStr const &f_GetActorID() const
				{
					return NContainer::TCMap<NStr::CStr, CRemoteActor>::fs_GetKey(this);
				}
				
				DMibListLinkDS_Link(CRemoteActor, m_Link);
				
				~CRemoteActor()
				{
					DMibFastCheck(!m_Link.f_IsInList());
				}
			};			

			struct CRemoteNamespace
			{
				DMibListLinkDS_List(CRemoteActor, m_Link) m_RemoteActors;
			};
			
			struct CHost : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
			{
				DMibListLinkDS_List(CClientConnection, m_HostLink) m_ClientConnections;
				DMibListLinkDS_List(CServerConnection, m_HostLink) m_ServerConnections;
				
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
				NStr::CStr m_UniqueHostID; // Differs from HostID when anonymous 
				NStr::CStr m_RealHostID; 
				
				NContainer::TCMap<NStr::CStr, CRemoteActor> m_RemoteActors;
				
				NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;
				bool m_bAllowAllNamespaces = false;
				
				bool m_bIncoming = false;
				bool m_bOutgoing = false;
				bool m_bAnonymous = false;
				bool m_bDeleted = false;
				
				~CHost();
				
				void f_DeletePackets();
				void f_Destroy();
				bool f_CanReceivePublish() const;
				bool f_CanSendPublish() const;
			};
			
			struct CDistributedActorDataInternal : public NPrivate::CDistributedActorData
			{
				NPtr::TCWeakPointer<CHost> m_pHost;
			};
			
			struct CLocalNamespace; 
			
			struct CPublishedActor
			{
				TCWeakDistributedActor<CActor> m_Actor;
				NContainer::TCVector<uint32> m_Hierarchy;
				CLocalNamespace *m_pNamespace = nullptr;

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

			struct CListen
			{
				TCActor<NWeb::CWebSocketServerActor> m_WebsocketServer;
				CActorCallback m_ListenCallbackSubscription;
				
				NStr::CStr const &f_GetID() const
				{
					return NContainer::TCMap<NStr::CStr, CListen>::fs_GetKey(*this);
				}
			};
		}
		
		struct CActorDistributionManager::CInternal
		{
			using CConnection = NActorDistributionManagerInternal::CConnection;
			using CClientConnection = NActorDistributionManagerInternal::CClientConnection; 
			using CServerConnection = NActorDistributionManagerInternal::CServerConnection; 
			using CPacket = NActorDistributionManagerInternal::CPacket;
			using CRemoteActor = NActorDistributionManagerInternal::CRemoteActor;
			using CRemoteNamespace = NActorDistributionManagerInternal::CRemoteNamespace;
			using CHost = NActorDistributionManagerInternal::CHost;
			using CDistributedActorDataInternal = NActorDistributionManagerInternal::CDistributedActorDataInternal;
			using CPublishedActor = NActorDistributionManagerInternal::CPublishedActor;
			using CLocalNamespace = NActorDistributionManagerInternal::CLocalNamespace;
			using CActorSubscription = NActorDistributionManagerInternal::CActorSubscription;
			using CListen = NActorDistributionManagerInternal::CListen;
			
			friend struct NActorDistributionManagerInternal::CHost;
			
			CInternal(CActorDistributionManager *_pThis, NStr::CStr const &_HostID);
			~CInternal();
			
			NContainer::TCMap<NStr::CStr, NPtr::TCSharedPointer<CClientConnection>> m_ClientConnections;
			NContainer::TCMap<mint, NPtr::TCSharedPointer<CServerConnection>> m_ServerConnections;
			mint m_NextConnectionID = 0;
			
			NContainer::TCMap<NStr::CStr, NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag>> m_Hosts;
			CActorDistributionManager *m_pThis;
			NStr::CStr m_ExecutionID;
			NStr::CStr m_HostID;
			
			NContainer::TCMap<NStr::CStr, CActorSubscription> m_SubscribedActors;

			NContainer::TCSet<NStr::CStr> m_AllowedIncomingConnectionNamespaces;
			NContainer::TCSet<NStr::CStr> m_AllowedOutgoingConnectionNamespaces;
			NContainer::TCSet<NStr::CStr> m_AllowedBothNamespaces;
			
			NContainer::TCMap<NStr::CStr, CListen> m_Listens;
			
			TCActor<NWeb::CWebSocketClientActor> m_WebsocketClientConnector;
			
			NContainer::TCMap<NStr::CStr, CLocalNamespace> m_LocalNamespaces;
			NContainer::TCMap<NStr::CStr, CRemoteNamespace> m_RemoteNamespaces;

			NIntrusive::TCAVLTree<CPublishedActor::CLinkTraits_m_TreeLink, CPublishedActor::CSortActorID> m_PublishedActors;
			
			TCActor<ICActorDistributionManagerAccessHandler> m_AccessHandler;
			
			void fp_ScheduleReconnect
				(
					NPtr::TCSharedPointer<CClientConnection> const &_pConnection
					, NPtr::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
					, bool _bRetry
					, mint _Sequence
					, NStr::CStr const &_ConnectionError 
				)
			;
			void fp_Reconnect
				(
					NPtr::TCSharedPointer<CClientConnection> const &_pConnection
					, NPtr::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
					, bool _bRetry
				)
			;
			void fp_DestroyServerConnection(CServerConnection &_Connection, bool _bSaveHost);
			void fp_DestroyClientConnection(CClientConnection &_Connection, bool _bSaveHost);

			void fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection);
			void fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection);
			void fp_Listen
				(
					NStr::CStr const &_ListenID
					, CActorDistributionListenSettings const &_Settings
					, NPtr::TCSharedPointer<TCContinuation<CDistributedActorListenReference>> const &_pContinuation
				)
			;
			template <typename tf_CCommand>
			void fp_QueueCommand(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, tf_CCommand const &_Command);
			void fp_QueuePacket(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_Data);
			void fp_SendPacketQueue(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost);
			void fp_ProcessPacketQueue(CConnection *_pConnection);
			bool fp_HandleProtocolIncoming(CConnection *_pConnection, NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage);
			void fp_Identify(CConnection *_pConnection);
			NContainer::TCSet<NStr::CStr> const &fp_GetAllowedNamespacesForHost(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, bool &o_bAllowAll);
			void fp_NotifyNewActor(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, CRemoteActor &_RemoteActor);
			void fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor);
			
			void fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, NException::CExceptionBase const &_Exception);
			void fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, CAsyncResult const &_Exception);
			void fp_ReplyToRemoteCall(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data);
			bool fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
			bool fp_NamespaceAllowedForAnonymous(NStr::CStr const &_Namespace) const;
		};
	}
}

#include "Malterlib_Concurrency_DistributedActor_Manager_PacketQueue.hpp"