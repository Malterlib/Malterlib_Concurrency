// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CTrustManagerDatabaseTestHelper : public ICDistributedActorTrustManagerDatabase
	{
		static constexpr bool mc_bAllowInternalAccess = true;
		
		TCFuture<CBasicConfig> f_GetBasicConfig() override;
		TCFuture<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) override;
		TCFuture<int32> f_GetNewCertificateSerial() override;
		TCFuture<CDefaultUser> f_GetDefaultUser() override;
		TCFuture<void> f_SetDefaultUser(CDefaultUser const &_DefaultUser) override;
		TCFuture<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override;
		TCFuture<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) override;
		TCFuture<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
		TCFuture<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
		TCFuture<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) override;
		TCFuture<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() override;
		TCFuture<void> f_AddListenConfig(CListenConfig const &_Config) override;
		TCFuture<void> f_RemoveListenConfig(CListenConfig const &_Config) override;
		TCFuture<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override;
		TCFuture<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) override;
		TCFuture<CClient> f_GetClient(NStr::CStr const &_HostID) override;
		TCFuture<NStorage::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) override;
		TCFuture<bool> f_HasClient(NStr::CStr const &_HostID) override;
		TCFuture<void> f_SetClient(NStr::CStr const &_HostID, CClient const &_Client) override;
		TCFuture<void> f_RemoveClient(NStr::CStr const &_HostID) override;
		TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> f_EnumClientConnections(bool _bIncludeFullInfo) override;
		TCFuture<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address) override;
		TCFuture<void> f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override;
		TCFuture<void> f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) override;
		TCFuture<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) override;
		TCFuture<NContainer::TCMap<NStr::CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) override;
		TCFuture<CNamespace> f_GetNamespace(NStr::CStr const &_NamespaceName) override;
		TCFuture<void> f_AddNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override;
		TCFuture<void> f_SetNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) override;
		TCFuture<void> f_RemoveNamespace(NStr::CStr const &_NamespaceName) override;
		TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> f_EnumPermissions(bool _bIncludeFullInfo) override;
		TCFuture<CPermissions> f_GetPermissions(CPermissionIdentifiers const &_Identity) override;
		TCFuture<void> f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) override;
		TCFuture<void> f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) override;
		TCFuture<void> f_RemovePermissions(CPermissionIdentifiers const &_Identity) override;
		TCFuture<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) override;
		TCFuture<CUserInfo> f_GetUserInfo(NStr::CStr const &_UserID) override;
		TCFuture<void> f_AddUser(NStr::CStr const &_UserID, CUserInfo const &_UserInfo) override;
		TCFuture<void> f_SetUserInfo(NStr::CStr const &_UserID, CUserInfo const &_UserInfo) override;
		TCFuture<void> f_RemoveUser(NStr::CStr const &_UserID) override;
		TCFuture<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> f_EnumAuthenticationFactor(bool _bIncludeFullInfo) override;
		TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CUserAuthenticationFactor const &_Factor) override;
		TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CUserAuthenticationFactor const &_Factor) override;
		TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorName) override;

		CBasicConfig m_BasicConfig;
		int32 m_CertificateSerial = 0;
		CDefaultUser m_DefaultUser;

		NContainer::TCMap<NStr::CStr, CServerCertificate> m_ServerCertificates;
		NContainer::TCSet<CListenConfig> m_ListenConfigs;
		NContainer::TCMap<NStr::CStr, CClient> m_Clients;
		NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> m_ClientConnections;
		NContainer::TCMap<NStr::CStr, CNamespace> m_Namespaces;
		NContainer::TCMap<CPermissionIdentifiers, CPermissions> m_Permissions;
		NContainer::TCMap<NStr::CStr, CUserInfo> m_Users;
		NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>> m_UserAuthenticationFactors;
	};
	
	struct CTrustManagerTestHelper
	{
		TCActor<CDistributedActorTrustManager> f_TrustManager(NStr::CStr const &_FriendlyName, NStr::CStr const &_SessionID = {}, bool _bSupportAuthentication = true) const;
		
		CTrustManagerTestHelper();
		~CTrustManagerTestHelper();
		
		TCActor<CTrustManagerDatabaseTestHelper> m_Database;
	};
	
	struct CTrustedSubscriptionTestHelper
	{
		CTrustedSubscriptionTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager, fp64 _Timeout = 30.0);
		~CTrustedSubscriptionTestHelper();
		
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> f_SubscribeFromHost(NStr::CStr const &_HostID, NStr::CStr const &_Namespace = tf_CActor::mc_pDefaultNamespace);
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> f_Subscribe(NStr::CStr const &_Namespace = tf_CActor::mc_pDefaultNamespace, NStr::CStr const &_HostID = {});
		template <typename tf_CActor>
		NContainer::TCVector<TCDistributedActor<tf_CActor>> f_SubscribeMultiple
			(
				mint _nActors
				, NStr::CStr const &_Namespace = tf_CActor::mc_pDefaultNamespace
				, NStr::CStr const &_HostID = {}
			)
		;
		
	private:
		struct CSubscription
		{
			virtual ~CSubscription() = default; 
		};

		struct CInternal : public CActor
		{
			CInternal(TCActor<CDistributedActorTrustManager> const &_TrustManager);

			template <typename tf_CActor>
			TCFuture<NContainer::TCVector<TCDistributedActor<tf_CActor>>> f_Subscribe(mint _nActors, NStr::CStr const &_Namespace, NStr::CStr const &_HostID);
			
		private:
			TCActor<CDistributedActorTrustManager> mp_TrustManager;
			NContainer::TCVector<NStorage::TCUniquePointer<CSubscription>> mp_Subscriptions;
			NContainer::TCSet<NStr::CStr> mp_SeenActors;
		};
		
		TCActor<CInternal> mp_Internal;		
		fp64 mp_Timeout;
	};
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_TestHelpers.hpp"
