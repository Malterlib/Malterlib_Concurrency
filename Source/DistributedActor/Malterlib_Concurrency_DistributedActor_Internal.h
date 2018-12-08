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
	struct CConnection : public NStorage::TCSharedPointerIntrusiveBase<NStorage::ESharedPointerOption_SupportWeakPointer>
	{
		virtual ~CConnection()
		{
		}

		void f_Reset(bool _bResetHost);
		void f_Destroy(NStr::CStr const &_Error);
		TCDispatchedActorCall<void> f_Disconnect();
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

		TCActor<NWeb::CWebSocketActor> m_Connection;
		CActorSubscription m_ConnectionSubscription;
		NStorage::TCSharedPointer<NNetwork::CSSLContext> m_pSSLContext;
		NStorage::TCSharedPointerSupportWeak<CHost> m_pHost;
		TCContinuation<void> m_IdentifyContinuation;
		DMibListLinkDS_Link(CConnection, m_Link);
		DMibListLinkDS_Link(CConnection, m_HostLink);
		NStr::CStr m_LastError;
		bool m_bIncoming = false;
		bool m_bIdentified = false;

		CHostInfo f_GetHostInfo() const;
	};

	struct CClientConnection : public CConnection
	{
		void f_Reset(bool _bResetHost);
		void f_Destroy(NStr::CStr const &_Error);
		NStr::CStr f_GetConnectionID() const override;
		NStr::CStr f_GetServerURL() const override;
		void f_SetLastError(NStr::CStr const &_Error);

		NStr::CStr m_ConnectionID;
		NWeb::NHTTP::CURL m_ServerURL;
		mint m_ConnectionSequence = 0;
		bool m_bConnected = false;
		bool m_bAnonymous = false;
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
	};

	struct CPacket
	{
		CPacket(NStorage::TCSharedPointer<NContainer::CSecureByteVector> const &_pData)
			: m_pData(_pData)
		{
		}
		NStorage::TCSharedPointer<NContainer::CSecureByteVector> m_pData; // Shared pointer because we need to keep this around and possibly resend it
		uint64 f_GetPacketID() const;

		NIntrusive::TCAVLLink<> m_TreeLink;
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
		TCContinuation<NContainer::CSecureByteVector> m_Continuation;
		NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> m_pState;
	};

	struct CHost : public NPrivate::ICHost
	{
		CHost(CActorDistributionManager &_DistributionManager);
		~CHost();

		void f_DeletePackets();
		void f_Destroy();
		bool f_CanReceivePublish() const;
		bool f_CanSendPublish() const;
		CHostInfo f_GetHostInfo() const;

		void f_DestroyImplicitFunction(NStr::CStr const &_FunctionID);

		DMibListLinkDS_List(CClientConnection, m_HostLink) m_ClientConnections;
		DMibListLinkDS_List(CServerConnection, m_HostLink) m_ServerConnections;

		DMibListLinkDS_List(CConnection, m_Link) m_ActiveConnections;
		CConnection *m_pLastSendConnection = nullptr;

		DMibListLinkDS_List(CPacket, m_Link) m_Incoming_ReceivedPackets;
		uint64 m_Incoming_NextPacketID = 1;

		DMibListLinkDS_List(CPacket, m_Link) m_Outgoing_QueuedPackets;
		NIntrusive::TCAVLTree<&CPacket::m_TreeLink, CPacket::CSortPacketID> m_Outgoing_SentPackets;
		uint64 m_Outgoing_CurrentPacketID = 0;

		DMibListLinkDS_Link(CHost, m_RealHostsLink);

		NContainer::TCMap<uint32, COutstandingCall> m_OutstandingCalls;

		NContainer::TCMap<NStr::CStr, TCWeakActor<CActor>> m_ImplicitlyPublishedActors;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedInterface> m_ImplicitlyPublishedInterfaces;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedFunction> m_ImplicitlyPublishedFunctions;
		NContainer::TCMap<NStr::CStr, CImplicitlyPublishedSubscription> m_ImplicitlyPublishedSubscriptions;
		NContainer::TCMap<NStr::CStr, CSubscriptionReferences> m_RemoteSubscriptionReferences;
		NContainer::TCMap<NStr::CStr, CSubscriptionReferences> m_LocalSubscriptionReferences;

		NContainer::TCMap<NStr::CStr, TCContinuation<void>> m_PendingRemoteSubscriptionDestroys;

		NStr::CStr m_LastExecutionID;
		NStr::CStr m_UniqueHostID; // Differs from HostID when anonymous
		NStr::CStr m_RealHostID;
		NStr::CStr m_FriendlyName;

		NContainer::TCMap<NStr::CStr, CRemoteActor> m_RemoteActors;

		TCActorSubscriptionManager<void (), true> m_OnDisconnect;

		NContainer::TCSet<NStr::CStr> m_AllowedNamespaces;

		TCDistributedActor<ICDistributedActorAuthenticationHandler> m_AuthenticationHandler;
		NStr::CStr m_ClaimedUserID;
		NStr::CStr m_ClaimedUserName;

		bool m_bAllowAllNamespaces = false;

		bool m_bIncoming = false;
		bool m_bOutgoing = false;
		bool m_bAnonymous = false;
		bool m_bDeleted = false;
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

	struct CActorPublicationSubscription
	{
		CActorPublicationSubscription(CActorDistributionManager *_pManager);
		TCActorSubscriptionManager<void (CAbstractDistributedActor &&_NewActor), true> m_fOnNewActor;
		TCActorSubscriptionManager<void (CDistributedActorIdentifier const &_RemovedActor), true> m_fOnRemovedActorActor;
	};

	struct CListen
	{
		TCActor<NWeb::CWebSocketServerActor> m_WebsocketServer;
		CActorSubscription m_ListenCallbackSubscription;
		NStr::CStr m_LastReportedError;

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
				TCContinuation<void> (NStorage::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pNewServerConnection, NStr::CStr const &_RealHostID)
			>
			m_fNewWebsocketConnection
		;
	};
}
	
namespace NMib::NConcurrency
{
	struct CActorDistributionManagerInternal
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

		friend struct NActorDistributionManagerInternal::CHost;
		
		CActorDistributionManagerInternal(CActorDistributionManager *_pThis, CActorDistributionManagerInitSettings const &_InitSettings);
		~CActorDistributionManagerInternal();
		
		NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag>> m_ClientConnections;
		NContainer::TCMap<mint, NStorage::TCSharedPointer<CServerConnection, NStorage::CSupportWeakTag>> m_ServerConnections;
		mint m_NextConnectionID = 0;
		
		NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointerSupportWeak<CHost>> m_Hosts;
		NContainer::TCMap<NStr::CStr, DMibListLinkDS_List(CHost, m_RealHostsLink)> m_HostsByRealHostID;
		CActorDistributionManager *m_pThis;
		NStr::CStr m_ExecutionID;
		NStr::CStr m_HostID;
		NStr::CStr m_FriendlyName;
		NStr::CStr m_Enclave;
		
		TCActorSubscriptionManager<void (CHostInfo const &_HostInfo), true> m_OnHostInfoChanged;
		
		NContainer::TCMap<NStr::CStr, CActorPublicationSubscription> m_SubscribedActors;

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
		
		void fp_ScheduleReconnect
			(
				NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
				, NStorage::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
				, bool _bRetry
				, mint _Sequence
				, NStr::CStr const &_ConnectionError 
			)
		;
		void fp_Reconnect
			(
				NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
				, NStorage::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
				, bool _bRetry
			)
		;
		void fp_DestroyServerConnection(CServerConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error);
		void fp_DestroyClientConnection(CClientConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error);
		
		NStr::CStr fp_TranslateHostname(NStr::CStr const &_Hostname) const;
		NWeb::NHTTP::CURL fp_TranslateURL(NWeb::NHTTP::CURL const &_URL) const;
		
		template <typename tf_CReturnType>
		bool fp_DecodeClientConnectionSettings
			(
				CActorDistributionConnectionSettings const &_Settings
				, TCContinuation<tf_CReturnType> &_Continuation
				, CDecodedClientConnectionSetting &o_DecodedSettings
			)
		;

		void fp_ResetHostState(CHost &_Host, CConnection *_pSaveConnection, bool _bSaveInactive);
		void fp_DestroyHost(CHost &_Host, CConnection *_pSaveConnection);
		void fp_Listen
			(
				NStr::CStr const &_ListenID
				, CActorDistributionListenSettings const &_Settings
				, NStorage::TCSharedPointer<TCContinuation<CDistributedActorListenReference>> const &_pContinuation
			)
		;
		template <typename tf_CCommand>
		void fp_QueueCommand(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, tf_CCommand const &_Command);
		uint64 fp_QueuePacket(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, NContainer::CSecureByteVector &&_Data);
		void fp_SendPacketQueue(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost);
		void fp_ProcessPacketQueue(CConnection *_pConnection);
		void fp_SendPacket(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CSecureByteVector> &&_pMessage);
		bool fp_HandleProtocolIncoming(CConnection *_pConnection, NStorage::TCSharedPointer<NContainer::CSecureByteVector> const &_pMessage);
		void fp_Identify(CConnection *_pConnection);
		NContainer::TCSet<NStr::CStr> const &fp_GetAllowedNamespacesForHost(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, bool &o_bAllowAll);
		void fp_NotifyNewActor(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, CRemoteActor &_RemoteActor);
		void fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor);
		
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
				, NContainer::CSecureByteVector const &_Data
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
		bool fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleDestroySubscription(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_HandleSubscriptionDestroyed(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream);
		bool fp_NamespaceAllowedForAnonymous(NStr::CStr const &_Namespace) const;
		bool fp_RegisterActorFunctorsForCall(NPrivate::CDistributedActorStreamContextState &_State, NActorDistributionManagerInternal::CHost &_Host, TCContinuation<> &_Continuation);
		void fp_RegisterLocalSubscriptions(NPrivate::CDistributedActorStreamContextState &_State);
		void fp_DestroyLocalSubscription(NActorDistributionManagerInternal::CHost &_Host, NStr::CStr const &_SubscriptionID);
	};
}

#include "Malterlib_Concurrency_DistributedActor_Manager_PacketQueue.hpp"
