// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include <Mib/Concurrency/DistributedActor>

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManagerInterface : public NConcurrency::CActor
	{
	public:
		enum
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};
	  
		struct CTrustTicket
		{
			enum
			{
				EVersion = 0x101
			};
			
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);
			
			NStr::CStr f_ToStringTicket() const;
			static CTrustTicket fs_FromStringTicket(NStr::CStr const &_StringTicket);

			CDistributedActorTrustManager_Address m_ServerAddress;
			NContainer::TCVector<uint8> m_ServerPublicCert;
			NStr::CStr m_Token;
		};
		
		struct CNamespacePermissions
		{
			inline NStr::CStr const &f_GetName() const;
			inline bool operator ==(CNamespacePermissions const &_Right) const;
			inline bool operator <(CNamespacePermissions const &_Right) const;
			template <typename tf_CString>
			void f_Format(tf_CString &o_String) const;

			void f_Feed(CDistributedActorWriteStream &_Stream) const;
			void f_Consume(CDistributedActorReadStream &_Stream);
			
			NContainer::TCMap<NStr::CStr, CHostInfo> m_AllowedHosts;
			NContainer::TCMap<NStr::CStr, CHostInfo> m_DisallowedHosts;
		};

		struct CTrustGenerateConnectionTicketResult
		{
			void f_Feed(CDistributedActorWriteStream &_Stream) &&;
			void f_Consume(CDistributedActorReadStream &_Stream);
			
			CTrustTicket m_Ticket;
			TCActorSubscriptionWithID<0> m_OnUseTicketSubscription;
		};

		CDistributedActorTrustManagerInterface();
		~CDistributedActorTrustManagerInterface();

		virtual TCContinuation<NStr::CStr> f_GetHostID() const = 0;

		virtual TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens() = 0;
		virtual TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients() = 0;
		virtual TCContinuation<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket
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
			) = 0
		;
		virtual TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) = 0;
		virtual TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) = 0;

		virtual TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CHostInfo>> f_EnumClientConnections() = 0;
		virtual TCContinuation<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout) = 0;
		virtual TCContinuation<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo) = 0;
		virtual TCContinuation<void> f_AllowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts) = 0;
		virtual TCContinuation<void> f_DisallowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>> f_EnumHostPermissions(bool _bIncludeHostInfo) = 0;
		virtual TCContinuation<void> f_AddHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions) = 0;
		virtual TCContinuation<void> f_RemoveHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions) = 0;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.hpp"
