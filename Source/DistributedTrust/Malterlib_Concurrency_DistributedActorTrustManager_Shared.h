// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Web/HTTP/URL>

namespace NMib::NConcurrency
{
	enum EDistributedActorTrustManagerOrderingFlag
	{
		EDistributedActorTrustManagerOrderingFlag_None = 0
		, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions = DMibBit(1)
	};

	struct CDistributedActorTrustManager_Address
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		bool operator == (CDistributedActorTrustManager_Address const &_Right) const;
		bool operator < (CDistributedActorTrustManager_Address const &_Right) const;
		
		NWeb::NHTTP::CURL m_URL;
		
		CDistributedActorTrustManager_Address();
		~CDistributedActorTrustManager_Address();
		CDistributedActorTrustManager_Address(NWeb::NHTTP::CURL const &_URL);
		CDistributedActorTrustManager_Address(NWeb::NHTTP::CURL &&_URL);
		
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_IntoStr) const;
	};

	enum EPermissionIdentifierType
	{
		EPermissionIdentifierType_None
		, EPermissionIdentifierType_HostID
		, EPermissionIdentifierType_UserID
	};

	struct CPermissionIdentifier
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		bool operator < (CPermissionIdentifier const &_Right) const;
		bool operator == (CPermissionIdentifier const &_Right) const;

		EPermissionIdentifierType m_Type = EPermissionIdentifierType_None;
		NStr::CStr m_ID;
	};

	struct CPermissionIdentifiers
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		bool operator == (CPermissionIdentifiers const &_Right) const;
		bool operator < (CPermissionIdentifiers const &_Right) const;

		CPermissionIdentifiers();
		CPermissionIdentifiers(NStr::CStr const &_HostID, NStr::CStr const &_UserID);

		// This "constructor" handles the "legacy" cases where we have only a host id as well as the host id + user id from database filename
		static CPermissionIdentifiers fs_GetKeyFromFileName(NStr::CStr const &_IDs);
		static CPermissionIdentifiers fs_GetKeyFromFileNameOld(NStr::CStr const &_IDs);
		static NContainer::TCVector<CPermissionIdentifiers> fs_GetPowerSet(NStr::CStr const &_HostID, NStr::CStr const &_UserID);
		static NContainer::TCVector<CPermissionIdentifiers> fs_GetPowerSet(CPermissionIdentifiers const &_Identify);

		NStr::CStr f_GetStr() const;
		NStr::CStr const &f_GetHostID() const;
		NStr::CStr const &f_GetUserID() const;

		NContainer::TCSet<CPermissionIdentifier> m_IDs;
		static NStr::CStr const ms_EmptyID;
	};

	struct CPermissionRequirements
	{
		static constexpr int64 mc_DefaultMaximumLifetime = 15;
		static constexpr int64 mc_OverrideLifetimeNotSet = -1;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		bool operator == (CPermissionRequirements const &_Right) const;

		NContainer::TCSet<NContainer::TCSet<NStr::CStr>> m_AuthenticationFactors;
		int64 m_MaximumAuthenticationLifetime = mc_OverrideLifetimeNotSet;
	};
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.hpp"
