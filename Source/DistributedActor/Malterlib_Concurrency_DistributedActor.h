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
#include <Mib/Network/SSLKeySetting>
#include <Mib/Web/WebSocket>

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

	template <typename t_CActor = CActor>
	using TCDistributedActor = TCActor<TCDistributedActorWrapper<t_CActor>>;

	namespace NPrivate
	{
		struct CDistributedActorData;
		struct CDistributedActorStreamContext;

		struct CDistributedActorStreamContextState;
		struct CDistributedActorSubscriptionReferenceState;

		struct CDistributedActorInterfaceInfo;

		struct ICHost : public NStorage::TCSharedPointerIntrusiveBase<NStorage::ESharedPointerOption_SupportWeakPointer>
		{
			virtual ~ICHost();

			NAtomic::TCAtomic<uint32> m_ActorProtocolVersion = 0;
			bool m_bDeleted = false;
		};

		TCDispatchedActorCall<CDistributedActorPublication> fg_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, CDistributedActorInterfaceInfo &&_InterfaceInfo
				, NStr::CStr const &_Namespace
			)
		;
	}

	struct CDistributedActorIdentifier;

	bool operator == (CDistributedActorIdentifier const &_Left, TCActor<> const &_Right);
	bool operator == (TCActor<> const &_Left, CDistributedActorIdentifier const &_Right);

	template <typename t_CActor>
	bool operator == (CDistributedActorIdentifier const &_Left, TCActor<t_CActor> const &_Right);
	template <typename t_CActor>
	bool operator == (TCActor<t_CActor> const &_Left, CDistributedActorIdentifier const &_Right);

	struct CDistributedActorIdentifier
	{
		CDistributedActorIdentifier();
		CDistributedActorIdentifier(NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost, NStr::CStr const &_ActorID);

		bool operator == (CDistributedActorIdentifier const &_Other) const;
		bool operator < (CDistributedActorIdentifier const &_Other) const;

	private:
		template <typename t_CActor>
		friend bool operator == (CDistributedActorIdentifier const &_Left, TCActor<t_CActor> const &_Right);
		template <typename t_CActor>
		friend bool operator == (TCActor<t_CActor> const &_Left, CDistributedActorIdentifier const &_Right);
		friend bool operator == (CDistributedActorIdentifier const &_Left, TCActor<> const &_Right);
		friend bool operator == (TCActor<> const &_Left, CDistributedActorIdentifier const &_Right);

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

		bool operator ==(CHostInfo const &_Right) const;
		bool operator <(CHostInfo const &_Right) const;

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
		typename tf_CMemberFunction
		, tf_CMemberFunction t_pMemberFunction
		, uint32 t_NameHash
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(tf_CActor const &_Actor, tfp_CParams && ...p_Params)
	{
		return _Actor(t_pMemberFunction, fg_Forward<tfp_CParams>(p_Params)...);
	}

	template
	<
		typename tf_CMemberFunction
		, tf_CMemberFunction t_pMemberFunction
		, uint32 t_NameHash
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params);

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
		static NNetwork::CSSLKeySetting fs_DefaultKeySetting();

		CActorDistributionCryptographySettings(NStr::CStr const &_HostID);
		~CActorDistributionCryptographySettings();
		void f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, NNetwork::CSSLKeySetting _KeySetting = fs_DefaultKeySetting());
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
		bool operator == (CDistributedActorProtocolVersions const &_Other) const;
		bool operator < (CDistributedActorProtocolVersions const &_Other) const;

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
	CDistributedActorProtocolVersions fg_SubscribeVersions();

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
				, CDistributedActorProtocolVersions &_ProtocolVersions
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
		auto f_Stop() ->
			TCActorCall
			<
				TCActor<CConcurrentActor>
				, TCFuture<void> (CActor::*)(NFunction::TCFunctionMovable<TCFuture<void> ()> &&)
				, NStorage::TCTuple<NFunction::TCFunctionMovable<TCFuture<void> ()>>
				, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCFuture<void> ()>>
				, false
			>
		;

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
		TCDispatchedActorCall<void> f_Disconnect();
		TCDispatchedActorCall<CDistributedActorConnectionStatus> f_GetStatus();
		TCDispatchedActorCall<void> f_UpdateConnectionSettings(CActorDistributionConnectionSettings const &_Settings);
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
		TCDispatchedActorCall<CActorSubscription> f_OnDisconnect(TCActor<CActor> const &_Actor, NFunction::TCFunctionMovable<void ()> &&_fOnDisconnect) const;
		uint32 f_GetProtocolVersion() const;
		NStr::CStr const &f_GetClaimedUserID() const;
		NStr::CStr const &f_GetClaimedUserName() const;
		NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> f_GetHost() const;

		bool operator == (CCallingHostInfo const &_Right) const;
		bool operator < (CCallingHostInfo const &_Right) const;

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

	struct CCallingHostInfoScope final : public CCoroutineThreadLocalHandler
	{
		CCallingHostInfoScope(CCallingHostInfo &&_NewInfo);
		~CCallingHostInfoScope();
		void f_Suspend() override;
		void f_Resume() override;

	private:
		CCallingHostInfo mp_NewInfo;
		CCallingHostInfo mp_PrevInfo;
	};

	struct CActorDistributionManager : public CActor
	{
		using CActorHolder = CActorDistributionManagerHolder;

		struct CConnectionResult
		{
			CDistributedActorConnectionReference m_ConnectionReference;
			NStr::CStr m_UniqueHostID;
			NStr::CStr m_RealHostID;
			NContainer::TCVector<NContainer::CByteVector> m_CertificateChain;
			CHostInfo m_HostInfo;

			CConnectionResult(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_ConnectionID)
				: m_ConnectionReference(_DistributionManager, _ConnectionID)
			{
			}
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
		TCFuture<CConnectionResult> f_Connect(CActorDistributionConnectionSettings const &_Settings);

		TCFuture<NContainer::CSecureByteVector> f_CallRemote
			(
				NStorage::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
				, NContainer::CSecureByteVector &&_CallData
				, NPrivate::CDistributedActorStreamContext const &_Context
			)
		;

		TCFuture<void> f_KickHost(NStr::CStr const &_HostID);

		TCFuture<CActorSubscription> f_SubscribeActors
			(
				NContainer::TCVector<NStr::CStr> const &_NameSpaces /// Leave empty to subscribe to all actors
				, TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<void (CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
				, NFunction::TCFunctionMovable<void (CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
			)
		;

		CActorSubscription f_SubscribeHostInfoChanged
			(
				TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<void (CHostInfo const &_HostInfo)> &&_fHostInfoChanged
			)
		;

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
		void fp_RemoveConnection(NStr::CStr const &_ConnectionID);
		TCFuture<void> fp_UpdateConnectionSettings(NStr::CStr const &_ConnectionID, CActorDistributionConnectionSettings const &_Settings);
		void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
		void fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID);
		TCFuture<CDistributedActorConnectionStatus> fp_GetConnectionStatus(NStr::CStr const &_ConnectionID);
		CActorSubscription fp_OnRemoteDisconnect
			(
				TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<void ()> &&_fOnDisconnect
				, NStr::CStr const &_UniqueHostID
				, NStr::CStr const &_LastExecutionID
			)
		;

		friend TCDispatchedActorCall<CDistributedActorPublication> NPrivate::fg_PublishActor
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
}

#define DMibPublishActorFunction(d_Function) DMibConcurrencyRegisterMemberFunctionWithStreams \
	( \
		decltype(&d_Function) \
		, &d_Function \
		, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
		, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext \
		, NMib::NConcurrency::CDistributedActorReadStream \
		, NMib::NConcurrency::CDistributedActorWriteStream \
	)

#define DMibActorFunctionAlternateName(d_Function, d_AlternateName, d_UpToVersion) TCAlternateHashForMemberFunction \
	< \
		::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_AlternateName)) \
		, d_UpToVersion \
	>

#define DMibCallActor(d_Actor, d_Function, ...) ::NMib::NConcurrency::fg_CallActor \
	< \
		decltype(&d_Function) \
		, &d_Function \
		, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
	> \
	(d_Actor, ##__VA_ARGS__)

#ifndef DMibPNoShortCuts
#	define DCallActor DMibCallActor
#	define DPublishActorFunction DMibPublishActorFunction
#endif

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor_Private.h"
#include "Malterlib_Concurrency_DistributedActor_Stream.h"
#include "Malterlib_Concurrency_DistributedActor_AuthenticationHandler.h"
#include "Malterlib_Concurrency_DistributedActor.hpp"

