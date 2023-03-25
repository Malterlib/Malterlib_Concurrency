// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedTrust.h"

namespace NMib::NConcurrency
{
	class ICDistributedActorTrustManagerAuthenticationActor;
	struct CAuthenticationActorInfo;

	class CDistributedActorTrustManagerProxy : public CDistributedActorTrustManagerInterface
	{
	public:
		enum EPermission
		{
			EPermission_None = 0
			
			, EPermission_HostID_Read = DMibBit(0)

			, EPermission_Listen_Read = DMibBit(1)
			, EPermission_Listen_Add = DMibBit(2)
			, EPermission_Listen_Remove = DMibBit(3)

			, EPermission_Client_Read = DMibBit(4)
			, EPermission_Client_Add = DMibBit(5)
			, EPermission_Client_Remove = DMibBit(6)

			, EPermission_ClientConnection_Read = DMibBit(7)
			, EPermission_ClientConnection_Write = DMibBit(8)
			, EPermission_ClientConnection_Remove = DMibBit(9)

			, EPermission_NamespacePermissions_Read = DMibBit(10)
			, EPermission_NamespacePermissions_Add = DMibBit(11)
			, EPermission_NamespacePermissions_Remove = DMibBit(12)

			, EPermission_Permissions_Read = DMibBit(13)
			, EPermission_Permissions_Add = DMibBit(14)
			, EPermission_Permissions_Remove = DMibBit(15)

			, EPermission_User_Read = DMibBit(16)
			, EPermission_User_Write = DMibBit(17)
			, EPermission_User_Remove = DMibBit(18)

			, EPermission_UserPrivate_Read = DMibBit(19)

			, EPermission_ConnectionsDebugStats_Read = DMibBit(20)

			, EPermission_Listen_Write = DMibBit(21)

			, EPermission_Listen = EPermission_Listen_Read | EPermission_Listen_Write | EPermission_Listen_Add | EPermission_Listen_Remove
			, EPermission_Client = EPermission_Client_Read | EPermission_Client_Add | EPermission_Client_Remove
			, EPermission_ClientConnection = EPermission_ClientConnection_Read | EPermission_ClientConnection_Write | EPermission_ClientConnection_Remove
			, EPermission_NamespacePermissions = EPermission_NamespacePermissions_Read | EPermission_NamespacePermissions_Add | EPermission_NamespacePermissions_Remove
			, EPermission_Permissions = EPermission_Permissions_Read | EPermission_Permissions_Add | EPermission_Permissions_Remove
			, EPermission_UserPermissions = EPermission_User_Read | EPermission_User_Write | EPermission_User_Remove

			, EPermission_All =
			(
				EPermission_HostID_Read 
				| EPermission_Listen 
				| EPermission_Client 
				| EPermission_ClientConnection 
				| EPermission_NamespacePermissions 
				| EPermission_Permissions
				| EPermission_UserPermissions
				| EPermission_UserPrivate_Read
				| EPermission_ConnectionsDebugStats_Read
			)
		};
		
		struct CPermissions
		{
			EPermission m_Permissions = EPermission_None;
			NContainer::TCSet<NStr::CStr> m_AllowedNamespaces; // If empty all namespaces are allowed
			NContainer::TCSet<NStr::CStr> m_AllowedPermissions; // If empty all permissions are allowed
			NContainer::TCSet<NStr::CStr> m_AllowedHosts; // If empty all hosts are allowed
			NContainer::TCSet<NStr::CStr> m_AllowedUsers; // If empty all users are allowed
		};
		
		CDistributedActorTrustManagerProxy(TCActor<CDistributedActorTrustManager> const &_Actor, CPermissions const &_Permissions);
		~CDistributedActorTrustManagerProxy();

		TCFuture<NStr::CStr> f_GetHostID() const override;

		TCFuture<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens() override;
		TCFuture<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address) override;
		TCFuture<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address) override;
		TCFuture<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address) override;
		TCFuture<void> f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> const &_Address) override;
		TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> f_GetPrimaryListen() override;

		TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients() override;
		TCFuture<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket(CGenerateConnectionTicket &&_Command) override;
		TCFuture<void> f_RemoveClient(NStr::CStr const &_HostID) override;
		TCFuture<bool> f_HasClient(NStr::CStr const &_HostID) override;

		TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections() override;
		TCFuture<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1) override;
		TCFuture<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) override;
		TCFuture<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) override;
		TCFuture<void> f_RemoveClientConnection(CRemoveClientConnection const &_Command) override;
		TCFuture<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo) override;
		TCFuture<void> f_AllowHostsForNamespace(CChangeNamespaceHosts const &_Command) override;
		TCFuture<void> f_DisallowHostsForNamespace(CChangeNamespaceHosts const &_Command) override;

		TCFuture<CEnumPermissionsResult> f_EnumPermissions(bool _bIncludeHostInfo) override;
		TCFuture<void> f_AddPermissions(CAddPermissions const &_Command) override;
		TCFuture<void> f_RemovePermissions(CRemovePermissions const &_Command) override;

		TCFuture<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) override;
		TCFuture<NStorage::TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> f_TryGetUser(NStr::CStr const &_UserID) override;
		TCFuture<void> f_AddUser(NStr::CStr const &_UserID, NStr::CStr const &_UserName) override;
		TCFuture<void> f_RemoveUser(NStr::CStr const &_UserID) override;
		TCFuture<void> f_SetUserInfo
			(
				NStr::CStr const &_UserID
				, NStorage::TCOptional<NStr::CStr> const &_UserName
				, NContainer::TCSet<NStr::CStr> const &_RemoveMetadata
				, NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> const &_AddMetadata
			) override
		;
		TCFuture<NStr::CStr> f_ExportUser(NStr::CStr const &_UserID, bool _bIncludePrivate);
		TCFuture<NStr::CStr> f_ImportUser(NStr::CStr const &_UserData);

		TCFuture<NContainer::TCMap<NStr::CStr, CLocalAuthenticationActorInfo>> f_EnumAuthenticationActors() const override;
		TCFuture<NContainer::TCMap<NStr::CStr, CLocalAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID) const override;
		TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CLocalAuthenticationData &&_Data) override;
		TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CLocalAuthenticationData &&_Data) override;
		TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID) override;

		TCFuture<CConnectionsDebugStats> f_GetConnectionsDebugStats() override;

	private:
		NException::CException fp_AccessDenied() const;
		bool fp_CheckPermissions(EPermission _Permissions) const;
		
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		CPermissions mp_Permissions;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
