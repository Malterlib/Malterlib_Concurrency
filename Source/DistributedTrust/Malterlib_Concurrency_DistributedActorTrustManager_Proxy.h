// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

namespace NMib::NConcurrency
{
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
			, EPermission_Listen = EPermission_Listen_Read | EPermission_Listen_Add | EPermission_Listen_Remove  

			, EPermission_Client_Read = DMibBit(4)
			, EPermission_Client_Add = DMibBit(5)
			, EPermission_Client_Remove = DMibBit(6)
			, EPermission_Client = EPermission_Client_Read | EPermission_Client_Add | EPermission_Client_Remove 

			, EPermission_ClientConnection_Read = DMibBit(7)
			, EPermission_ClientConnection_Write = DMibBit(8)
			, EPermission_ClientConnection_Remove = DMibBit(9)
			, EPermission_ClientConnection = EPermission_ClientConnection_Read | EPermission_ClientConnection_Write | EPermission_ClientConnection_Remove 

			, EPermission_NamespacePermissions_Read = DMibBit(10)
			, EPermission_NamespacePermissions_Add = DMibBit(11)
			, EPermission_NamespacePermissions_Remove = DMibBit(12)
			, EPermission_NamespacePermissions = EPermission_NamespacePermissions_Read | EPermission_NamespacePermissions_Add | EPermission_NamespacePermissions_Remove

			, EPermission_HostPermissions_Read = DMibBit(13)
			, EPermission_HostPermissions_Add = DMibBit(14)
			, EPermission_HostPermissions_Remove = DMibBit(15)
			, EPermission_HostPermissions = EPermission_HostPermissions_Read | EPermission_HostPermissions_Add | EPermission_HostPermissions_Remove
			
			, EPermission_All = 
			(
				EPermission_HostID_Read 
				| EPermission_Listen 
				| EPermission_Client 
				| EPermission_ClientConnection 
				| EPermission_NamespacePermissions 
				| EPermission_HostPermissions
			)
		};
		
		struct CPermissions
		{
			EPermission m_Permissions = EPermission_None;
			NContainer::TCSet<NStr::CStr> m_AllowedNamespaces; // If empty all namespaces are allowed
			NContainer::TCSet<NStr::CStr> m_AllowedPermissions; // If empty all permissions are allowed
			NContainer::TCSet<NStr::CStr> m_AllowedHosts; // If empty all hosts are allowed
		};
		
		CDistributedActorTrustManagerProxy(TCActor<CDistributedActorTrustManager> const &_Actor, CPermissions const &_Permissions);
		~CDistributedActorTrustManagerProxy();

		TCContinuation<NStr::CStr> f_GetHostID() const override;

		TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens() override;
		TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address) override;
		TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address) override;
		TCContinuation<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address) override;

		TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients() override;
		TCContinuation<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket
			(
				CDistributedActorTrustManager_Address const &_Address
				, TCActorFunctorWithID
				<
					TCContinuation<void> 
					(
						NStr::CStr const &_HostID
						, CCallingHostInfo const &_HostInfo
						, NContainer::TCVector<uint8> const &_CertificateRequest
					)
					, 0
				> 
				&&_fOnUseTicket
			) override
		;
		TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) override;
		TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) override;

		TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections() override;
		TCContinuation<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1) override;
		TCContinuation<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) override;
		TCContinuation<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) override;
		TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) override;
		TCContinuation<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address) override;

		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo) override;
		TCContinuation<void> f_AllowHostsForNamespace(CChangeNamespaceHosts const &_Command) override;
		TCContinuation<void> f_DisallowHostsForNamespace(CChangeNamespaceHosts const &_Command) override;

		TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>> f_EnumHostPermissions(bool _bIncludeHostInfo) override;
		TCContinuation<void> f_AddHostPermissions(CChangeHostPermissions const &_Command) override;
		TCContinuation<void> f_RemoveHostPermissions(CChangeHostPermissions const &_Command) override;

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
