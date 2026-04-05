// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_DistributedTrust_Shared.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthData.h"
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Encoding/EJson>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	struct CAuthenticationActorInfo;
	class ICDistributedActorTrustManagerAuthenticationActor;
	class CDistributedActorTrustManagerInterface : public NConcurrency::CActor
	{
	public:
		enum : uint32
		{
			EProtocolVersion_Min = 0x102
			, EProtocolVersion_SupportOrderingFlags = 0x103
			, EProtocolVersion_SupportOnCertificateSigned = 0x104
			, EProtocolVersion_SupportUserAuthentication = 0x105
			, EProtocolVersion_SupportClaimedUserName = 0x106
			, EProtocolVersion_SupportDebugStatsAndPreserveHost = 0x107
			, EProtocolVersion_SupportPrimaryListen = 0x108
			, EProtocolVersion_PriorityDebugStats = 0x109
			, EProtocolVersion_Current = 0x109
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
			NContainer::CByteVector m_ServerPublicCert;
			NStr::CStrSecure m_Token;
		};

		struct CNamespacePermissions
		{
			inline NStr::CStr const &f_GetName() const;
			inline auto operator <=> (CNamespacePermissions const &_Right) const noexcept = default;

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

			auto operator <=> (CClientConnectionInfo const &_Right) const noexcept = default;

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

		struct CLocalPermissionRequirements : public CPermissionRequirements
		{
			CLocalPermissionRequirements(CPermissionRequirements &&_Other);
			CLocalPermissionRequirements(CPermissionRequirements const &_Other);

			CLocalPermissionRequirements() = default;
			CLocalPermissionRequirements(CLocalPermissionRequirements &&) = default;
			CLocalPermissionRequirements(CLocalPermissionRequirements const &) = default;

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			using CPermissionRequirements::f_Format;
			using CPermissionRequirements::operator =;
		};

		struct CAddPermissions
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CPermissionIdentifiers m_Identity;
			NContainer::TCMap<NStr::CStr, CLocalPermissionRequirements> m_Permissions;
			EDistributedActorTrustManagerOrderingFlag m_OrderingFlags = EDistributedActorTrustManagerOrderingFlag_None;
		};

		struct CRemovePermissions
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CPermissionIdentifiers m_Identity;
			NContainer::TCSet<NStr::CStr> m_Permissions;
			EDistributedActorTrustManagerOrderingFlag m_OrderingFlags = EDistributedActorTrustManagerOrderingFlag_None;
		};

		struct CUserInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CString>
			void f_Format(tf_CString &o_String) const;
			auto operator <=> (CUserInfo const &_Right) const noexcept = default;

			NStr::CStr m_UserName;
			NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> m_Metadata;
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
			using CCallingHostInfo::operator <=>;
		};

		struct CLocalAuthenticationData : public CAuthenticationData
		{
			CLocalAuthenticationData(CAuthenticationData &&_Other);
			CLocalAuthenticationData(CAuthenticationData const &_Other);

			CLocalAuthenticationData() = default;
			CLocalAuthenticationData(CLocalAuthenticationData &&) = default;
			CLocalAuthenticationData(CLocalAuthenticationData const &) = default;

			CLocalAuthenticationData &operator = (CLocalAuthenticationData &&) = default;
			CLocalAuthenticationData &operator = (CLocalAuthenticationData const &) = default;

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			using CAuthenticationData::operator =;
		};

		struct CGenerateConnectionTicket
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CDistributedActorTrustManager_Address m_Address;
			TCActorFunctorWithID
				<
					TCFuture<void>
					(
						NStr::CStr _HostID
						, CLocalCallingHostInfo _HostInfo
						, NContainer::CByteVector _CertificateRequest
					)
					, 0
				>
				m_fOnUseTicket
			;
			TCActorFunctorWithID<TCFuture<void> (NStr::CStr _HostID, CLocalCallingHostInfo _HostInfo), 0> m_fOnCertificateSigned;
		};

		struct CPermissionInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CString>
			void f_Format(tf_CString &o_String) const;

			CPermissionIdentifiers const &f_GetIdentifiers() const;

			NStr::CStr f_GetDesc() const;
			NStr::CStr f_GetDescColored(NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			bool operator == (CPermissionInfo const &_Right) const noexcept;

			NStorage::TCOptional<CHostInfo> m_HostInfo;
			NStorage::TCOptional<CUserInfo> m_UserInfo;
			NContainer::TCSet<NContainer::TCSet<NStr::CStr>> m_AuthenticationFactors;
			int64 m_MaximumAuthenticationLifetime = -1;
		};

		struct CEnumPermissionsResult
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCMap<NStr::CStr, NContainer::TCMap<CPermissionIdentifiers, CPermissionInfo>> m_Permissions;
		};

		struct CLocalAuthenticationActorInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			EAuthenticationFactorCategory m_Category;
		};

		struct CRemoveClientConnection
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CDistributedActorTrustManager_Address m_Address;
			bool m_bPreserveHost = false;
		};

		struct CConnectionsDebugStats
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			CActorDistributionManager::CConnectionsDebugStats m_DebugStats;
		};

		CDistributedActorTrustManagerInterface();
		~CDistributedActorTrustManagerInterface();

		virtual TCFuture<NStr::CStr> f_GetHostID() const = 0;

		virtual TCFuture<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens() = 0;
		virtual TCFuture<void> f_AddListen(CDistributedActorTrustManager_Address _Address) = 0;
		virtual TCFuture<void> f_RemoveListen(CDistributedActorTrustManager_Address _Address) = 0;
		virtual TCFuture<bool> f_HasListen(CDistributedActorTrustManager_Address _Address) = 0;

		virtual TCFuture<void> f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address) = 0;
		virtual TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> f_GetPrimaryListen() = 0;

		virtual TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients() = 0;
		virtual TCFuture<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket(CGenerateConnectionTicket _Command) = 0;
		virtual TCFuture<void> f_RemoveClient(NStr::CStr _HostID) = 0;
		virtual TCFuture<bool> f_HasClient(NStr::CStr _HostID) = 0;

		virtual TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections() = 0;
		virtual TCFuture<CHostInfo> f_AddClientConnection(CTrustTicket _TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCFuture<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCFuture<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency = -1) = 0;
		virtual TCFuture<void> f_RemoveClientConnection(CRemoveClientConnection _Command) = 0;
		virtual TCFuture<bool> f_HasClientConnection(CDistributedActorTrustManager_Address _Address) = 0;

		virtual TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo) = 0;
		virtual TCFuture<void> f_AllowHostsForNamespace(CChangeNamespaceHosts _Command) = 0;
		virtual TCFuture<void> f_DisallowHostsForNamespace(CChangeNamespaceHosts _Command) = 0;

		virtual TCFuture<CEnumPermissionsResult> f_EnumPermissions(bool _bIncludeHostInfo) = 0;
		virtual TCFuture<void> f_AddPermissions(CAddPermissions _Command) = 0;
		virtual TCFuture<void> f_RemovePermissions(CRemovePermissions _Command) = 0;

		virtual TCFuture<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) = 0;
		virtual TCFuture<NStorage::TCOptional<CUserInfo>> f_TryGetUser(NStr::CStr _UserID) = 0;
		virtual TCFuture<void> f_AddUser(NStr::CStr _UserID, NStr::CStr _UserName) = 0;
		virtual TCFuture<void> f_RemoveUser(NStr::CStr _UserID) = 0;
		virtual TCFuture<void> f_SetUserInfo
			(
				NStr::CStr _UserID
				, NStorage::TCOptional<NStr::CStr> _UserName
				, NContainer::TCSet<NStr::CStr> _RemoveMetadata
				, NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> _AddMetadata
			) = 0
		;

		virtual TCFuture<NContainer::TCMap<NStr::CStr, CLocalAuthenticationActorInfo>> f_EnumAuthenticationActors() const = 0;

		virtual TCFuture<NContainer::TCMap<NStr::CStr, CLocalAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr _UserID) const = 0;
		virtual TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CLocalAuthenticationData _Data) = 0;
		virtual TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CLocalAuthenticationData _Data) = 0;
		virtual TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID) = 0;

		virtual TCFuture<CConnectionsDebugStats> f_GetConnectionsDebugStats() = 0;
	};
}

DMibConcurrencyRegisterMemberFunctionAlternateHashes
	(
		NMib::NConcurrency::CDistributedActorTrustManagerInterface::f_EnumPermissions
		, DMibActorFunctionAlternateName
		(
			NMib::NConcurrency::CDistributedActorTrustManagerInterface
			, f_EnumHostPermissions
			, 0x104
		)
	)
;

DMibConcurrencyRegisterMemberFunctionAlternateHashes
	(
		NMib::NConcurrency::CDistributedActorTrustManagerInterface::f_AddPermissions
		, DMibActorFunctionAlternateName(NMib::NConcurrency::CDistributedActorTrustManagerInterface, f_AddHostPermissions, 0x104)
	)
;

DMibConcurrencyRegisterMemberFunctionAlternateHashes
	(
		NMib::NConcurrency::CDistributedActorTrustManagerInterface::f_RemovePermissions
		, DMibActorFunctionAlternateName(NMib::NConcurrency::CDistributedActorTrustManagerInterface, f_RemoveHostPermissions, 0x104)
	)
;

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedTrust_Interface.hpp"
