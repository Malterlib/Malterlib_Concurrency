// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedTrust_Interface.h"
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include "../DistributedActor/Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

namespace NMib::NConcurrency
{
	CDistributedActorTrustManagerInterface::CDistributedActorTrustManagerInterface()
	{
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumListens);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetPrimaryListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_GetPrimaryListen);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumClients);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_GenerateConnectionTicket);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveClient);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasClient);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumClientConnections);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddAdditionalClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetClientConnectionConcurrency);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_HasClientConnection);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_GetHostID);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumNamespacePermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AllowHostsForNamespace);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_DisallowHostsForNamespace);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddPermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemovePermissions);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumUsers);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_TryGetUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveUser);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetUserInfo);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumUserAuthenticationFactors);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_EnumAuthenticationActors);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_AddUserAuthenticationFactor);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_SetUserAuthenticationFactor);
		DMibPublishActorFunction(CDistributedActorTrustManagerInterface::f_RemoveUserAuthenticationFactor);
	}

	CDistributedActorTrustManagerInterface::~CDistributedActorTrustManagerInterface() = default;

	void CDistributedActorTrustManagerInterface::CNamespacePermissions::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		_Stream << m_AllowedHosts;
		_Stream << m_DisallowedHosts;
	}

	void CDistributedActorTrustManagerInterface::CNamespacePermissions::f_Consume(CDistributedActorReadStream &_Stream)
	{
		_Stream >> m_AllowedHosts;
		_Stream >> m_DisallowedHosts;
	}

	void CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult::f_Feed(CDistributedActorWriteStream &_Stream) &&
	{
		_Stream << m_Ticket;
		_Stream << fg_Move(m_NotificationsSubscription);
	}

	void CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult::f_Consume(CDistributedActorReadStream &_Stream)
	{
		_Stream >> m_Ticket;
		_Stream >> m_NotificationsSubscription;
	}

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CClientConnectionInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostInfo;
		_Stream % m_ConnectionConcurrency;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CClientConnectionInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CChangeNamespaceHosts::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Namespace;
		_Stream % m_Hosts;
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportOrderingFlags)
			_Stream % m_OrderingFlags;
	};
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CChangeNamespaceHosts);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CAddPermissions::f_Stream(tf_CStream &_Stream)
	{
		if (_Stream.f_GetVersion() < EProtocolVersion_SupportUserAuthentication)
		{
			if constexpr (tf_CStream::mc_Direction == NStream::EStreamDirection_Consume)
			{
				NStr::CStr HostIDString;
				_Stream >> HostIDString;

				NStr::CStr HostID = fg_GetStrSep(HostIDString, "-");
				NStr::CStr UserID = HostIDString;
				m_Identity = CPermissionIdentifiers(HostID, UserID);

				NContainer::TCSet<NStr::CStr> Permissions;
				_Stream >> Permissions;
				m_Permissions.f_Clear();
				for (auto &Permission : Permissions)
					m_Permissions[Permission];
			}
			else
			{
				NStr::CStr HostID = m_Identity.f_GetHostID();
				if (auto &UserID = m_Identity.f_GetUserID())
				{
					// Intentionally create invalid hostid that can't be understood by remote, but does transfer back correctly
					HostID += "-";
					HostID += UserID;
				}
				_Stream << HostID;

				NContainer::TCSet<NStr::CStr> Permissions;
				for (auto &Permission : m_Permissions)
					Permissions[m_Permissions.fs_GetKey(Permission)];

				_Stream << Permissions;
			}
		}
		else
		{
			_Stream % m_Identity;
			_Stream % m_Permissions;
		}

		_Stream % m_OrderingFlags;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CAddPermissions);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CRemovePermissions::f_Stream(tf_CStream &_Stream)
	{
		if (_Stream.f_GetVersion() < EProtocolVersion_SupportUserAuthentication)
		{
			if constexpr (tf_CStream::mc_Direction == NStream::EStreamDirection_Consume)
			{
				NStr::CStr HostIDString;
				_Stream >> HostIDString;

				NStr::CStr HostID = fg_GetStrSep(HostIDString, "-");
				NStr::CStr UserID = HostIDString;
				m_Identity = CPermissionIdentifiers(HostID, UserID);
			}
			else
			{
				NStr::CStr HostID = m_Identity.f_GetHostID();
				if (auto &UserID = m_Identity.f_GetUserID())
				{
					// Intentionally create invalid hostid that can't be understood by remote, but does transfer back correctly
					HostID += "-";
					HostID += UserID;
				}
				_Stream << HostID;
			}
		}
		else
			_Stream % m_Identity;
		_Stream % m_Permissions;
		_Stream % m_OrderingFlags;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CRemovePermissions);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CGenerateConnectionTicket::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Address;
		_Stream % fg_Move(m_fOnUseTicket);
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportOnCertificateSigned)
			_Stream % fg_Move(m_fOnCertificateSigned);
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CGenerateConnectionTicket);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CUserInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UserName;
		_Stream % m_Metadata;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo, NStream::CBinaryStreamMemory<>, Feed);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CUserInfo, NStream::CBinaryStreamMemoryPtr<>, Consume);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalCallingHostInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % mp_UniqueHostID;
		_Stream % mp_HostInfo;
		_Stream % mp_LastExecutionID;
		_Stream % mp_ProtocolVersion;
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportUserAuthentication)
			_Stream % mp_ClaimedUserID;
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportClaimedUserName)
			_Stream % mp_ClaimedUserName;
		if constexpr (tf_CStream::mc_bConsume)
			mp_DistributionManager = _Stream.f_GetState().m_DistributionManager;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalCallingHostInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalAuthenticationData::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Category;
		_Stream % m_Name;
		_Stream % m_PublicData;
		_Stream % m_PrivateData;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData, NStream::CBinaryStreamMemory<>, Feed);
	DMibStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationData, NStream::CBinaryStreamMemoryPtr<>, Consume);

	CPermissionIdentifiers const &CDistributedActorTrustManagerInterface::CPermissionInfo::f_GetIdentifiers() const
	{
		return NContainer::TCMap<CPermissionIdentifiers, CPermissionInfo>::fs_GetKey(*this);
	}

	NStr::CStr CDistributedActorTrustManagerInterface::CPermissionInfo::f_GetDesc() const
	{
		using namespace NStr;
		CStr Result;
		auto &Identifiers = f_GetIdentifiers();

		if (m_HostInfo)
			Result += "Host: {}"_f << m_HostInfo->f_GetDesc();
		else if (auto HostID = Identifiers.f_GetHostID())
			Result += "Host: Invalid host with ID {}"_f << HostID;

		if (m_UserInfo)
			fg_AddStrSep(Result, "User: {} [{}]"_f << Identifiers.f_GetUserID() << m_UserInfo->m_UserName, "\n");
		else if (auto UserID = Identifiers.f_GetHostID())
			fg_AddStrSep(Result, "User: Invalid user with ID {}"_f << UserID, "\n");

		if (!m_AuthenticationFactors.f_IsEmpty())
		{
			fg_AddStrSep(Result, "Authentication Factors:", "\n");
			for (auto const &Factors : m_AuthenticationFactors)
				fg_AddStrSep(Result, "{vs}"_f << Factors, " ");

			if (m_MaximumAuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet)
				fg_AddStrSep(Result, "Authentication Lifetime: Infinite (determined by signed expire time)"_f << m_MaximumAuthenticationLifetime, "\n");
			else if (m_MaximumAuthenticationLifetime != CPermissionRequirements::mc_DefaultMaximumLifetime)
				fg_AddStrSep(Result, "\nAuthentication Lifetime: {}"_f << m_MaximumAuthenticationLifetime, "\n");
		}

		return Result;
	}

	NStr::CStr CDistributedActorTrustManagerInterface::CPermissionInfo::f_GetDescColored(NCommandLine::EAnsiEncodingFlag _AnsiFlags) const
	{
		using namespace NStr;
		CStr Result;

		NCommandLine::CAnsiEncoding AnsiEncoding(_AnsiFlags);

		auto &Identifiers = f_GetIdentifiers();

		if (m_HostInfo)
			Result += "{}Host{}: {}"_f << AnsiEncoding.f_Bold() << AnsiEncoding.f_Default() << m_HostInfo->f_GetDescColored(_AnsiFlags);
		else if (auto HostID = Identifiers.f_GetHostID())
			Result += "{}Host{}: Invalid host with ID {}"_f << AnsiEncoding.f_Bold() << AnsiEncoding.f_Default() << HostID;

		if (m_UserInfo)
		{
			fg_AddStrSep
				(
					Result
					, "{}User{}: {} [{}{}{}]"_f
					<< AnsiEncoding.f_Bold()
					<< AnsiEncoding.f_Default()
					<< Identifiers.f_GetUserID()
					<< AnsiEncoding.f_Prompt()
					<< m_UserInfo->m_UserName
					<< AnsiEncoding.f_Default()
					, "\n"
				)
			;
		}
		else if (auto UserID = Identifiers.f_GetUserID())
			fg_AddStrSep(Result, "{}User{}: Invalid user with ID {}"_f << AnsiEncoding.f_Bold() << AnsiEncoding.f_Default() << UserID, "\n");

		if (!m_AuthenticationFactors.f_IsEmpty())
		{
			fg_AddStrSep(Result, "{}Authentication Factors{}:"_f << AnsiEncoding.f_Bold() << AnsiEncoding.f_Default(), "\n");
			for (auto const &Factors : m_AuthenticationFactors)
				fg_AddStrSep(Result, "{vs}"_f << Factors, " ");

			if (m_MaximumAuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet)
			{
				fg_AddStrSep
					(
						Result
						, " {}Authentication Lifetime{}: Infinite (determined by signed expire time)"_f
						<< AnsiEncoding.f_Bold()
						<< AnsiEncoding.f_Default()
						<< m_MaximumAuthenticationLifetime
						, "\n"
					)
				;
			}
			else if (m_MaximumAuthenticationLifetime != CPermissionRequirements::mc_DefaultMaximumLifetime)
			{
				fg_AddStrSep(Result, "{}Authentication Lifetime{}: {}"_f << AnsiEncoding.f_Bold() << AnsiEncoding.f_Default() << m_MaximumAuthenticationLifetime, "\n");
			}
		}

		return Result;
	}

	bool CDistributedActorTrustManagerInterface::CPermissionInfo::operator == (CPermissionInfo const &_Right) const noexcept
	{
		return NStorage::fg_TupleReferences(m_HostInfo, m_UserInfo) == NStorage::fg_TupleReferences(_Right.m_HostInfo, _Right.m_UserInfo);
	}

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CPermissionInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostInfo;
		_Stream % m_UserInfo;
		_Stream % m_AuthenticationFactors;
		_Stream % m_MaximumAuthenticationLifetime;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CPermissionInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalPermissionRequirements::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_AuthenticationFactors;
		_Stream % m_MaximumAuthenticationLifetime;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalPermissionRequirements);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CEnumPermissionsResult::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Permissions;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CEnumPermissionsResult);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CLocalAuthenticationActorInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Category;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CLocalAuthenticationActorInfo);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CRemoveClientConnection::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Address;
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportDebugStatsAndPreserveHost)
			_Stream % m_bPreserveHost;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CRemoveClientConnection);

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CConnectionsDebugStats::f_Stream(tf_CStream &_Stream)
	{
		static_assert(CActorDistributionManager::CConnectionsDebugStats::mc_Version == CActorDistributionManager::CConnectionsDebugStats::EVersion::mc_AddPrioritization); // Implement mapping

		uint32 DebugStatsVersion = 0;
		if (_Stream.f_GetVersion() >= EProtocolVersion_PriorityDebugStats)
			DebugStatsVersion = (uint32)CActorDistributionManager::CConnectionsDebugStats::EVersion::mc_AddPrioritization;
		else if (_Stream.f_GetVersion() >= EProtocolVersion_SupportDebugStatsAndPreserveHost)
			DebugStatsVersion = (uint32)CActorDistributionManager::CConnectionsDebugStats::EVersion::mc_Initial;

		DMibBinaryStreamVersion(_Stream, DebugStatsVersion);
		_Stream % m_DebugStats;
	}
	DMibDistributedStreamImplement(CDistributedActorTrustManagerInterface::CConnectionsDebugStats);
}
