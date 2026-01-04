// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthActor.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthCache.h"

#include <Mib/Concurrency/ActorCallOnce>
#include <Mib/Concurrency/DistributedActorAuthentication>
#include <Mib/Concurrency/DistributedActorAuthenticationHandler>
#include <Mib/Concurrency/ActorSequencerActor>

namespace NMib::NConcurrency
{
	struct CAuthenticationActorInfo;
	using namespace NDistributedActorTrustManagerDatabase;

	enum EInitialize
	{
		EInitialize_None
		, EInitialize_Success
		, EInitialize_Failure
	};

	struct CDistributedActorTrustManager::CInternal : public CActorInternal
	{
		struct CHostState;

		struct CListenState
		{
			CDistributedActorListenReference m_ListenReference;
		};

		struct CConnectionState
		{
			CClientConnection m_ClientConnection;
			NContainer::TCVector<CDistributedActorConnectionReference> m_ConnectionReferences;
			CHostState *m_pHost = nullptr;
			bool m_bRemoving = false;

			DMibListLinkDS_Link(CConnectionState, m_Link);

			inline CDistributedActorTrustManager_Address const &f_GetAddress() const;
		};

		struct CTicketState
		{
			NTime::CTimer m_CreationTime;
			TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest)> m_fOnUseTicket;
			TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo)> m_fOnCertificateSigned;
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

			NIntrusive::TCAVLLink<> m_LinkAllowed;
			NIntrusive::TCAVLLink<> m_LinkHost;

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
			NIntrusive::TCAVLTree<&CTypedActor::m_LinkHost, CTypedActor::CCompareIdentifier> m_Actors;
		};

		struct CNamespaceTypeState
		{
			uint32 f_GetTypeHash() const
			{
				return NContainer::TCMap<uint32, CNamespaceTypeState>::fs_GetKey(*this);
			}
			NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> f_GetAllowed(NPrivate::CTrustedActorSubscriptionState const &_State);

			NContainer::TCSet<NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState>> m_Subscriptions;

			NContainer::TCMap<CDistributedActorIdentifier, CTypedActor> m_Actors;
			NContainer::TCMap<NStr::CStr, CTypedHost> m_Hosts;
			NIntrusive::TCAVLTree<&CTypedActor::m_LinkAllowed, CTypedActor::CCompareIdentifier> m_AllowedActors;

		};

		struct CNamespaceState
		{
			CNamespace m_Namespace;
			CActorSubscription m_Subscription;
			NContainer::TCMap<uint32, CNamespaceTypeState> m_Types;
			mint m_nSubscriptions = 0;
			mint m_SubscriptionSequence = 0;
			bool m_bExistsInDatabase = false;
			bool m_bSubscribing = false;
			NContainer::TCVector<NFunction::TCFunctionMovable<void (TCAsyncResult<void> const &_Result, CNamespaceState &_NamespaceState)>> m_OnSubscribe;

			inline NStr::CStr const &f_GetNamespaceName() const;
		};

		struct CPermissionState
		{
			NDistributedActorTrustManagerDatabase::CPermissions m_Permissions;
			bool m_bExistsInDatabase = false;

			inline CPermissionIdentifiers const &f_GetIdentity() const;
		};

		struct CPermissionSubscriptionState
		{
			NContainer::TCSet<NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState>> m_Subscriptions;
			NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> m_PermissionsPerIdentity;

			inline NStr::CStr const &f_GetPermission() const;
		};

		struct CUserState
		{
			NDistributedActorTrustManagerDatabase::CUserInfo m_UserInfo;
			bool m_bExistsInDatabase = false;
		};

		struct CUserAuthenticationFactorState
		{
			NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor m_AuthenticationFactor;
			bool m_bExistsInDatabase = false;
		};

		struct CTicketInterface : public CActor
		{
			enum : uint32
			{
				EProtocolVersion_Min = 0x101
				, EProtocolVersion_Current = 0x101
			};

			CTicketInterface(CDistributedActorTrustManager::CInternal *_pInternal, TCWeakActor<CDistributedActorTrustManager> const &_ThisActor);

			TCFuture<NContainer::CByteVector> f_SignCertificate(NStr::CStr _Token, NContainer::CByteVector _CertificateRequest);
			
		private:
			CDistributedActorTrustManager::CInternal *mp_pInternal;
			TCWeakActor<CDistributedActorTrustManager> mp_ThisActor;
		};

		struct CDistributedActorAuthenticationImplementation : public ICDistributedActorAuthentication
		{
			CDistributedActorAuthenticationImplementation();

			auto f_RegisterAuthenticationHandler(CRegisterAuthenticationHandlerParams _Params)
				-> TCFuture<TCActorSubscriptionWithID<>> override
			;
			TCFuture<bool> f_AuthenticatePermissionPattern
				(
					NStr::CStr _Pattern
					, NContainer::TCSet<NStr::CStr> _AuthenticationFactors
					, NStr::CStr _RequestID
				) override
			;

			DMibDelegatedActorImplementation(CDistributedActorTrustManager);
		};

		struct CActorDistributionManagerAccessHandler : public ICActorDistributionManagerAccessHandler
		{
			using CActorHolder = CDelegatedActorHolder;

			TCFuture<NStr::CStr> f_ValidateClientAccess(NStr::CStr _HostID, NContainer::TCVector<NContainer::CByteVector> _CertificateChain) override;

			CActorDistributionManagerAccessHandler(CDistributedActorTrustManager::CInternal *_pInternal);

			DMibDelegatedActorImplementation(CDistributedActorTrustManager::CInternal);
		};

		CInternal
			(
				CDistributedActorTrustManager *_pThis
				, TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
				, COptions &&_Options
			)
		;
		~CInternal();

		TCFuture<void> f_Init();

		TCFuture<void> f_WaitForInit();

		void f_RemoveClientConnection(CConnectionState *_pClientConnection);

		TCFuture<NContainer::CByteVector> f_SignCertificate
			(
				NStr::CStr _Token
				, NContainer::CByteVector _CertificateRequest
				, CCallingHostInfo _HostInfo
			)
		;
		TCFuture<NStr::CStr> f_ValidateClientAccess(NStr::CStr _HostID, NContainer::TCVector<NContainer::CByteVector> _CertificateChain);

		NMib::NConcurrency::CActorDistributionConnectionSettings f_GetConnectionSettings(CConnectionState const &_State);

		void f_ApplyConnectionConcurrency(CConnectionState &_ConnectionState);

		TCFuture<bool> f_AuthenticatePermissionPattern
			(
				NStr::CStr _Pattern
				, NContainer::TCSet<NStr::CStr> _AuthenticationFactor
				, CCallingHostInfo _CallingHostInfo
				, NStr::CStr _RequestID
			)
		;

		NStorage::TCSharedPointer<bool> m_pDestroyed = fg_Construct(false);
		CDistributedActorTrustManager *m_pThis;
		TCActor<ICDistributedActorTrustManagerDatabase> m_Database;
		NFunction::TCFunctionMovable<TCActor<CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)> m_fDistributionManagerFactory;

		TCActor<CActorDistributionManager> m_ActorDistributionManager;
		TCActor<CActorDistributionManagerAccessHandler> m_AccessHandler;

		TCDistributedActor<CTicketInterface> m_TicketInterface;
		CDistributedActorPublication m_TicketInterfacePublication;

		TCDistributedActorInstance<CDistributedActorAuthenticationImplementation> m_AuthenticationInterface;

		CSequencer m_InitSequencer{"Init"};

		CActorSubscription m_HostInfoChangedSubscription;

		NStr::CStr m_FriendlyName;
		NStr::CStr m_Enclave;

		CBasicConfig m_BasicConfig;
		CDefaultUser m_DefaultUser;

		NContainer::TCMap<CListenConfig, CListenState> m_Listen;
		NContainer::TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;

		CListenState *m_pPrimaryListen = nullptr;

		NContainer::TCMap<NStr::CStr, CHostState> m_Hosts;
		NContainer::TCMap<CDistributedActorTrustManager_Address, CConnectionState> m_ClientConnections;
		NContainer::TCSet<CDistributedActorTrustManager_Address> m_ClientConnectionsInDatabase;

		NContainer::TCMap<NStr::CStr, CTicketState> m_Tickets;

		NContainer::TCMap<NStr::CStr, CNamespaceState> m_Namespaces;
		mint m_NamespaceSubscriptionSequence = 0;

		NContainer::TCSet<NStr::CStr> m_RegisteredPermissions;
		NContainer::TCMap<CPermissionIdentifiers, CPermissionState> m_Permissions;
		NContainer::TCMap<NStr::CStr, CPermissionSubscriptionState> m_PermissionsSubscriptions;
		CDistributedActorTrustManagerAuthenticationCache m_AuthenticationCache;

		NContainer::TCMap<NStr::CStr, CUserState> m_Users;

		// The outer map uses the UserID as index, the inner uses the FactorID
		NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactorState>> m_UserAuthenticationFactors;
		NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo> m_AuthenticationActors;

		NTime::CTimer m_TicketTimer;

		NCryptography::CPublicKeySetting const m_KeySetting;
		NNetwork::ENetFlag const m_ListenFlags;
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;

		EInitialize m_Initialize = EInitialize_None;
		NException::CExceptionPointer m_pInitializeException;

		NContainer::TCVector<TCPromise<void>> m_AwaitingConnection;

		fp64 m_InitialConnectionTimeout = 5.0;
		fp64 m_HostTimeout;
		fp64 m_HostDaemonTimeout;
		fp64 m_ReconnectDelay;

		int32 m_DefaultConnectionConcurrency = 1;

		bool m_bRetryOnListenFailureDuringInit = true;
		bool m_bWaitForConnectionsDuringInit = true;
		bool m_bSupportAuthentication = true;
		bool m_bConnectionsInitialized = false;
		bool m_bTimeoutForUnixSockets = true;
	};
}

#include "Malterlib_Concurrency_DistributedTrust_Internal.hpp"
