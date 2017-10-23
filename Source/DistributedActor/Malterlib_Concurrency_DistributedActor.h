// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../Actor/Malterlib_Concurrency_Defines.h"
#include "../Actor/Malterlib_Concurrency_AsyncResult.h"
#include "../Actor/Malterlib_Concurrency_Continuation.h"
#include "../Actor/Malterlib_Concurrency_Actor.h"
#include "../Actor/Malterlib_Concurrency_ActorHolder.h"

#include <Mib/Web/HTTP/URL>
#include <Mib/Memory/Allocators/Secure>
#include <Mib/Concurrency/WeakActor>
#include <Mib/Network/SSLKeySetting>
#include <Mib/Web/WebSocket>

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedActorWriteStream;
		struct CDistributedActorReadStream;
		
		struct CActorDistributionManager;
		struct CDistributedActorPublication;
		struct CDistributedActorProtocolVersions;
		
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
			
			struct ICHost : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
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
			CDistributedActorIdentifier(NPtr::TCWeakPointer<NPrivate::ICHost> const &_pHost, NStr::CStr const &_ActorID);
			
			bool operator == (CDistributedActorIdentifier const &_Other) const;
			bool operator < (CDistributedActorIdentifier const &_Other) const;
			
		private:
			template <typename t_CActor>
			friend bool operator == (CDistributedActorIdentifier const &_Left, TCActor<t_CActor> const &_Right);
			template <typename t_CActor>
			friend bool operator == (TCActor<t_CActor> const &_Left, CDistributedActorIdentifier const &_Right);
			friend bool operator == (CDistributedActorIdentifier const &_Left, TCActor<> const &_Right);
			friend bool operator == (TCActor<> const &_Left, CDistributedActorIdentifier const &_Right);
			
			NPtr::TCWeakPointer<NPrivate::ICHost> mp_pHost;
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
				, NPtr::TCWeakPointer<NPrivate::ICHost> const &_pHost
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
			
			NContainer::TCVector<uint8> m_PublicServerCertificate;
			NContainer::TCVector<uint8> m_PublicClientCertificate;
		};
		
		struct CActorDistributionCryptographySettings
		{
			static NNet::CSSLKeySetting fs_DefaultKeySetting(); 
			
			CActorDistributionCryptographySettings(NStr::CStr const &_HostID);
			~CActorDistributionCryptographySettings();
			void f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, NNet::CSSLKeySetting _KeySetting = fs_DefaultKeySetting());
			NContainer::TCVector<uint8> f_GenerateRequest() const;
			NContainer::TCVector<uint8> f_SignRequest(NContainer::TCVector<uint8> const &_Request);
			void f_AddRemoteServer(NHTTP::CURL const &_URL, NContainer::TCVector<uint8> const &_ServerCert, NContainer::TCVector<uint8> const &_ClientCert);
			NStr::CStr f_GetHostID() const;
			
			template <typename tf_CStream>
 			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);
			
			NStr::CStr m_HostID;
			
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateKey;
			NContainer::TCVector<uint8> m_PublicCertificate;
			
			NContainer::TCMap<NHTTP::CURL, CActorDistributionCryptographyRemoteServer> m_RemoteClientCertificates;
			
			NContainer::TCMap<NStr::CStr, NContainer::TCVector<uint8>> m_SignedClientCertificates;
			
			NStr::CStr m_Subject;
			
			int32 m_Serial = 0;
		};
		
		struct CActorDistributionConnectionSettings
		{
			CActorDistributionConnectionSettings();
			~CActorDistributionConnectionSettings();

			void f_SetCryptography(CActorDistributionCryptographySettings const &_Settings);
			
			NHTTP::CURL m_ServerURL;
			NContainer::TCVector<uint8> m_PublicServerCertificate;
			NContainer::TCVector<uint8> m_PublicClientCertificate;
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateClientKey;
			bool m_bRetryConnectOnFailure = true;
			bool m_bAllowInsecureConnection = false; // Only enabled when m_PublicServerCertificate is empty 
		};

		struct CActorDistributionListenSettings
		{
			CActorDistributionListenSettings(uint16 _Port, NNet::ENetAddressType _AddressType = NNet::ENetAddressType_None);			
			CActorDistributionListenSettings(NContainer::TCVector<NHTTP::CURL> const &_Addresses);
			~CActorDistributionListenSettings();

			void f_SetCryptography(CActorDistributionCryptographySettings const &_Settings);

			NContainer::TCVector<NHTTP::CURL> m_ListenAddresses;
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateKey;
			NContainer::TCVector<uint8> m_CACertificate;
			NContainer::TCVector<uint8> m_PublicCertificate;
			NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
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
					, NPtr::TCWeakPointer<NPrivate::ICHost> const &_pHost
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
			NPtr::TCWeakPointer<NPrivate::ICHost> mp_pHost;
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
			void f_Clear();
			void f_Republish(NStr::CStr const &_HostID) const;
			
		private:
			CDistributedActorPublication(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID);
			
			TCWeakActor<CActorDistributionManager> mp_DistributionManager;
			NStr::CStr mp_Namespace;
			NStr::CStr mp_ActorID;
		};

		template <typename t_CImplementation>
		struct TCDelegatedActorInterface
		{
			void f_Clear();
			TCDispatchedActorCall<void> f_Destroy();
			
			template <typename ...tfp_CInterfaces, typename tf_CThis>
			TCContinuation<void> f_Publish(TCActor<CActorDistributionManager> const &_DistributionManager, tf_CThis *_pThis, NStr::CStr const &_Namespace);
			template <typename tf_CThis>
			void f_Construct(TCActor<CActorDistributionManager> const &_DistributionManager, tf_CThis *_pThis);
			
			TCDistributedActor<t_CImplementation> m_Actor;
			CDistributedActorPublication m_Publication;
			t_CImplementation *m_pActor;
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
					, TCContinuation<void> (CActor::*)(NFunction::TCFunctionMovable<TCContinuation<void> ()> &&)
					, NContainer::TCTuple<NFunction::TCFunctionMovable<TCContinuation<void> ()>>
					, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCContinuation<void> ()>> 
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
			
			NContainer::TCVector<NContainer::TCVector<uint8>> const &f_GetCertificateChain() const;
			
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
			virtual TCContinuation<NStr::CStr> f_ValidateClientAccess(NStr::CStr const &_HostID, NContainer::TCVector<NContainer::TCVector<uint8>> const &_CertificateChain) = 0;
		};

		struct CCallingHostInfo
		{
			CCallingHostInfo();
			CCallingHostInfo
				(
					TCActor<CActorDistributionManager> const &_DistributionManager
					, NStr::CStr const &_UniqueHostID
					, CHostInfo const &_HostInfo
					, NStr::CStr const &_LastExecutionID
					, uint32 _ProtocolVersion
				)
			;
			
			NStr::CStr const &f_GetRealHostID() const;
			NStr::CStr const &f_GetUniqueHostID() const;
			CHostInfo const &f_GetHostInfo() const;
			NStr::CStr const &f_LastExecutionID() const;
			TCActor<CActorDistributionManager> f_GetDistributionManager() const;
			TCDispatchedActorCall<CActorSubscription> f_OnDisconnect(TCActor<CActor> const &_Actor, NFunction::TCFunctionMutable<void ()> &&_fOnDisconnect) const;
			uint32 f_GetProtocolVersion() const;
			
			bool operator ==(CCallingHostInfo const &_Right) const;
			bool operator <(CCallingHostInfo const &_Right) const;
			
 			void f_Feed(CDistributedActorWriteStream &_Stream) const;
			void f_Consume(CDistributedActorReadStream &_Stream);
			
		private:
			TCWeakActor<CActorDistributionManager> mp_DistributionManager;
			NStr::CStr mp_UniqueHostID; // Differs from HostID when anonymous
			CHostInfo mp_HostInfo;
			NStr::CStr mp_LastExecutionID;
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
					, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				)
			;

			template <typename tf_CActor, typename... tfp_CParams>
			TCActor<TCDistributedActorWrapper<tf_CActor>> f_ConstructActor(tfp_CParams &&...p_Params);

			template <typename tf_CActor, typename tf_CDelegateTo, typename... tfp_CParams>
			TCActor<TCDistributedActorWrapper<TCDistributedInterfaceDelegator<tf_CActor, tf_CDelegateTo>>> f_ConstructInterface(tf_CDelegateTo *_pDelegateTo, tfp_CParams &&...p_Params);
		};
		
		struct CCallingHostInfoScope
		{
			CCallingHostInfoScope(CCallingHostInfo &&_NewInfo);
			~CCallingHostInfoScope();
		private:
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
				NContainer::TCVector<NContainer::TCVector<uint8>> m_CertificateChain;
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
			
			TCContinuation<CDistributedActorListenReference> f_Listen(CActorDistributionListenSettings const &_Settings);
			TCContinuation<CConnectionResult> f_Connect(CActorDistributionConnectionSettings const &_Settings);
			
			TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_CallRemote
				(
					NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
					, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_CallData
					, NPrivate::CDistributedActorStreamContext const &_Context
				)
			;
			
			TCContinuation<void> f_KickHost(NStr::CStr const &_HostID); 
			
			TCContinuation<CActorSubscription> f_SubscribeActors
				(
					NContainer::TCVector<NStr::CStr> const &_NameSpaces /// Leave empty to subscribe to all actors
					, TCActor<CActor> const &_Actor
					, NFunction::TCFunctionMutable<void (CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
					, NFunction::TCFunctionMutable<void (CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
				)
			;

			CActorSubscription f_SubscribeHostInfoChanged
				(
					TCActor<CActor> const &_Actor
					, NFunction::TCFunctionMutable<void (CHostInfo const &_HostInfo)> &&_fHostInfoChanged
				)
			;
			
			TCContinuation<CActorSubscription> f_RegisterWebsocketHandler
				(
					NStr::CStr const &_Path  
					, TCActor<> const &_Actor
					, NFunction::TCFunctionMutable
					<
						TCContinuation<void> (NPtr::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pNewServerConnection, NStr::CStr const &_RealHostID)
					>
					&&_fNewWebsocketConnection
				)
			;

			static CCallingHostInfo const &fs_GetCallingHostInfo();
			static NStr::CStr fs_GetCertificateHostID(NContainer::TCVector<uint8> const &_Certificate);
			static NStr::CStr fs_GetCertificateRequestHostID(NContainer::TCVector<uint8> const &_Certificate);
			static bool fs_IsValidNamespaceName(NStr::CStr const &_String);
			static bool fs_IsValidHostID(NStr::CStr const &_String);
			static bool fs_IsValidEnclave(NStr::CStr const &_String);

		private:
			TCContinuation<void> fp_Destroy() override;
			
			TCContinuation<CDistributedActorPublication> fp_PublishActor
				(
					TCDistributedActor<CActor> &&_Actor
					, NStr::CStr const &_Namespace
					, NPrivate::CDistributedActorInterfaceInfo const &_InterfaceInfo
				)
			;
			
			void fp_RegisterRemoteSubscription
				(
					NPtr::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &_pContextState
					, NStr::CStr const &_SubscriptionID
					, uint32 _SubscriptionSequenceID 
				)
			;
			TCContinuation<void> fp_DestroyRemoteSubscription
				(
					NPtr::TCSharedPointer<NPrivate::ICHost> const &_pHost
					, NStr::CStr const &_SubscriptionID
					, NStr::CStr const &_LastExecutionID
				)
			;
			void fp_CleanupRemoteContext(NFunction::TCFunction<void (CActorDistributionManagerInternal &_Internal)> const &_fCleanup);
			TCContinuation<void> fp_RemoveListen(NStr::CStr const &_ListenID);
			void fp_RemoveConnection(NStr::CStr const &_ConnectionID);
			TCContinuation<void> fp_UpdateConnectionSettings(NStr::CStr const &_ConnectionID, CActorDistributionConnectionSettings const &_Settings);
			void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
			void fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID);
			TCContinuation<CDistributedActorConnectionStatus> fp_GetConnectionStatus(NStr::CStr const &_ConnectionID);
			CActorSubscription fp_OnRemoteDisconnect
				(
					TCActor<CActor> const &_Actor
					, NFunction::TCFunctionMutable<void ()> &&_fOnDisconnect
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
			
			NPtr::TCUniquePointer<CActorDistributionManagerInternal> mp_pInternal;
		};
		
		CCallingHostInfo const &fg_GetCallingHostInfo();
		NStr::CStr const &fg_GetCallingHostID();
		CActorDistributionManagerInitSettings fg_InitDistributionManager(CActorDistributionManagerInitSettings const &_Settings);
		TCActor<CActorDistributionManager> const &fg_GetDistributionManager();
		void fg_InitDistributedActorSystem();
	}
}

#	define DMibPublishActorFunction(d_Function) DMibConcurrencyRegisterMemberFunctionWithStreams \
		( \
			decltype(&d_Function) \
			, &d_Function \
			, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
			, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext \
			, NMib::NConcurrency::CDistributedActorReadStream \
			, NMib::NConcurrency::CDistributedActorWriteStream \
		)

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
#include "Malterlib_Concurrency_DistributedActor.hpp"

