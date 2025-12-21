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
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Network/ResolveActor>

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib::NConcurrency::NActorDistributionManagerInternal
{
	struct CHost;
}

DMibDefineSharedPointerType(NMib::NConcurrency::NActorDistributionManagerInternal::CHost, true, false);

namespace NMib::NConcurrency
{
	struct CResultSubscriptionData;
}

namespace NMib::NConcurrency::NActorDistributionManagerInternal
{
	struct CConnection
	{
		virtual ~CConnection()
		{
			f_DiscardIdentifyPromise("Destroyed");

			if (m_Connection)
				fg_Move(m_Connection).f_Destroy().f_DiscardResult();
		}

		void f_Reset(bool _bResetHost, CActorDistributionManagerInternal &_This, NStr::CStr const &_Message, TCPromise<void> *_pPromise);
		void f_Destroy(NStr::CStr const &_Message, CActorDistributionManagerInternal &_This, TCPromise<void> *_pPromise);
		TCFuture<void> f_Disconnect();
		static void fs_LogClose
			(
				TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> const &_Result
				, bool _bIsLastConnection
				, NStr::CStr const &_ConnectionID
				, NStr::CStr const &_ServerURL
				, NStr::CStr const &_Desc
			)
		;
		virtual NStr::CStr f_GetConnectionID() const = 0;
		virtual NStr::CStr f_GetServerURL() const;
		void f_DiscardIdentifyPromise(NStr::CStr const &_Error);

		NStorage::CIntrusiveRefCountWithWeak m_RefCount;
		TCActor<NWeb::CWebSocketActor> m_Connection;
		CActorSubscription m_ConnectionSubscription;
		NStorage::TCSharedPointer<NNetwork::CSSLContext> m_pSSLContext;
		NStorage::TCSharedPointerSupportWeak<CHost> m_pHost;
		TCPromise<bool> m_IdentifyPromise;
		NContainer::TCVector<TCPromise<void>> m_PublishFinished;
		DMibListLinkDS_Link(CConnection, m_Link);
		DMibListLinkDS_Link(CConnection, m_HostLink);
		bool m_bIncoming = false;
		bool m_bIdentified = false;
		bool m_bPulishFinished = false;
		bool m_bFirstConnection = false;

		CHostInfo f_GetHostInfo() const;
	};

	struct CClientConnection : public CConnection
	{
		void f_Reset(bool _bResetHost, CActorDistributionManagerInternal &_This, NStr::CStr const &_Message);
		void f_Destroy(NStr::CStr const &_Message, CActorDistributionManagerInternal &_This, TCPromise<void> *_pPromise);
		NStr::CStr f_GetConnectionID() const override;
		NStr::CStr f_GetServerURL() const override;
		void f_SetLastError(NStr::CStr const &_Error);

		NStr::CStr m_ConnectionID;
		NWeb::NHTTP::CURL m_ServerURL;
		mint m_ConnectionSequence = 0;
		bool m_bConnected = false;
		bool m_bAnonymous = false;
		bool m_bRetryConnectOnFailure = false;
		bool m_bRetryConnectOnFirstFailure = false;
		NStr::CStr m_LastConnectionError;
		NTime::CTime m_LastConnectionErrorTime;
		NStr::CStr m_LastLoggedError;
		NStr::CStr m_ExpectedRealHostID;
	};

	struct CServerConnection : public CConnection
	{
		CServerConnection(mint _ConnectionID);
		NStr::CStr f_GetConnectionID() const override;

		mint const m_ConnectionID;
		NStr::CStr m_ListenID;
	};

	// Pack priority (8 bits) and packetID (56 bits) into a single uint64 for efficient lookup
	// Format: [priority:8][packetID:56] - priority in high bits for correct sort order
	inline_always uint64 fg_MakePriorityPacketKey(uint8 _Priority, uint64 _PacketID)
	{
		return (uint64(_Priority) << 56) | (_PacketID & 0x00FFFFFFFFFFFFFF);
	}

	// Validate that a packet ID from the network fits in 56 bits
	// Returns false if the ID has upper bits set (potential malicious client)
	inline_always bool fg_IsValidPacketID(uint64 _PacketID)
	{
		return (_PacketID & 0xFF00000000000000) == 0;
	}

	struct CPacket final
	{
		// Sort by (priority, packetID) packed into uint64 for efficient lookup during resend
		struct CSortPriorityPacketID
		{
			inline_always_debug uint64 operator ()(CPacket const &_Entry)
			{
				return fg_MakePriorityPacketKey(_Entry.m_Priority, _Entry.f_GetPacketID());
			}
		};

		CPacket(NStorage::TCSharedPointer<NContainer::CIOByteVector> const &_pData, uint8 _Priority)
			: m_pData(_pData)
			, m_Priority(_Priority)
		{
		}

		uint64 f_GetPacketID() const;

		NStorage::TCSharedPointer<NContainer::CIOByteVector> m_pData; // Shared pointer because we need to keep this around and possibly resend it
		NIntrusive::TCAVLLink<> m_TreeLink;
		DMibListLinkDS_Link(CPacket, m_Link);

		uint8 m_Priority = 128; // Packet priority (0 = highest, 255 = lowest, 128 = default)
	};

	struct CRemoteActor
	{
		NStr::CStr m_Namespace;
		TCDistributedActor<CActor> m_Actor;
		NContainer::TCVector<uint32> m_Hierarchy;
		CHost *m_pHost = nullptr;
		CDistributedActorProtocolVersions m_ProtocolVersions;

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

	struct CImplicitlyPublishedFunction
	{
		NStorage::TCSharedPointer<NPrivate::CStreamingFunction> m_pFunction;
		NStr::CStr m_AllowedDisptachActorID;
	};

	struct CImplicitlyPublishedSubscription
	{
		CActorSubscription m_Subscription;
		NContainer::TCVector<TCActor<>> m_ReferencedActors;
	};

	struct CPublishedInterface
	{
		TCWeakDistributedActor<CActor> m_Actor;
		NContainer::TCVector<uint32> m_Hierarchy;
		CDistributedActorProtocolVersions m_ProtocolVersions;
	};

	struct CImplicitlyPublishedInterface : public CPublishedInterface
	{
	};

	struct CSubscriptionReferences
	{
		NContainer::TCSet<NStr::CStr> m_Functions;
		NContainer::TCSet<NStr::CStr> m_Actors;
		NContainer::TCSet<NStr::CStr> m_Interfaces;
	};

	struct COutstandingCall
	{
		TCPromise<NContainer::CIOByteVector> m_Promise{CPromiseConstructEmpty()};
		NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> m_pState;
	};

	// Composite key for outstanding calls: encodes priority in upper 8 bits, packet ID in lower 56 bits
	inline_always uint64 fg_MakeOutstandingCallKey(uint8 _Priority, uint64 _PacketID)
	{
		return (uint64(_Priority) << 56) | (_PacketID & 0x00FFFFFFFFFFFFFFull);
	}

	struct CWaitingForPublication
	{
		TCPromise<void> m_Promise;
	};

	// Per-priority packet queue for priority-aware packet handling
	struct CPriorityQueues
	{
		DMibListLinkDS_List(CPacket, m_Link) m_IncomingPackets;
		DMibListLinkDS_List(CPacket, m_Link) m_OutgoingPackets;
		uint64 m_IncomingNextPacketID = 1; // Receiver: next expected packet ID
		uint64 m_OutgoingCurrentPacketID = 0; // Sender: last assigned packet ID
	};

	struct CHost : public NPrivate::ICHost
	{
		CHost(CActorDistributionManager &_DistributionManager, CDistributedActorHostInfo &&_Info);
		~CHost();

		struct COnDisconnect
		{
			~COnDisconnect();

			TCActorFunctorWeak<TCFuture<void> ()> m_fOnDisconnect;
			NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
		};

		struct CAuthenticationHandlerEntry
		{
			TCDistributedActor<ICDistributedActorAuthenticationHandler> m_Handler;
			NStr::CStr m_ClaimedUserID;
			NStr::CStr m_ClaimedUserName;
		};

		void f_DeletePackets();
		void f_Destroy();
		bool f_CanReceivePublish() const;
		bool f_CanSendPublish() const;
		bool f_IsDaemon() const;
		CHostInfo f_GetHostInfo() const;

		void f_DestroyImplicitFunction(NStr::CStr const &_FunctionID);
		uint32 f_GetUniqueWaitPublicationID();

		DMibListLinkDS_List(CClientConnection, m_HostLink) m_ClientConnections;
		DMibListLinkDS_List(CServerConnection, m_HostLink) m_ServerConnections;

		DMibListLinkDS_List(CConnection, m_Link) m_ActiveConnections;
		CConnection *m_pLastSendConnection = nullptr;

		// Per-priority packet queues for packet handling
		NContainer::TCMap<uint8, CPriorityQueues> m_PriorityQueues;
		NIntrusive::TCAVLTree<&CPacket::m_TreeLink, CPacket::CSortPriorityPacketID> m_Outgoing_SentPackets;

		DMibListLinkDS_Link(CHost, m_RealHostsLink);

		NTime::CClock m_InactiveClock;
		DMibListLinkDS_Link(CHost, m_InactiveHostsLink);

		NContainer::TCMap<uint64, COutstandingCall> m_OutstandingCalls; // Key is fg_MakeOutstandingCallKey(priority, packetID)

		NContainer::TCMap<NStr::CStr, TCWeakActor<CActor>> m_ImplicitlyPublishedActors;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedInterface> m_ImplicitlyPublishedInterfaces;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedFunction> m_ImplicitlyPublishedFunctions;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedSubscription> m_ImplicitlyPublishedSubscriptions;
		NContainer::TCMap<NStr::CStr, CSubscriptionReferences> m_RemoteSubscriptionReferences;
		NContainer::TCMap<NStr::CStr, CSubscriptionReferences> m_LocalSubscriptionReferences;

		NContainer::TCMap<NStr::CStr, TCPromise<void>> m_PendingRemoteSubscriptionDestroys;
		NContainer::TCMap<uint32, CWaitingForPublication> m_WaitingPublications;

		NStr::CStr m_LastExecutionID;
		NStr::CStr m_FriendlyName;

		NContainer::TCMap<NStr::CStr, CRemoteActor> m_RemoteActors;

		NContainer::TCLinkedList<COnDisconnect> m_OnDisconnect;

		NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;

		NContainer::TCMap<uint32, CAuthenticationHandlerEntry> m_AuthenticationHandlers;
		NStr::CStr m_ExecutionID = NCryptography::fg_RandomID();

		NStr::CStr m_LastError;
		NTime::CTime m_LastErrorTime;

		uint64 m_nSentPackets = 0;
		uint64 m_nSentBytes = 0;

		uint64 m_nReceivedPackets = 0;
		uint64 m_nReceivedBytes = 0;

		uint64 m_nDiscardedPackets = 0;
		uint64 m_nDiscardedBytes = 0;

		uint64 m_LastNotifyConnectionLostSequenceSent = 0;
		uint64 m_LastNotifyConnectionLostSequenceReceived = 0;

		uint32 m_NextWaitPublicationID = 1;

		bool m_bAllowAllNamespaces = false;

		bool m_bIncoming = false;
		bool m_bOutgoing = false;
		bool m_bLoggedConnection = false;
	};

	struct CLocalNamespace;

	struct CPublishedActor : public CPublishedInterface
	{
		CLocalNamespace *m_pNamespace = nullptr;

		NStr::CStr const &f_GetActorID() const
		{
			return NContainer::TCMap<NStr::CStr, CPublishedActor>::fs_GetKey(this);
		}

		NIntrusive::TCAVLLink<> m_TreeLink;

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

	struct CActorPublicationSubscriptionInstance
	{
		mint f_GetID() const
		{
			return NContainer::TCMap<mint, CActorPublicationSubscriptionInstance>::fs_GetKey(this);
		}

		TCActor<CActor> m_DispatchActor;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<TCFuture<void> (CAbstractDistributedActor &&_NewActor)>> m_pOnNewActor;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<TCFuture<void> (CDistributedActorIdentifier &&_RemovedActor)>> m_pOnRemovedActor;
	};

	struct CActorPublicationSubscription
	{
		CActorPublicationSubscription();
		~CActorPublicationSubscription();

		NContainer::TCMap<mint, CActorPublicationSubscriptionInstance> m_Instances;
	};

	struct CListen
	{
		TCActor<NWeb::CWebSocketServerActor> m_WebsocketServer;
		CActorSubscription m_ListenCallbackSubscription;
		NStr::CStr m_LastReportedError;
		NContainer::TCVector<NWeb::NHTTP::CURL> m_ListenAddresses;

		NStr::CStr const &f_GetID() const
		{
			return NContainer::TCMap<NStr::CStr, CListen>::fs_GetKey(*this);
		}
	};

	struct CDecodedClientConnectionSetting
	{
		bool m_bAnonymous;
		NNetwork::CSSLSettings m_ClientSettings;
		NStr::CStr m_RealHostID;
	};

	struct CWebsocketHandler
	{
		NStr::CStr const &f_GetWebPath() const
		{
			return NContainer::TCMap<NStr::CStr, CWebsocketHandler>::fs_GetKey(*this);
		}

		TCWeakActor<> m_Actor;
		NFunction::TCFunctionMutable
			<
				TCFuture<void> (NStorage::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pNewServerConnection, NStr::CStr const &_RealHostID)
			>
			m_fNewWebsocketConnection
		;
	};
}

namespace NMib::NConcurrency
{
	struct CActorDistributionManagerInternal : public CActorInternal
	{
		using CConnection = NActorDistributionManagerInternal::CConnection;
		using CClientConnection = NActorDistributionManagerInternal::CClientConnection;
		using CServerConnection = NActorDistributionManagerInternal::CServerConnection;
		using CPacket = NActorDistributionManagerInternal::CPacket;
		using CRemoteActor = NActorDistributionManagerInternal::CRemoteActor;
		using CRemoteNamespace = NActorDistributionManagerInternal::CRemoteNamespace;
		using CHost = NActorDistributionManagerInternal::CHost;
		using CPublishedActor = NActorDistributionManagerInternal::CPublishedActor;
		using CLocalNamespace = NActorDistributionManagerInternal::CLocalNamespace;
		using CActorPublicationSubscription = NActorDistributionManagerInternal::CActorPublicationSubscription;
		using CListen = NActorDistributionManagerInternal::CListen;
		using CDecodedClientConnectionSetting = NActorDistributionManagerInternal::CDecodedClientConnectionSetting;
		using CWebsocketHandler = NActorDistributionManagerInternal::CWebsocketHandler;
		using CPriorityQueues = NActorDistributionManagerInternal::CPriorityQueues;

		friend struct NActorDistributionManagerInternal::CHost;

		CActorDistributionManagerInternal(CActorDistributionManager *_pThis, CActorDistributionManagerInitSettings const &_InitSettings);
		~CActorDistributionManagerInternal();

		NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag>> m_ClientConnections;
		NContainer::TCMap<mint, NStorage::TCSharedPointer<CServerConnection, NStorage::CSupportWeakTag>> m_ServerConnections;
		mint m_NextConnectionID = 0;

		NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointerSupportWeak<CHost>> m_Hosts;
		NContainer::TCMap<NStr::CStr, DMibListLinkDS_List(CHost, m_RealHostsLink)> m_HostsByRealHostID;
		CActorDistributionManager *m_pThis;
		NStr::CStr m_HostID;
		NStr::CStr m_FriendlyName;
		NStr::CStr m_Enclave;

		struct COnHostInfoChanged
		{
			~COnHostInfoChanged();

			TCActorFunctorWeak<TCFuture<void> (CHostInfo _HostInfo)> m_fOnHostInfoChanged;
			NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
		};

		NContainer::TCLinkedList<COnHostInfoChanged> m_OnHostInfoChanged;

		NContainer::TCMap<NStr::CStr, CActorPublicationSubscription> m_SubscribedActors;
		mint m_SubscribedActorsNextInstanceID = 0;

		NWeb::CWebsocketSettings m_WebsocketSettings{.m_MaxMessageSize = CActorDistributionManager::mc_MaxMessageSize};

		NContainer::TCSet<NStr::CStr> m_AllowedIncomingConnectionNamespaces;
		NContainer::TCSet<NStr::CStr> m_AllowedOutgoingConnectionNamespaces;
		NContainer::TCSet<NStr::CStr> m_AllowedBothNamespaces;

		NContainer::TCMap<NStr::CStr, CListen> m_Listens;

		TCActor<NWeb::CWebSocketClientActor> m_WebsocketClientConnector;

		TCActor<NNetwork::CResolveActor> m_ResolveActor;

		NContainer::TCMap<NStr::CStr, CLocalNamespace> m_LocalNamespaces;
		NContainer::TCMap<NStr::CStr, CRemoteNamespace> m_RemoteNamespaces;

		NIntrusive::TCAVLTree<&CPublishedActor::m_TreeLink, CPublishedActor::CSortActorID> m_PublishedActors;

		TCActor<ICActorDistributionManagerAccessHandler> m_AccessHandler;
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;

		NContainer::TCMap<NStr::CStr, CWebsocketHandler> m_WebsocketHandlers;

		DMibListLinkDS_List(CHost, m_InactiveHostsLink) m_InactiveHosts;
		CActorSubscription m_CleanupTimerSubscription;
		fp64 m_HostTimeout;
		fp64 m_HostDaemonTimeout;
		fp64 m_ReconnectDelay;
		bool m_bCleanupSetup = false;
		bool m_bPreShutdown = false;

		void fp_CleanupUpdateTimer(bool _bForceUpdate);
		void fp_CleanupPerform();
		TCFuture<void> fp_PreShutdownCleanupPerform();
		void fp_CleanupMarkActive(CHost &_Host);
		void fp_CleanupMarkInactive(CHost &_Host);

		void fp_NotifyDisconnect(CHost &_Host);

		void fp_ScheduleReconnect
			(
				NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
				, bool _bRetry
				, mint _Sequence
				, NStr::CStr const &_ConnectionError
				, bool _bResetHost = false
			)
		;
		void fp_Reconnect
			(
				NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
				, NStorage::TCSharedPointer<TCPromise<CActorDistributionManager::CConnectionResult>> const &_pPromise
				, bool _bRetry
				, bool _bRetryOnFirst
				, fp64 _Timeout
			)
		;
		void fp_DestroyServerConnection(CServerConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error, bool _bLastActiveNormalClosure);
		void fp_DestroyClientConnection(CClientConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error, bool _bLastActiveNormalClosure, TCPromise<void> *_pPromise);

		NStr::CStr fp_TranslateHostname(NStr::CStr const &_Hostname) const;
		NWeb::NHTTP::CURL fp_TranslateURL(NWeb::NHTTP::CURL const &_URL) const;

		NException::CExceptionPointer fp_DecodeClientConnectionSettings
			(
				CActorDistributionConnectionSettings const &_Settings
				, CDecodedClientConnectionSetting &o_DecodedSettings
			)
		;

		void fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection, bool _bSaveInactive, NStr::CStr const &_Error);
		void fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection, NStr::CStr const &_Reason);
		TCFuture<void> fp_OnNewConnection(NStr::CStr _ListenID, NWeb::CWebSocketNewServerConnection _NewServerConnection);
		TCFuture<CDistributedActorListenReference> fp_ListenTry(NStr::CStr _ListenID, CActorDistributionListenSettings _Settings);
		TCFuture<CDistributedActorListenReference> fp_Listen(NStr::CStr _ListenID, CActorDistributionListenSettings _Settings);
		uint64 fp_QueuePacket(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, NContainer::CIOByteVector &&_Data);
		void fp_SendPacketQueue(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost);
		bool fp_QueueIncomingPacket(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CIOByteVector> const &_pMessage, NStream::CBinaryStreamMemoryPtr<> &_Stream, uint8 _Priority);
		void fp_ProcessPacketQueue(CConnection *_pConnection, CPriorityQueues &_PrioroityQueues, uint8 _Priority);
		void fp_RemoveAcknowledgedPackets(CHost &_Host, uint8 _Priority, uint64 _LastInOrderPacketID);
		void fp_SendPacket(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CIOByteVector> &&_pMessage, uint8 _Priority);
		void fp_OnInvalidConnection
			(
				CConnection *_pConnection
				, NException::CExceptionPointer &&_pException
			)
		;

		bool fp_HandleProtocolIncoming(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CIOByteVector> const &_pMessage);
		void fp_Identify(CConnection *_pConnection);
		NContainer::TCSet<NStr::CStr> const &fp_GetAllowedNamespacesForHost(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, bool &o_bAllowAll);
		void fp_NotifyNewActor
			(
				NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
				, CRemoteActor &_RemoteActor
				, TCFutureVector<void> *_pResults
			)
		;
		void fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor, TCFutureVector<void> *_pResults);

		void fp_ReplyToRemoteCallWithException
			(
				NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
				, uint64 _PacketID
				, NException::CExceptionBase const &_Exception
				, NPrivate::CDistributedActorStreamContext const &_Context
			)
		;
		void fp_ReplyToRemoteCallWithException
			(
				NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
				, uint64 _PacketID
				, CAsyncResult const &_Exception
				, NPrivate::CDistributedActorStreamContext const &_Context
			)
		;
		void fp_ReplyToRemoteCall
			(
				NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
				, uint64 _PacketID
				, NContainer::CIOByteVector const &_Data
				, NPrivate::CDistributedActorStreamContext const &_Context
			)
		;
		void fp_RegisterImplicitSubscriptions
			(
				CHost &_Host
				, NPrivate::CDistributedActorStreamContextState &_State
				, CResultSubscriptionData const *_pSubscriptionData
			)
		;
		bool fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream, bool _bHasAuthHandlerID, uint8 _Priority);
		bool fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream, uint8 _Priority);
		bool fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandlePublishFinishedPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleUnpublishFinishedPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleDestroySubscription(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleSubscriptionDestroyed(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_NamespaceAllowedForAnonymous(NStr::CStr const &_Namespace) const;
		NException::CExceptionPointer fp_RegisterActorFunctorsForCall(NPrivate::CDistributedActorStreamContextState &_State, CHost &_Host);
		void fp_RegisterLocalSubscriptions(NPrivate::CDistributedActorStreamContextState &_State);
		void fp_DestroyLocalSubscription(CHost &_Host, NStr::CStr const &_SubscriptionID);
		void fp_DestroyRemoteSubscriptionReset(CHost &_Host, NStr::CStr const &_SubscriptionID);
	};
}
