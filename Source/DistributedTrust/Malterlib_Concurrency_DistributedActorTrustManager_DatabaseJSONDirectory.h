// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib
{
	namespace NConcurrency
	{
		class CDistributedActorTrustManagerDatabase_JSONDirectory : public ICDistributedActorTrustManagerDatabase
		{
		public:
			CDistributedActorTrustManagerDatabase_JSONDirectory(NStr::CStr const &_BaseDirectory);
			~CDistributedActorTrustManagerDatabase_JSONDirectory();
			
			TCContinuation<CBasicConfig> f_GetBasicConfig() override;
			TCContinuation<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) override;
			TCContinuation<int32> f_GetNewCertificateSerial() override;
			TCContinuation<CDefaultUser> f_GetDefaultUser() override;
			TCContinuation<void> f_SetDefaultUser(CDefaultUser const &_DefaultUser) override;

			TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override;
			TCContinuation<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) override;
			TCContinuation<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
			TCContinuation<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) override;
			TCContinuation<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) override;
			
			TCContinuation<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() override;
			TCContinuation<void> f_AddListenConfig(CListenConfig const &_Config) override;
			TCContinuation<void> f_RemoveListenConfig(CListenConfig const &_Config) override;

			TCContinuation<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override;
 			TCContinuation<CClient> f_GetClient(NStr::CStr const &_HostID) override;
			TCContinuation<NPtr::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) override;
 			TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) override;
			TCContinuation<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) override;
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
			
			TCContinuation<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> f_EnumPermissions(bool _bIncludeFullInfo) override;
			TCContinuation<CPermissions> f_GetPermissions(CPermissionIdentifiers const &_Identity) override;
			TCContinuation<void> f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) override;
			TCContinuation<void> f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) override;
			TCContinuation<void> f_RemovePermissions(CPermissionIdentifiers const &_Identity) override;

			TCContinuation<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) override;
			TCContinuation<CUserInfo> f_GetUserInfo(NStr::CStr const &_User) override;
			TCContinuation<void> f_AddUser(NStr::CStr const &_User, CUserInfo const &_Users) override;
			TCContinuation<void> f_SetUserInfo(NStr::CStr const &_User, CUserInfo const &_Users) override;
			TCContinuation<void> f_RemoveUser(NStr::CStr const &_User) override;

			TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> f_EnumAuthenticationFactor(bool _bIncludeFullInfo) override;
			TCContinuation<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorName, CUserAuthenticationFactor const &_Factor) override;
			TCContinuation<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorName, CUserAuthenticationFactor const &_Factor) override;
			TCContinuation<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorName) override;

		private:
			struct CInternal;
			NPtr::TCUniquePointer<CInternal> mp_pInternal;
		};
	}
}
