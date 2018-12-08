// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyDefines>
#include <Mib/Encoding/EJSON>
#include "../Actor/Malterlib_Concurrency_Actor.h"
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationData.h"

namespace NMib::NConcurrency::NDistributedActorTrustManagerDatabase
{
	struct CBasicConfig
	{
		enum
		{
			EVersion = 0x102
			, EConversionVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_HostID;
		NContainer::CSecureByteVector m_CAPrivateKey;
		NContainer::CByteVector m_CACertificate;

		uint32 m_Version = EConversionVersion;
	};

	struct CServerCertificate
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::CSecureByteVector m_PrivateKey;
		NContainer::CByteVector m_PublicCertificate;
	};

	struct CListenConfig
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		inline_always bool operator == (CListenConfig const &_Right) const;
		inline_always bool operator < (CListenConfig const &_Right) const;

		CDistributedActorTrustManager_Address m_Address;
	};

	struct CClient
	{
		enum
		{
			EVersion = 0x102
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::CByteVector m_PublicCertificate;
		NStr::CStr m_LastFriendlyName;
	};

	struct CClientConnection
	{
		enum
		{
			EVersion = 0x103
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		CHostInfo f_GetHostInfo() const;
		int32 f_GetEffectiveConnectionConcurrency(int32 _DefaultConcurrency) const;

		NContainer::CByteVector m_PublicServerCertificate;
		NContainer::CByteVector m_PublicClientCertificate;
		NStr::CStr m_LastFriendlyName;
		int32 m_ConnectionConcurrency = -1;
	};

	struct CNamespace
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::TCSet<NStr::CStr> m_AllowedHosts;
	};

	struct CPermissions
	{
		enum
		{
			EVersion = 0x102
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::TCMap<NStr::CStr, CPermissionRequirements> m_Permissions;
	};

	struct CUserInfo
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_UserName;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_Metadata;
	};

	struct CDefaultUser
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_UserID;
	};

	struct CUserAuthenticationFactor
	{
		enum
		{
			EVersion = 0x101
		};

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		CUserAuthenticationFactor const &operator = (NConcurrency::CAuthenticationData const &_Data);

		EAuthenticationFactorCategory m_Category;
		NStr::CStr m_Name;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_PublicData;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_PrivateData;
	};
}

namespace NMib::NConcurrency
{
	class ICDistributedActorTrustManagerDatabase : public NConcurrency::CActor
	{
	public:
		using CBasicConfig = NDistributedActorTrustManagerDatabase::CBasicConfig;
		using CServerCertificate = NDistributedActorTrustManagerDatabase::CServerCertificate;
		using CListenConfig = NDistributedActorTrustManagerDatabase::CListenConfig;
		using CClient = NDistributedActorTrustManagerDatabase::CClient;
		using CClientConnection = NDistributedActorTrustManagerDatabase::CClientConnection;
		using CNamespace = NDistributedActorTrustManagerDatabase::CNamespace;
		using CPermissions = NDistributedActorTrustManagerDatabase::CPermissions;
		using CUserInfo = NDistributedActorTrustManagerDatabase::CUserInfo;
		using CDefaultUser = NDistributedActorTrustManagerDatabase::CDefaultUser;
		using CUserAuthenticationFactor = NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor;

		virtual TCContinuation<CBasicConfig> f_GetBasicConfig() = 0;
		virtual TCContinuation<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) = 0;
		virtual TCContinuation<int32> f_GetNewCertificateSerial() = 0;
		virtual TCContinuation<CDefaultUser> f_GetDefaultUser() = 0;
		virtual TCContinuation<void> f_SetDefaultUser(CDefaultUser const &_DefaultUser) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>> f_EnumServerCertificates(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) = 0;
		virtual TCContinuation<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) = 0;
		virtual TCContinuation<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) = 0;
		virtual TCContinuation<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) = 0;

		virtual TCContinuation<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() = 0;
		virtual TCContinuation<void> f_AddListenConfig(CListenConfig const &_Config) = 0;
		virtual TCContinuation<void> f_RemoveListenConfig(CListenConfig const &_Config) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CClient>> f_EnumClients(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CClient> f_GetClient(NStr::CStr const &_HostID) = 0;
		virtual TCContinuation<NStorage::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) = 0;
		virtual TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) = 0;
		virtual TCContinuation<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) = 0;
		virtual TCContinuation<void> f_SetClient(NStr::CStr const &_HostID, CClient const &_Client) = 0;
		virtual TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) = 0;

		virtual TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> f_EnumClientConnections(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;
		virtual TCContinuation<void> f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) = 0;
		virtual TCContinuation<void> f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) = 0;
		virtual TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CNamespace>> f_EnumNamespaces(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CNamespace> f_GetNamespace(NStr::CStr const &_NamespaceName) = 0;
		virtual TCContinuation<void> f_AddNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) = 0;
		virtual TCContinuation<void> f_SetNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace) = 0;
		virtual TCContinuation<void> f_RemoveNamespace(NStr::CStr const &_NamespaceName) = 0;

		virtual TCContinuation<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> f_EnumPermissions(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CPermissions> f_GetPermissions(CPermissionIdentifiers const &_Identity) = 0;
		virtual TCContinuation<void> f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) = 0;
		virtual TCContinuation<void> f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions) = 0;
		virtual TCContinuation<void> f_RemovePermissions(CPermissionIdentifiers const &_Identity) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<CUserInfo> f_GetUserInfo(NStr::CStr const &_User) = 0;
		virtual TCContinuation<void> f_AddUser(NStr::CStr const &_User, CUserInfo const &_UserInfo) = 0;
		virtual TCContinuation<void> f_SetUserInfo(NStr::CStr const &_User, CUserInfo const &_UserInfo) = 0;
		virtual TCContinuation<void> f_RemoveUser(NStr::CStr const &_User) = 0;

		virtual TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> f_EnumAuthenticationFactor(bool _bIncludeFullInfo) = 0;
		virtual TCContinuation<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CUserAuthenticationFactor const &_Factor) = 0;
		virtual TCContinuation<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CUserAuthenticationFactor const &_Factor) = 0;
		virtual TCContinuation<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID) = 0;
	};
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.hpp"
