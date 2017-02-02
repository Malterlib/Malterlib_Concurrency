// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CTrustManagerDatabaseTestHelper : public ICDistributedActorTrustManagerDatabase
	{
		static constexpr bool mc_bAllowInternalAccess = true;
		
		TCContinuation<CBasicConfig> f_GetBasicConfig() override;
		TCContinuation<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) override;
		TCContinuation<int32> f_GetNewCertificateSerial() override;
		TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override;
		TCContinuation<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) override;
		TCContinuation<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
		TCContinuation<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
		TCContinuation<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) override;
		TCContinuation<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() override;
		TCContinuation<void> f_AddListenConfig(CListenConfig const &_Config) override;
		TCContinuation<void> f_RemoveListenConfig(CListenConfig const &_Config) override;
		TCContinuation<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override;
		TCContinuation<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) override;
		TCContinuation<CClient> f_GetClient(NStr::CStr const &_HostID) override;
		TCContinuation<NPtr::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) override;
		TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) override;
		TCContinuation<void> f_SetClient(NStr::CStr const &_HostID, CClient const &_Client) override;
		TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) override;
		TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> f_EnumClientConnections(bool _bIncludeFullInfo) override;
		TCContinuation<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address) override;
		TCContinuation<void> f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override;
		TCContinuation<void> f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override;
		TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) override;
		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) override;
		TCContinuation<CNamespace> f_GetNamespace(NStr::CStr const &_NamespaceName) override;
		TCContinuation<void> f_AddNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override;
		TCContinuation<void> f_SetNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override;
		TCContinuation<void> f_RemoveNamespace(NStr::CStr const &_NamespaceName) override;
		TCContinuation<NContainer::TCMap<NStr::CStr, CHostPermissions>> f_EnumHostPermissions(bool _bIncludeFullInfo) override;
		TCContinuation<CHostPermissions> f_GetHostPermissions(NStr::CStr const &_HostID) override;
		TCContinuation<void> f_AddHostPermissions(NStr::CStr const &_HostID, CHostPermissions const &_HostPermissions) override;
		TCContinuation<void> f_SetHostPermissions(NStr::CStr const &_HostID, CHostPermissions const &_HostPermissions) override;
		TCContinuation<void> f_RemoveHostPermissions(NStr::CStr const &_HostID) override;
		
		CBasicConfig m_BasicConfig;
		int32 m_CertificateSerial = 0;
		
		NContainer::TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;
		NContainer::TCSet<CListenConfig> m_ListenConfigs;
		NContainer::TCMap<NStr::CStr, CClient> m_Clients;
		NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> m_ClientConnections;
		NContainer::TCMap<NStr::CStr, CNamespace> m_Namespaces;
		NContainer::TCMap<NStr::CStr, CHostPermissions> m_HostPermissions;
	};
	
	struct CTrustManagerTestHelper
	{
		TCActor<CDistributedActorTrustManager> f_TrustManager(NStr::CStr const &_FriendlyName, NStr::CStr const &_SessionID = {}) const;
		
		CTrustManagerTestHelper();
		~CTrustManagerTestHelper();
		
		TCActor<CTrustManagerDatabaseTestHelper> m_Database;
	};
	
	struct CTrustedSubscriptionTestHelper
	{
		CTrustedSubscriptionTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CTrustedSubscriptionTestHelper();
		
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> f_Subscribe(NStr::CStr const &_Namespace = tf_CActor::mc_pDefaultNamespace);
		template <typename tf_CActor>
		NContainer::TCVector<TCDistributedActor<tf_CActor>> f_SubscribeMultiple(mint _nActors, NStr::CStr const &_Namespace = tf_CActor::mc_pDefaultNamespace);
		
	private:
		struct CSubscription
		{
			virtual ~CSubscription() = default; 
		};

		struct CInternal : public CActor
		{
			CInternal(TCActor<CDistributedActorTrustManager> const &_TrustManager);

			template <typename tf_CActor>
			TCContinuation<NContainer::TCVector<TCDistributedActor<tf_CActor>>> f_Subscribe(mint _nActors, NStr::CStr const &_Namespace);
			
		private:
			TCActor<CDistributedActorTrustManager> mp_TrustManager;
			NContainer::TCVector<NPtr::TCUniquePointer<CSubscription>> mp_Subscriptions;
		};
		
		TCActor<CInternal> mp_Internal;		
	};
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_TestHelpers.hpp"
