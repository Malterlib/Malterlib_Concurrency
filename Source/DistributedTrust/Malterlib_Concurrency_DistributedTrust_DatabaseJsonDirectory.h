// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedTrust_Database.h"

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManagerDatabase_JsonDirectory : public ICDistributedActorTrustManagerDatabase
	{
	public:
		CDistributedActorTrustManagerDatabase_JsonDirectory(NStr::CStr const &_BaseDirectory);
		~CDistributedActorTrustManagerDatabase_JsonDirectory();

		TCFuture<CBasicConfig> f_GetBasicConfig() override;
		TCFuture<void> f_SetBasicConfig(CBasicConfig _BasicConfig) override;
		TCFuture<int32> f_GetNewCertificateSerial() override;
		TCFuture<CDefaultUser> f_GetDefaultUser() override;
		TCFuture<void> f_SetDefaultUser(CDefaultUser _DefaultUser) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) override;
		TCFuture<CServerCertificate> f_GetServerCertificate(NStr::CStr _HostName) override;
		TCFuture<void> f_AddServerCertificate(NStr::CStr _HostName, CServerCertificate _Certificate) override;
		TCFuture<void> f_SetServerCertificate(NStr::CStr _HostName, CServerCertificate _Certificate) override;
		TCFuture<void> f_RemoveServerCertificate(NStr::CStr _HostName) override;

		TCFuture<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() override;
		TCFuture<void> f_AddListenConfig(CListenConfig _Config) override;
		TCFuture<void> f_RemoveListenConfig(CListenConfig _Config) override;

		TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> f_GetPrimaryListen() override;
		TCFuture<void> f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) override;
		TCFuture<CClient> f_GetClient(NStr::CStr _HostID) override;
		TCFuture<NStorage::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr _HostID) override;
		TCFuture<bool> f_HasClient(NStr::CStr _HostID) override;
		TCFuture<void> f_AddClient(NStr::CStr _HostID, CClient _Client) override;
		TCFuture<void> f_SetClient(NStr::CStr _HostID, CClient _Client) override;
		TCFuture<void> f_RemoveClient(NStr::CStr _HostID) override;

		TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> f_EnumClientConnections(bool _bIncludeFullInfo) override;
		TCFuture<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address _Address) override;
		TCFuture<void> f_AddClientConnection(CDistributedActorTrustManager_Address _Address, CClientConnection _ClientConnection) override;
		TCFuture<void> f_SetClientConnection(CDistributedActorTrustManager_Address _Address, CClientConnection _ClientConnection) override;
		TCFuture<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address _Address) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) override;
		TCFuture<CNamespace> f_GetNamespace(NStr::CStr _NamespaceName) override;
		TCFuture<void> f_AddNamespace(NStr::CStr _NamespaceName, CNamespace _Namespace) override;
		TCFuture<void> f_SetNamespace(NStr::CStr _NamespaceName, CNamespace _Namespace) override;
		TCFuture<void> f_RemoveNamespace(NStr::CStr _NamespaceName) override;

		TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> f_EnumPermissions(bool _bIncludeFullInfo) override;
		TCFuture<CPermissions> f_GetPermissions(CPermissionIdentifiers _Identity) override;
		TCFuture<void> f_AddPermissions(CPermissionIdentifiers _Identity, CPermissions _Permissions) override;
		TCFuture<void> f_SetPermissions(CPermissionIdentifiers _Identity, CPermissions _Permissions) override;
		TCFuture<void> f_RemovePermissions(CPermissionIdentifiers _Identity) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) override;
		TCFuture<CUserInfo> f_GetUserInfo(NStr::CStr _User) override;
		TCFuture<void> f_AddUser(NStr::CStr _User, CUserInfo _Users) override;
		TCFuture<void> f_SetUserInfo(NStr::CStr _User, CUserInfo _Users) override;
		TCFuture<void> f_RemoveUser(NStr::CStr _User) override;

		TCFuture<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> f_EnumAuthenticationFactor(bool _bIncludeFullInfo) override;
		TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorName, CUserAuthenticationFactor _Factor) override;
		TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorName, CUserAuthenticationFactor _Factor) override;
		TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorName) override;

		TCFuture<NStr::CStr> f_GetClientLastFriendlyName(NStr::CStr _HostID) override;

	private:
		TCFuture<void> fp_Destroy() override;

		struct CInternal;
		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
