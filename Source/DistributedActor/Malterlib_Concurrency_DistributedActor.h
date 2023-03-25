// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../Actor/Malterlib_Concurrency_Defines.h"
#include "../Actor/Malterlib_Concurrency_AsyncResult.h"
#include "../Actor/Malterlib_Concurrency_Promise.h"
#include "../Actor/Malterlib_Concurrency_Actor.h"
#include "../Actor/Malterlib_Concurrency_ActorHolder.h"

#include <Mib/Web/HTTP/URL>
#include <Mib/Memory/Allocators/Secure>
#include <Mib/Concurrency/WeakActor>
#include <Mib/Concurrency/ActorFunctorWeak>
#include <Mib/Cryptography/PublicCrypto>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Network/DebugFlags>

namespace NMib::NWeb
{
	struct CWebSocketNewServerConnection;
}

namespace NMib::NConcurrency
{
	struct CDistributedActorWriteStream;
	struct CDistributedActorReadStream;

	struct CActorDistributionManager;
	struct CDistributedActorPublication;
	struct CDistributedActorProtocolVersions;
	struct ICDistributedActorAuthenticationHandler;

	template <typename t_CActor>
	struct TCDistributedActorWrapper;

	template <typename t_CActor>
	struct TCIsActorAlwaysAlive<TCDistributedActorWrapper<t_CActor>> : public TCIsActorAlwaysAlive<t_CActor>
	{
	};

	template <typename t_CActor>
	struct TCIsDistributedActorWrapper
	{
		static constexpr bool mc_Value = false;
		using CActor = t_CActor;
	};

	template <typename t_CActor>
	struct TCIsDistributedActorWrapper<TCDistributedActorWrapper<t_CActor>>
	{
		static constexpr bool mc_Value = true;
		using CActor = t_CActor;
	};

	template <typename t_CActor = CActor>
	using TCDistributedActor = TCActor<TCDistributedActorWrapper<t_CActor>>;

	namespace NPrivate
	{
		struct CDistributedActorData;
		struct CDistributedActorStreamContext;

		struct CDistributedActorStreamContextState;
		struct CDistributedActorSubscriptionReferenceState;

		struct CDistributedActorInterfaceInfo;

		struct ICHost
		{
			ICHost(CDistributedActorHostInfo &&_Info);
			virtual ~ICHost();

			NStorage::CIntrusiveRefCountWithWeak m_RefCount;
			CDistributedActorHostInfo const m_HostInfo;
			NAtomic::TCAtomic<uint32> m_ActorProtocolVersion = 0;
			NAtomic::TCAtomic<bool> m_bDeleted = false;
		};

		TCFuture<CDistributedActorPublication> fg_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, CDistributedActorInterfaceInfo &&_InterfaceInfo
				, NStr::CStr const &_Namespace
			)
		;
	}

	struct CDistributedActorIdentifier;

	struct CDistributedActorIdentifier
	{
		CDistributedActorIdentifier();
		CDistributedActorIdentifier(NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost, NStr::CStr const &_ActorID);

		bool operator == (CDistributedActorIdentifier const &_Other) const = default;
		auto operator <=> (CDistributedActorIdentifier const &_Other) const = default;

		template <typename tf_CActor>
		COrdering_Weak operator <=> (TCActor<tf_CActor> const &_Right) const;
		COrdering_Weak operator <=> (TCActor<> const &_Right) const;

		template <typename t_CActor>
		bool operator == (TCActor<t_CActor> const &_Right) const;
		bool operator == (TCActor<> const &_Right) const;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

	private:

		NStorage::TCWeakPointer<NPrivate::ICHost> mp_pHost;
		NStr::CStr mp_ActorID;
	};

	struct CHostInfo
	{
		CHostInfo();
		~CHostInfo();
		CHostInfo(NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName);

		void f_Feed(CDistributedActorWriteStream &_Stream) const;
		void f_Consume(CDistributedActorReadStream &_Stream);

		NStr::CStr f_GetDesc() const;
		NStr::CStr f_GetDescColored(NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;

		bool f_IsEmpty() const;

		auto operator <=> (CHostInfo const &_Right) const = default;

		template <typename tf_CString>
		void f_Format(tf_CString &o_String) const;

		NStr::CStr m_HostID;
		NStr::CStr m_FriendlyName;
	};

	template <typename t_CActor>
	struct TCDistributedActorWrapper : public t_CActor
	{
		template <typename... tf_CActor>
		TCDistributedActorWrapper(tf_CActor &&...p_Params);
	};

	template <typename t_CActor>
	using TCWeakDistributedActor = TCWeakActor<TCDistributedActorWrapper<t_CActor>>;

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(TCActor<CActorDistributionManager> const &_DistributionManager, tfp_CParams &&...p_Params);

	template <typename tf_CActor, typename... tfp_CParams>
	TCDistributedActor<tf_CActor> fg_ConstructRemoteDistributedActor
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, NStr::CStr const &_ActorID
			, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
			, uint32 _ProtocolVersion
			, tfp_CParams &&...p_Params
		)
	;

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> &&_Actor, tfp_CParams && ...p_Params)
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CParams...>
	;

	struct CActorDistributionCryptographyRemoteServer
	{
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NContainer::CByteVector m_PublicServerCertificate;
		NContainer::CByteVector m_PublicClientCertificate;
	};

	struct CActorDistributionCryptographySettings
	{
		static NCryptography::CPublicKeySetting fs_DefaultKeySetting();

		CActorDistributionCryptographySettings(NStr::CStr const &_HostID);
		~CActorDistributionCryptographySettings();
		void f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, NCryptography::CPublicKeySetting _KeySetting = fs_DefaultKeySetting());
		NContainer::CByteVector f_GenerateRequest() const;
		NContainer::CByteVector f_SignRequest(NContainer::CByteVector const &_Request);
		void f_AddRemoteServer(NWeb::NHTTP::CURL const &_URL, NContainer::CByteVector const &_ServerCert, NContainer::CByteVector const &_ClientCert);
		NStr::CStr f_GetHostID() const;

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_HostID;

		NContainer::CSecureByteVector m_PrivateKey;
		NContainer::CByteVector m_PublicCertificate;

		NContainer::TCMap<NWeb::NHTTP::CURL, CActorDistributionCryptographyRemoteServer> m_RemoteClientCertificates;

		NContainer::TCMap<NStr::CStr, NContainer::CByteVector> m_SignedClientCertificates;

		NStr::CStr m_Subject;

		int32 m_Serial = 0;
	};

	struct CActorDistributionConnectionSettings
	{
		CActorDistributionConnectionSettings();
		~CActorDistributionConnectionSettings();

		void f_SetCryptography(CActorDistributionCryptographySettings const &_Settings);

		NWeb::NHTTP::CURL m_ServerURL;
		NContainer::CByteVector m_PublicServerCertificate;
		NContainer::CByteVector m_PublicClientCertificate;
		NContainer::CSecureByteVector m_PrivateClientKey;
		bool m_bRetryConnectOnFirstFailure = true;
		bool m_bRetryConnectOnFailure = true;
		bool m_bAllowInsecureConnection = false; // Only enabled when m_PublicServerCertificate is empty
	};

	struct CActorDistributionListenSettings
	{
		CActorDistributionListenSettings(uint16 _Port, NNetwork::ENetAddressType _AddressType = NNetwork::ENetAddressType_None);
		CActorDistributionListenSettings(NContainer::TCVector<NWeb::NHTTP::CURL> const &_Addresses);
		~CActorDistributionListenSettings();

		void f_SetCryptography(CActorDistributionCryptographySettings const &_Settings);

		NContainer::TCVector<NWeb::NHTTP::CURL> m_ListenAddresses;
		NContainer::CSecureByteVector m_PrivateKey;
		NContainer::CByteVector m_CACertificate;
		NContainer::CByteVector m_PublicCertificate;
		NNetwork::ENetFlag m_ListenFlags = NNetwork::ENetFlag_None;
		bool m_bRetryOnListenFailure = true;
	};

	struct CDistributedActorProtocolVersions
	{
		CDistributedActorProtocolVersions();
		CDistributedActorProtocolVersions(uint32 _MinSupported, uint32 _MaxSupported);

		bool f_HighestSupportedVersion(CDistributedActorProtocolVersions const &_Other, uint32 &o_Version) const;
		bool f_ValidVersion(uint32 _Version);
		auto operator <=> (CDistributedActorProtocolVersions const &_Other) const = default;

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		uint32 m_MinSupported = TCLimitsInt<uint32>::mc_Max;
		uint32 m_MaxSupported = TCLimitsInt<uint32>::mc_Max;
	};

	struct CDistributedActorInterfaceShare
	{
		CDistributedActorInterfaceShare();
		~CDistributedActorInterfaceShare();
		CDistributedActorInterfaceShare(NContainer::TCVector<uint32> const &_Hierarchy, CDistributedActorProtocolVersions const &_ProtocolVersions, TCDistributedActor<CActor> &&_Actor);

		NContainer::TCVector<uint32> m_Hierarchy;
		CDistributedActorProtocolVersions m_ProtocolVersions;
		TCDistributedActor<CActor> m_Actor;
	};

	template <typename t_CType>
	struct TCDistributedActorInterfaceShare : public CDistributedActorInterfaceShare
	{
		TCDistributedActorInterfaceShare(NContainer::TCVector<uint32> const &_Hierarchy, CDistributedActorProtocolVersions const &_ProtocolVersions, TCDistributedActor<CActor> &&_Actor);

		TCDistributedActor<t_CType> f_GetActor() const;
	};

	template <typename tf_CType>
	CDistributedActorProtocolVersions fg_SubscribeVersions(uint32 _MinSupportedVersion = 0, uint32 _MaxSupportedVersion = TCLimitsInt<uint32>::mc_Max);

	struct CAbstractDistributedActor
	{
		CAbstractDistributedActor();
		CAbstractDistributedActor
			(
				NStr::CStr const &_ActorID
				, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
				, TCWeakActor<CActorDistributionManager> const &_DistributionManager
				, NContainer::TCVector<uint32> const &_InheritanceHierarchy
				, NStr::CStr const &_UniqueHostID
				, CHostInfo const &_HostInfo
				, CDistributedActorProtocolVersions const &_ProtocolVersions
			)
		;

		TCDistributedActor<CActor> f_GetActor(uint32 _TypeHash, CDistributedActorProtocolVersions const &_SupportedVersions) const;
		template <typename tf_CType>
		TCDistributedActor<tf_CType> f_GetActor() const;
		inline NStr::CStr const &f_GetUniqueHostID() const;
		inline NStr::CStr const &f_GetRealHostID() const;
		inline CHostInfo const &f_GetHostInfo() const;
		inline CDistributedActorProtocolVersions const &f_GetProtocolVersions() const;
		NContainer::TCVector<uint32> const &f_GetTypeHashes() const;
		inline bool f_IsEmpty() const;
		CDistributedActorIdentifier f_GetIdentifier() const;

	private:
		NStorage::TCWeakPointer<NPrivate::ICHost> mp_pHost;
		NStr::CStr mp_ActorID;
		TCWeakActor<CActorDistributionManager> mp_DistributionManager;

		NContainer::TCVector<uint32> mp_InheritanceHierarchy;
		NStr::CStr mp_UniqueHostID;
		CHostInfo mp_HostInfo;
		CDistributedActorProtocolVersions mp_ProtocolVersions;
	};

	template <typename tf_CFirst>
	NContainer::TCVector<uint32> fg_GetDistributedActorInheritanceHierarchy();

	struct CDistributedActorSecurity
	{
		NContainer::TCVector<NStr::CStr> m_AllowedIncomingConnectionNamespaces;
		NContainer::TCVector<NStr::CStr> m_AllowedOutgoingConnectionNamespaces;
	};

	struct CActorDistributionManager;
	struct CDistributedActorPublication
	{
		friend struct CActorDistributionManager;
		friend struct CActorDistributionManagerInternal;

		CDistributedActorPublication();
		~CDistributedActorPublication();

		CDistributedActorPublication(CDistributedActorPublication const &) = delete;
		CDistributedActorPublication &operator = (CDistributedActorPublication const &) = delete;

		CDistributedActorPublication(CDistributedActorPublication &&);
		CDistributedActorPublication &operator = (CDistributedActorPublication &&);

		bool f_IsValid() const;
		TCFuture<void> f_Destroy();
		void f_Republish(NStr::CStr const &_HostID) const;

	private:
		CDistributedActorPublication(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID);

		TCWeakActor<CActorDistributionManager> mp_DistributionManager;
		NStr::CStr mp_Namespace;
		NStr::CStr mp_ActorID;
	};

	template <typename t_CImplementation>
	struct TCDistributedActorInstance
	{
		~TCDistributedActorInstance();

		TCFuture<void> f_Destroy();

		template <typename tf_CFirstInterface, typename ...tfp_CInterfaces, typename tf_CThis>
		TCFuture<void> f_Publish(TCActor<CActorDistributionManager> const &_DistributionManager, tf_CThis *_pThis, NStr::CStr const &_Namespace = tf_CFirstInterface::mc_pDefaultNamespace);
		template <typename tf_CThis>
		void f_Construct(TCActor<CActorDistributionManager> const &_DistributionManager, tf_CThis *_pThis);

		TCDistributedActor<t_CImplementation> m_Actor;
		CDistributedActorPublication m_Publication;
		t_CImplementation *m_pActor = nullptr;
	private:
#if DMibEnableSafeCheck > 0
		CActor *mp_pThis = nullptr;
#endif
	};

	struct CDistributedActorListenReference
	{
		friend struct CActorDistributionManager;
		friend struct CActorDistributionManagerInternal;

		CDistributedActorListenReference();
		~CDistributedActorListenReference();

		CDistributedActorListenReference(CDistributedActorListenReference const &) = delete;
		CDistributedActorListenReference &operator = (CDistributedActorListenReference const &) = delete;

		CDistributedActorListenReference(CDistributedActorListenReference &&);
		CDistributedActorListenReference &operator = (CDistributedActorListenReference &&);

		void f_Clear();
		TCFuture<void> f_Stop();
		TCFuture<void> f_Debug_BreakAllConnections(fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
		TCFuture<void> f_Debug_SetServerBroken(bool _bBroken);

	private:
		CDistributedActorListenReference(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_ListenID);

		TCWeakActor<CActorDistributionManager> mp_DistributionManager;
		NStr::CStr mp_ListenID;
	};

	struct CDistributedActorConnectionStatus
	{
		CHostInfo m_HostInfo;
		NStr::CStr m_Error;
		NTime::CTime m_ErrorTime;
		bool m_bConnected = false;
	};

	struct CDistributedActorConnectionReference
	{
		friend struct CActorDistributionManager;
		friend struct CActorDistributionManagerInternal;

		CDistributedActorConnectionReference();
		~CDistributedActorConnectionReference();

		CDistributedActorConnectionReference(CDistributedActorConnectionReference const &) = delete;
		CDistributedActorConnectionReference &operator = (CDistributedActorConnectionReference const &) = delete;

		CDistributedActorConnectionReference(CDistributedActorConnectionReference &&);
		CDistributedActorConnectionReference &operator = (CDistributedActorConnectionReference &&);

		void f_Clear();
		TCFuture<void> f_Disconnect(bool _bPreserveHost = false);
		TCFuture<void> f_Reconnect();
		TCFuture<void> f_Debug_Break(fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
		TCFuture<CDistributedActorConnectionStatus> f_GetStatus();
		TCFuture<void> f_UpdateConnectionSettings(CActorDistributionConnectionSettings const &_Settings);
		bool f_IsValid() const;

		NContainer::TCVector<NContainer::CByteVector> const &f_GetCertificateChain() const;

	private:
		CDistributedActorConnectionReference
			(
				TCWeakActor<CActorDistributionManager> const &_DistributionManager
				, NStr::CStr const &_ConnectionID
			)
		;

		TCWeakActor<CActorDistributionManager> mp_DistributionManager;
		NStr::CStr mp_ConnectionID;
	};

	struct ICActorDistributionManagerAccessHandler : public CActor
	{
		virtual TCFuture<NStr::CStr> f_ValidateClientAccess(NStr::CStr const &_HostID, NContainer::TCVector<NContainer::CByteVector> const &_CertificateChain) = 0;
	};

	struct CCallingHostInfo
	{
		CCallingHostInfo();
		CCallingHostInfo
			(
				TCActor<CActorDistributionManager> const &_DistributionManager
				, TCDistributedActor<ICDistributedActorAuthenticationHandler> const &_AuthenticationHandler
				, NStr::CStr const &_UniqueHostID
				, CHostInfo const &_HostInfo
				, NStr::CStr const &_LastExecutionID
				, uint32 _ProtocolVersion
				, NStr::CStr const &_ClaimedUserID
				, NStr::CStr const &_ClaimedUserName
				, NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
			)
		;

		NStr::CStr const &f_GetRealHostID() const;
		NStr::CStr const &f_GetUniqueHostID() const;
		CHostInfo const &f_GetHostInfo() const;
		NStr::CStr const &f_LastExecutionID() const;
		TCActor<CActorDistributionManager> f_GetDistributionManager() const;
		TCDistributedActor<ICDistributedActorAuthenticationHandler> f_GetAuthenticationHandler() const;
		TCFuture<CActorSubscription> f_OnDisconnect(TCActorFunctorWeak<TCFuture<void> ()> &&_fOnDisconnect) const;
		uint32 f_GetProtocolVersion() const;
		NStr::CStr const &f_GetClaimedUserID() const;
		NStr::CStr const &f_GetClaimedUserName() const;
		NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> f_GetHost() const;

		bool operator == (CCallingHostInfo const &_Right) const;
		COrdering_Strong operator <=> (CCallingHostInfo const &_Right) const;

	protected:
		TCWeakActor<CActorDistributionManager> mp_DistributionManager;
		TCWeakDistributedActor<ICDistributedActorAuthenticationHandler> mp_AuthenticationHandler;
		NStorage::TCWeakPointer<NPrivate::ICHost> mp_pHost;
		NStr::CStr mp_UniqueHostID; // Differs from HostID when anonymous
		CHostInfo mp_HostInfo;
		NStr::CStr mp_LastExecutionID;
		NStr::CStr mp_ClaimedUserID;
		NStr::CStr mp_ClaimedUserName;
		uint32 mp_ProtocolVersion;
	};

	struct CActorDistributionManagerInternal;

	struct CActorDistributionManagerInitSettings
	{
		CActorDistributionManagerInitSettings(NStr::CStr const &_HostID, NStr::CStr const &_Enclave, NStr::CStr const &_FriendlyName);
		CActorDistributionManagerInitSettings(NStr::CStr const &_HostID, NStr::CStr const &_Enclave);

		NStr::CStr m_HostID;
		NStr::CStr m_FriendlyName;
		NStr::CStr m_Enclave; // Hosts with the same enclave are assumed to be from the same distribution manager instance. If two use the same enclave they will disconnect each other.

		fp64 m_HostTimeout = 10.0 * 60.0; // 10 minutes
		fp64 m_HostDaemonTimeout = 4.0 * 60.0 * 60.0; // 4 hours
		bool m_bTimeoutForUnixSockets = true;
	};

	template <typename t_CInterface, typename t_CDelegateTo>
	struct TCDistributedInterfaceDelegator : public t_CInterface
	{
		using CActorHolder = CDelegatedActorHolder;
		static constexpr bool mc_bAllowInternalAccess = true;

		template <typename ...tfp_CParams>
		TCDistributedInterfaceDelegator(t_CDelegateTo *_pDelegateTo, tfp_CParams && ...p_Params);
	};

	struct CActorDistributionManagerHolder : public CDefaultActorHolder
	{
		CActorDistributionManagerHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;

		template <typename tf_CActor, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<tf_CActor>> f_ConstructActor(tfp_CParams &&...p_Params);

		template <typename tf_CActor, typename tf_CDelegateTo, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<TCDistributedInterfaceDelegator<tf_CActor, tf_CDelegateTo>>> f_ConstructInterface(tf_CDelegateTo *_pDelegateTo, tfp_CParams &&...p_Params);
	};

	struct CCallingHostInfoScope final : public CCrossActorCallStateScope
	{
		CCallingHostInfoScope(CCallingHostInfo &&_NewInfo, bool _bAddToCoroutine = true);
		CCallingHostInfoScope(CCallingHostInfoScope &&_Other) = default;
		~CCallingHostInfoScope();
		void f_Suspend() override;
		void f_Resume() override;
		NFunction::TCFunctionMovable<void (bool _bException)> f_StoreState(bool _bFromSuspend) override;
		void f_InitialSuspend() override;

	private:
		CCallingHostInfo mp_NewInfo;
		CCallingHostInfo mp_PrevInfo;
		CCallingHostInfoScope *mp_pPrevScope = nullptr;
	};

	struct CActorDistributionManager : public CActor
	{
		using CActorHolder = CActorDistributionManagerHolder;

		struct CConnectionResult
		{
			CDistributedActorConnectionReference m_ConnectionReference;
			NStr::CStr m_UniqueHostID;
			NContainer::TCVector<NContainer::CByteVector> m_CertificateChain;
			CHostInfo m_HostInfo;

			CConnectionResult(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_ConnectionID)
				: m_ConnectionReference(_DistributionManager, _ConnectionID)
			{
			}
		};

		struct CHostState
		{
			NStr::CStr m_LastConnectionError;
			NTime::CTime m_LastConnectionErrorTime;
			bool m_bActive = false;
		};

		struct CWebsocketDebugStats
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			uint64 m_nSentBytes = 0;
			uint64 m_nReceivedBytes = 0;
			uint64 m_IncomingDataBufferBytes = 0;
			uint64 m_OutgoingDataBufferBytes = 0;
			fp64 m_SecondsSinceLastSend = 0.0;
			fp64 m_SecondsSinceLastReceive = 0.0;
			uint8 m_State = 0;
		};

		struct CConnectionDebugStats
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NWeb::NHTTP::CURL m_URL;
			CWebsocketDebugStats m_WebsocketStats;
			bool m_bIncoming = false;
			bool m_bIdentified = false;
		};

		struct CDebugHostStats
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NStr::CStr m_HostID;
			NStr::CStr m_UniqueHostID;

			NContainer::TCVector<CConnectionDebugStats> m_Connections;

			uint64 m_Incoming_NextPacketID = 0;
			uint64 m_Incoming_PacketsQueueLength = 0;
			uint64 m_Incoming_PacketsQueueBytes = 0;
			NContainer::TCVector<uint64> m_Incoming_PacketsQueueIDs;

			uint64 m_Outgoing_CurrentPacketID = 0;
			uint64 m_Outgoing_PacketsQueueLength = 0;
			uint64 m_Outgoing_PacketsQueueBytes = 0;
			NContainer::TCVector<uint64> m_Outgoing_PacketsQueueIDs;

			uint64 m_Outgoing_SentPacketsQueueLength = 0;
			uint64 m_Outgoing_SentPacketsQueueBytes = 0;
			NContainer::TCVector<uint64> m_Outgoing_SentPacketsQueueIDs;

			uint64 m_nSentPackets = 0;
			uint64 m_nSentBytes = 0;

			uint64 m_nReceivedPackets = 0;
			uint64 m_nReceivedBytes = 0;

			uint64 m_nDiscardedPackets = 0;
			uint64 m_nDiscardedBytes = 0;

			NStr::CStr m_LastExecutionID;
			NStr::CStr m_ExecutionID;
			NStr::CStr m_FriendlyName;
			NStr::CStr m_LastError;
			NTime::CTime m_LastErrorTime;
		};


		struct CConnectionsDebugStats
		{
			static constexpr uint32 mc_Version = 0x101;

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDebugHostStats> m_Hosts;
		};

		CActorDistributionManager(CActorDistributionManagerInitSettings const &_InitSettings);
		~CActorDistributionManager();

		void f_SetSecurity(CDistributedActorSecurity const &_Security);
		void f_SetAccessHandler(TCActor<ICActorDistributionManagerAccessHandler> const &_AccessHandler);
		void f_SetHostnameTraslate(NContainer::TCMap<NStr::CStr, NStr::CStr> const &_TranslateHostnames);

		TCFuture<void> f_SetAuthenticationHandler
			(
				NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
				, NStr::CStr const &_LastExecutionID
				, TCDistributedActor<ICDistributedActorAuthenticationHandler> const &_AuthenticationHandler
				, NStr::CStr const &_UserID
				, NStr::CStr const &_UserName
			)
		;
		TCFuture<void> f_RemoveAuthenticationHandler
			(
				NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
				, TCWeakDistributedActor<ICDistributedActorAuthenticationHandler> const &_AuthenticationHandler
			)
		;

		TCFuture<CDistributedActorListenReference> f_Listen(CActorDistributionListenSettings const &_Settings);
		TCFuture<CConnectionResult> f_Connect(CActorDistributionConnectionSettings const &_Settings, fp64 _Timeout);

		TCFuture<NContainer::CSecureByteVector> f_CallRemote
			(
				NStorage::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
				, NContainer::CSecureByteVector &&_CallData
				, NPrivate::CDistributedActorStreamContext const &_Context
			)
		;

		TCFuture<void> f_KickHost(NStr::CStr const &_HostID);
		TCFuture<CHostState> f_GetHostState(NStr::CStr const &_UniqueHostID);

		TCFuture<CActorSubscription> f_SubscribeActors
			(
				NStr::CStr const &_NameSpace /// Leave empty to subscribe to all actors
				, TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<void (CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
				, NFunction::TCFunctionMovable<void (CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
			)
		;

		CActorSubscription f_SubscribeHostInfoChanged(TCActorFunctorWeak<TCFuture<void> (CHostInfo const &_HostInfo)> &&_fHostInfoChanged);

		TCFuture<CActorSubscription> f_RegisterWebsocketHandler
			(
				NStr::CStr const &_Path
				, TCActor<> const &_Actor
				, NFunction::TCFunctionMutable
				<
					TCFuture<void> (NStorage::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pNewServerConnection, NStr::CStr const &_RealHostID)
				>
				&&_fNewWebsocketConnection
			)
		;

		static CCallingHostInfo const &fs_GetCallingHostInfo();
		static NStr::CStr fs_GetCertificateHostID(NContainer::CByteVector const &_Certificate);
		static NStr::CStr fs_GetCertificateRequestHostID(NContainer::CByteVector const &_Certificate);
		static bool fs_IsValidNamespaceName(NStr::CStr const &_String);
		static bool fs_IsValidHostID(NStr::CStr const &_String);
		static bool fs_IsValidEnclave(NStr::CStr const &_String);
		static bool fs_IsValidUserID(NStr::CStr const &_String);

		TCFuture<CConnectionsDebugStats> f_GetConnectionsDebugStats();

		static constexpr mint mc_RemoteCallOverhead = 38;
		static constexpr mint mc_RemoteCallReturnOverhead = 19;
		static constexpr mint mc_MaxMessageSize = 24 * 1024 * 1024;
		static constexpr mint mc_HalfMaxMessageSize = mc_MaxMessageSize / 2;

		static_assert(mc_HalfMaxMessageSize > (mc_RemoteCallOverhead + 512));

	private:
		TCFuture<void> fp_Destroy() override;

		TCFuture<CDistributedActorPublication> fp_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, NStr::CStr const &_Namespace
				, NPrivate::CDistributedActorInterfaceInfo const &_InterfaceInfo
			)
		;

		void fp_RegisterRemoteSubscription
			(
				NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &_pContextState
				, NStr::CStr const &_SubscriptionID
				, uint32 _SubscriptionSequenceID
			)
		;
		TCFuture<void> fp_DestroyRemoteSubscription
			(
				NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
				, NStr::CStr const &_SubscriptionID
				, NStr::CStr const &_LastExecutionID
			)
		;
		void fp_CleanupRemoteContext(NFunction::TCFunction<void (CActorDistributionManagerInternal &_Internal)> const &_fCleanup);
		TCFuture<void> fp_RemoveListen(NStr::CStr const &_ListenID);
		TCFuture<void> fp_Debug_BreakAllListenConnections(NStr::CStr const &_ListenID, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
		TCFuture<void> fp_Debug_SetListenServerBroken(NStr::CStr const &_ListenID, bool _bBroken);

		TCFuture<void> fp_RemoveConnection(NStr::CStr const &_ConnectionID, bool _bPreserveHost);
		TCFuture<void> fp_Debug_BreakConnection(NStr::CStr const &_ConnectionID, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
		TCFuture<void> fp_UpdateConnectionSettings(NStr::CStr const &_ConnectionID, CActorDistributionConnectionSettings const &_Settings);
		void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
		void fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID);
		TCFuture<CDistributedActorConnectionStatus> fp_GetConnectionStatus(NStr::CStr const &_ConnectionID);
		TCFuture<void> fp_Reconnect(NStr::CStr const &_ConnectionID);
		CActorSubscription fp_OnRemoteDisconnect
			(
				TCActorFunctorWeak<TCFuture<void> ()> &&_fOnDisconnect
				, NStr::CStr const &_UniqueHostID
				, NStr::CStr const &_LastExecutionID
			)
		;

		friend TCFuture<CDistributedActorPublication> NPrivate::fg_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, NPrivate::CDistributedActorInterfaceInfo &&_InterfaceInfo
				, NStr::CStr const &_Namespace
			)
		;
		friend struct CDistributedActorPublication;
		friend struct CDistributedActorPublication;
		friend struct CDistributedActorListenReference;
		friend struct CDistributedActorConnectionReference;
		friend struct CDistributedActorReadStream;
		friend struct CCallingHostInfo;
		friend struct NPrivate::CDistributedActorStreamContextState;
		friend struct NPrivate::CDistributedActorSubscriptionReferenceState;

		NStorage::TCUniquePointer<CActorDistributionManagerInternal> mp_pInternal;
	};

	CCallingHostInfo const &fg_GetCallingHostInfo();
	NStr::CStr const &fg_GetCallingHostID();
	CActorDistributionManagerInitSettings fg_InitDistributionManager(CActorDistributionManagerInitSettings const &_Settings);
	TCActor<CActorDistributionManager> const &fg_GetDistributionManager();
	void fg_InitDistributedActorSystem();

	template <typename tf_CActor>
	NStr::CStr fg_GetRemoteActorID(TCDistributedActor<tf_CActor> const &_NewActor);

	template <typename tf_CActor>
	NStr::CStr fg_GetRemoteActorID(TCWeakDistributedActor<tf_CActor> const &_NewActor);

#ifndef DCompiler_MSVC_Workaround
	extern template class TCActor<CActorDistributionManager>;
	extern template auto TCActor<CActorDistributionManager>::f_InternalCallActor
		<
			&CActorDistributionManager::f_CallRemote
			, NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CSecureByteVector &&
			, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext &
		>
		(
			NMib::NStorage::TCSharedPointer<NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CSecureByteVector &&
			, NPrivate::CDistributedActorStreamContext&
		) const &
	;

	extern template auto TCActor<CActorDistributionManager>::f_InternalCallActor
		<
			&CActorDistributionManager::f_CallRemote
			, NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CSecureByteVector &&
			, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext &
		>
		(
			NMib::NStorage::TCSharedPointer<NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CSecureByteVector &&
			, NPrivate::CDistributedActorStreamContext&
		) &&
	;
#endif
}

#define DMibPublishActorFunction(d_Function) DMibConcurrencyRegisterMemberFunctionWithStreams \
	( \
		&d_Function \
		, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext \
		, NMib::NConcurrency::CDistributedActorReadStream \
		, NMib::NConcurrency::CDistributedActorWriteStream \
	)

#define DMibActorFunctionAlternateName(d_Class, d_AlternateName, d_UpToVersion) TCAlternateHashForMemberFunction \
	< \
		::NMib::fg_GetMemberFunctionHash<d_Class>(DMibStringize(d_AlternateName)) \
		, d_UpToVersion \
	>

#ifdef DMibDebug
#	define DMibDelegatedActorImplementation(d_ThisType) \
	d_ThisType *m_pThis; \
	CEmpty self;
#else
#	define DMibDelegatedActorImplementation(d_ThisType) \
	d_ThisType *m_pThis;
#endif

#ifndef DMibPNoShortCuts
#	define DPublishActorFunction DMibPublishActorFunction
#	define DDelegatedActorImplementation DMibDelegatedActorImplementation
#endif

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor_Private.h"
#include "Malterlib_Concurrency_DistributedActor_Stream.h"
#include "Malterlib_Concurrency_DistributedActor_AuthenticationHandler.h"
#include "Malterlib_Concurrency_DistributedActor.hpp"

