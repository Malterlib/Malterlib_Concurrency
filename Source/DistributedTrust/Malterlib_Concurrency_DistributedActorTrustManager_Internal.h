// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

#include <Mib/Concurrency/ActorCallOnce>

namespace NMib
{
	namespace NConcurrency
	{
		using namespace NDistributedActorTrustManagerDatabase;
		
		enum EInitialize
		{
			EInitialize_None
			, EInitialize_Success
			, EInitialize_Failure
		};
		
		struct CDistributedActorTrustManager::CInternal
		{
			struct CHostState;
			
			struct CListenState
			{
				CDistributedActorListenReference m_ListenReference;
			};		

			struct CConnectionState
			{
				CClientConnection m_ClientConnection;
				CDistributedActorConnectionReference m_ConnectionReference;
				CHostState *m_pHost = nullptr;
				
				DMibListLinkDS_Link(CConnectionState, m_Link);
				
				inline CDistributedActorTrustManager_Address const &f_GetAddress() const;
			};
			
			struct CTicketState
			{
				NTime::CTimer m_CreationTime;
			};
			
			struct CHostState
			{
				DMibListLinkDS_List(CConnectionState, m_Link) m_ClientConnections;
				NStr::CStr m_FriendlyName;
				
				inline NStr::CStr const &f_GetHostID() const;
			};
			
			struct CTypedHost;
			
			struct CTypedActor
			{
				CDistributedActorIdentifier const &f_GetIdentifier() const
				{
					return NContainer::TCMap<CDistributedActorIdentifier, CTypedActor>::fs_GetKey(*this);
				}
				CAbstractDistributedActor m_AbstractActor;
				CTrustedActorInfo m_TrustedInfo;
				CTypedHost *m_pHost = nullptr;
				
				DMibIntrusiveLink(CTypedActor, NIntrusive::TCAVLLink<>, m_LinkAllowed);
				DMibIntrusiveLink(CTypedActor, NIntrusive::TCAVLLink<>, m_LinkHost);
				
				struct CCompareIdentifier
				{
					auto &operator()(CTypedActor const &_Actor)
					{
						return _Actor.f_GetIdentifier();
					}
				};
			};

			struct CTypedHost
			{
				NStr::CStr const &f_GetHostID() const
				{
					return NContainer::TCMap<NStr::CStr, CTypedHost>::fs_GetKey(*this);
				}
				NIntrusive::TCAVLTree<CTypedActor::CLinkTraits_m_LinkHost, CTypedActor::CCompareIdentifier> m_Actors;
			};
			
			struct CNamespaceTypeState
			{
				uint32 f_GetTypeHash() const
				{
					return NContainer::TCMap<uint32, CNamespaceTypeState>::fs_GetKey(*this);					
				}
				NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> f_GetAllowed(NPrivate::CTrustedActorSubscriptionState const &_State);
				
				NContainer::TCSet<NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState>> m_Subscriptions;
				
				NContainer::TCMap<CDistributedActorIdentifier, CTypedActor> m_Actors;
				NContainer::TCMap<NStr::CStr, CTypedHost> m_Hosts;
				NIntrusive::TCAVLTree<CTypedActor::CLinkTraits_m_LinkAllowed, CTypedActor::CCompareIdentifier> m_AllowedActors;
				
			};
			
			struct CNamespaceState
			{
				CNamespace m_Namespace;
				CActorSubscription m_Subscription;
				NContainer::TCMap<uint32, CNamespaceTypeState> m_Types;
				mint m_nSubscriptions = 0;
				bool m_bExistsInDatabase = false;
				bool m_bSubscribing = false;
				NContainer::TCVector<NFunction::TCFunction<bool (TCAsyncResult<void> const &_Result, CNamespaceState &_NamespaceState)>> m_OnSubscribe;
				
				inline NStr::CStr const &f_GetNamespaceName() const;
			};

			struct CHostPermissionState
			{
				NDistributedActorTrustManagerDatabase::CHostPermissions m_HostPermissions;
				bool m_bExistsInDatabase = false;
				
				inline NStr::CStr const &f_GetHostID() const;
			};
			
			struct CHostPermissionSubscriptionState
			{
				NContainer::TCSet<NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>> m_Subscriptions;
				NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> m_Permissions;

				inline NStr::CStr const &f_GetPermission() const;
			};
			
			struct CTicketInterface : public CActor
			{
				enum
				{
					EMinProtocolVersion = 0x101
					, EProtocolVersion = 0x101
				};
				
				TCContinuation<NContainer::TCVector<uint8>> f_SignCertificate(NStr::CStr const &_Token, NContainer::TCVector<uint8> const &_CertificateRequest);
				CTicketInterface(CDistributedActorTrustManager::CInternal *_pInternal, TCWeakActor<CDistributedActorTrustManager> const &_ThisActor);
			private:
				CDistributedActorTrustManager::CInternal *mp_pInternal;
				TCWeakActor<CDistributedActorTrustManager> mp_ThisActor;
			};
			
			struct CActorDistributionManagerAccessHandler : public ICActorDistributionManagerAccessHandler
			{
				TCContinuation<NStr::CStr> f_ValidateClientAccess(NStr::CStr const &_HostID, NContainer::TCVector<NContainer::TCVector<uint8>> const &_CertificateChain) override;
				CActorDistributionManagerAccessHandler(CDistributedActorTrustManager::CInternal *_pInternal, TCWeakActor<CDistributedActorTrustManager> const &_ThisActor);
			private:
				CDistributedActorTrustManager::CInternal *mp_pInternal;
				TCWeakActor<CDistributedActorTrustManager> mp_ThisActor;
			};
			
			CInternal
				(
					CDistributedActorTrustManager *_pThis
					, TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
					, NFunction::TCFunctionMovable
					<
						TCActor<CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)
					> &&_fConstructManager
					, NNet::CSSLKeySetting _KeySetting
					, NNet::ENetFlag _ListenFlags
					, NStr::CStr const &_FriendlyName
					, NStr::CStr const &_Enclave
					, NContainer::TCMap<NStr::CStr, NStr::CStr> const &_TranslateHostnames
				)
			;
			~CInternal();
			
			TCContinuation<NStr::CStr> f_InitAttempt();
			void f_Init
				(
					TCContinuation<NStr::CStr> &_Continuation
					, CBasicConfig const &_Basic
					, NContainer::TCSet<CListenConfig> const &_Listen
					, NContainer::TCMap<NStr::CStr, CServerCertificate> const &_ServerCertificates
					, NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> const &_ClientConnections
					, NContainer::TCMap<NStr::CStr, CNamespace> const &_Namespaces
					, NContainer::TCMap<NStr::CStr, CHostPermissions> const &_HostPermissions
				)
			;
			
			template <typename tf_CReturn>
			void f_RunAfterInit(TCContinuation<tf_CReturn> const &_Continuation, NFunction::TCFunctionMovable<void ()> &&_fToRun);
			
			void f_RemoveClientConnection(CConnectionState *_pClientConnection);
			
			TCContinuation<NContainer::TCVector<uint8>> f_SignCertificate(NStr::CStr const &_Token, NContainer::TCVector<uint8> const &_CertificateRequest);
			TCContinuation<NStr::CStr> f_ValidateClientAccess(NStr::CStr const &_HostID, NContainer::TCVector<NContainer::TCVector<uint8>> const &_CertificateChain);
			
			NMib::NConcurrency::CActorDistributionConnectionSettings f_GetConnectionSettings(CConnectionState const &_State);

			NPtr::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
			CDistributedActorTrustManager *m_pThis;
			TCActor<ICDistributedActorTrustManagerDatabase> m_Database;
			NFunction::TCFunctionMovable<TCActor<CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)> m_fDistributionManagerFactory;
			
			TCActor<CActorDistributionManager> m_ActorDistributionManager;
			TCActor<CActorDistributionManagerAccessHandler> m_AccessHandler;
			
			TCDistributedActor<CTicketInterface> m_TicketInterface;
			CDistributedActorPublication m_TicketInterfacePublication;		

			NPtr::TCUniquePointer<TCActorCallOnce<NStr::CStr>> m_pInitOnce;
			
			CActorSubscription m_HostInfoChangedSubscription;
			
			NStr::CStr m_FriendlyName;
			NStr::CStr m_Enclave;
			
			CBasicConfig m_BasicConfig;
			
			NContainer::TCMap<CListenConfig, CListenState> m_Listen;
			NContainer::TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;
			
			NContainer::TCMap<NStr::CStr, CHostState> m_Hosts;
			NContainer::TCMap<CDistributedActorTrustManager_Address, CConnectionState> m_ClientConnections;
			
			NContainer::TCMap<NStr::CStr, CTicketState> m_Tickets;
			
			NContainer::TCMap<NStr::CStr, CNamespaceState> m_Namespaces;
			
			NContainer::TCSet<NStr::CStr> m_RegisteredPermissions;
			NContainer::TCMap<NStr::CStr, CHostPermissionState> m_HostPermissions;
			NContainer::TCMap<NStr::CStr, CHostPermissionSubscriptionState> m_HostPermissionsSubscriptions;
			
			NTime::CTimer m_TicketTimer;
			
			NNet::CSSLKeySetting const m_KeySetting;
			NNet::ENetFlag const m_ListenFlags;
			NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;
			
			EInitialize m_Initialize = EInitialize_None;
			NStr::CStr m_InitializeError;
		};
	}
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.hpp"
