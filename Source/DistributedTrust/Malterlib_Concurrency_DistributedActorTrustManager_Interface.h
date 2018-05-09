// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationData.h"
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Encoding/EJSON>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManagerInterface : public NConcurrency::CActor
	{
	public:
		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x104
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
			
			NStr::CStrSecure f_ToStringTicket() const;
			static CTrustTicket fs_FromStringTicket(NStr::CStrSecure const &_StringTicket);

			CDistributedActorTrustManager_Address m_ServerAddress;
			NContainer::TCVector<uint8> m_ServerPublicCert;
			NStr::CStrSecure m_Token;
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
			TCActorSubscriptionWithID<0> m_NotificationsSubscription;
		};
		
		struct CClientConnectionInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CString>
			void f_Format(tf_CString &o_String) const;
			
			bool operator == (CClientConnectionInfo const &_Right) const;
			bool operator < (CClientConnectionInfo const &_Right) const;
			
			CHostInfo m_HostInfo;
			int32 m_ConnectionConcurrency = -1;
		};

		struct CChangeNamespaceHosts
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NStr::CStr m_Namespace;
			NContainer::TCSet<NStr::CStr> m_Hosts;
			EDistributedActorTrustManagerOrderingFlag m_OrderingFlags = EDistributedActorTrustManagerOrderingFlag_None;
		};

		struct CChangeHostPermissions
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NStr::CStr m_HostID;
			NContainer::TCSet<NStr::CStr> m_Permissions;
			EDistributedActorTrustManagerOrderingFlag m_OrderingFlags = EDistributedActorTrustManagerOrderingFlag_None;
		};

		struct CUserInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CString>
			void f_Format(tf_CString &o_String) const;

			NStr::CStr m_UserName;
			NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_Metadata;
		};

		struct CLocalCallingHostInfo : public CCallingHostInfo
		{
			CLocalCallingHostInfo(CCallingHostInfo &&_Other);
			CLocalCallingHostInfo(CCallingHostInfo const &_Other);

			CLocalCallingHostInfo() = default;
			CLocalCallingHostInfo(CLocalCallingHostInfo &&) = default;
			CLocalCallingHostInfo(CLocalCallingHostInfo const &) = default;

			CLocalCallingHostInfo &operator = (CLocalCallingHostInfo &&) = default;
			CLocalCallingHostInfo &operator = (CLocalCallingHostInfo const &) = default;

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			using CCallingHostInfo::operator =;
			using CCallingHostInfo::operator ==;
			using CCallingHostInfo::operator <;
		};

		struct CGenerateConnectionTicket
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CDistributedActorTrustManager_Address m_Address;
			TCActorFunctorWithID
				<
					TCContinuation<void>
					(
						NStr::CStr const &_HostID
						, CLocalCallingHostInfo const &_HostInfo
						, NContainer::TCVector<uint8> const &_CertificateRequest
					)
					, 0
				>
				m_fOnUseTicket
			;
			TCActorFunctorWithID<TCContinuation<void> (NStr::CStr const &_HostID, CLocalCallingHostInfo const &_HostInfo), 0> m_fOnCertificateSigned;
		};

		CDistributedActorTrustManagerInterface();
		~CDistributedActorTrustManagerInterface();

		virtual TCContinuation<NStr::CStr> f_GetHostID() const = 0;

		virtual TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens() = 0;
		virtual TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients() = 0;
		virtual TCContinuation<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket(CGenerateConnectionTicket &&_Command) = 0;
		virtual TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) = 0;
		virtual TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) = 0;

		virtual TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections() = 0;
		virtual TCContinuation<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCContinuation<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCContinuation<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo) = 0;
		virtual TCContinuation<void> f_AllowHostsForNamespace(CChangeNamespaceHosts const &_Command) = 0;
		virtual TCContinuation<void> f_DisallowHostsForNamespace(CChangeNamespaceHosts const &_Command) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>> f_EnumHostPermissions(bool _bIncludeHostInfo) = 0;
		virtual TCContinuation<void> f_AddHostPermissions(CChangeHostPermissions const &_Command) = 0;
		virtual TCContinuation<void> f_RemoveHostPermissions(CChangeHostPermissions const &_Command) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<NStorage::TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> f_TryGetUser(NStr::CStr const &_UserID) = 0;
		virtual TCContinuation<void> f_AddUser(NStr::CStr const &_UserID, NStr::CStr const &_UserName) = 0;
		virtual TCContinuation<void> f_RemoveUser(NStr::CStr const &_UserID) = 0;
		virtual TCContinuation<void> f_SetUserInfo
			(
				NStr::CStr const &_UserID
				, NStorage::TCOptional<NStr::CStr> const &_UserName
			 	, NContainer::TCSet<NStr::CStr> const &_RemoveMetadata
			 	, NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> const &_AddMetadata
			) = 0
		;

		virtual TCContinuation<NContainer::TCSet<NStr::CStr>> f_EnumAuthenticationFactors() = 0;
		virtual TCContinuation<NContainer::TCMap<NStr::CStr, NConcurrency::CAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID) = 0;
		virtual TCContinuation<void> f_AddAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data) = 0;
		virtual TCContinuation<void> f_SetAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data) = 0;
		virtual TCContinuation<void> f_RemoveAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID) = 0;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.hpp"
