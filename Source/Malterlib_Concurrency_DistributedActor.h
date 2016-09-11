// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_AsyncResult.h"
#include "Malterlib_Concurrency_Continuation.h"
#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Actor.h"
#include "Malterlib_Concurrency_ActorHolder.h"

#include <Mib/Web/HTTP/URL>
#include <Mib/Memory/Allocators/Secure>
#include <Mib/Concurrency/WeakActor>

namespace NMib
{
	namespace NConcurrency
	{
		using CDistributedActorWriteStream = NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>;
		using CDistributedActorReadStream = NStream::CBinaryStreamMemoryPtr<>;
		
		struct CActorDistributionManager;
		namespace NPrivate
		{
			struct CDistributedActorData;
			struct CDistributedActorStreamContextSending;
			struct CDistributedActorStreamContextReceiving;
			struct CDistributedActorStreamContextSendState;
			struct CStreamSubscriptionSend;
			struct ICHost : public NPtr::TCSharedPointerIntrusiveBase<NPtr::ESharedPointerOption_SupportWeakPointer>
			{
				virtual ~ICHost();
			};
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
		using TCDistributedActor = TCActor<TCDistributedActorWrapper<t_CActor>>;
		
		template <typename t_CActor>
		using TCWeakDistributedActor = TCWeakActor<TCDistributedActorWrapper<t_CActor>>;
		
		template <typename tf_CActor, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(tfp_CParams &&...p_Params);
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
		auto fg_CallActor(tf_CActor const &_Actor, tfp_CParams && ...p_Params)
		{
			return _Actor(t_pMemberFunction, fg_Forward<tfp_CParams>(p_Params)...);
		}
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
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
			CActorDistributionCryptographySettings(NStr::CStr const &_HostID);
			~CActorDistributionCryptographySettings();
			void f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, int32 _KeyBits = 4096);
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
			bool operator == (CDistributedActorProtocolVersions const &_Other) const;
			bool operator < (CDistributedActorProtocolVersions const &_Other) const;
			
			uint32 m_MinSupported = TCLimitsInt<uint32>::mc_Max;
			uint32 m_MaxSupported = TCLimitsInt<uint32>::mc_Max;
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
		
		struct CDistributedActorInheritanceHeirarchyPublish
		{
			template <typename ...tfp_CToPublish>
			static CDistributedActorInheritanceHeirarchyPublish fs_GetHierarchy();
			inline_always NContainer::TCVector<uint32> const &f_GetHierarchy() const;
			inline_always CDistributedActorProtocolVersions const &f_GetProtocolVersions() const;
			
		private:
			CDistributedActorInheritanceHeirarchyPublish();
			NContainer::TCVector<uint32> mp_Hierarchy;
			CDistributedActorProtocolVersions mp_ProtocolVersions;
		};
		
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
					, TCContinuation<void> (CActor::*)(NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&)
					, NContainer::TCTuple<NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>>
					, NMeta::TCTypeList<NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>> 
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
			TCActor<CActorDistributionManager> const &f_GetDistributionManager() const;
			TCDispatchedActorCall<CActorSubscription> f_OnDisconnect(TCActor<CActor> const &_Actor, NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fOnDisconnect) const;
			uint32 f_GetProtocolVersion() const;
			
			bool operator ==(CCallingHostInfo const &_Right) const;
			bool operator <(CCallingHostInfo const &_Right) const;
			
		private:
			TCActor<CActorDistributionManager> mp_DistributionManager;
			NStr::CStr mp_UniqueHostID; // Differs from HostID when anonymous
			CHostInfo mp_HostInfo;
			NStr::CStr mp_LastExecutionID;
			uint32 mp_ProtocolVersion;
		};
		
		struct CActorDistributionManagerInternal;
		
		struct CActorDistributionManager : public CActor
		{
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
			
			CActorDistributionManager(NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName);
			CActorDistributionManager(NStr::CStr const &_HostID);
			~CActorDistributionManager();
			
			void f_Construct();
			
			void f_SetSecurity(CDistributedActorSecurity const &_Security);
			void f_SetAccessHandler(TCActor<ICActorDistributionManagerAccessHandler> const &_AccessHandler);
			
			TCContinuation<CDistributedActorListenReference> f_Listen(CActorDistributionListenSettings const &_Settings);
			TCContinuation<CConnectionResult> f_Connect(CActorDistributionConnectionSettings const &_Settings);
			
			TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_CallRemote
				(
					NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
					, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_CallData
					, NPrivate::CDistributedActorStreamContextSending const &_Context
				)
			;
			
			TCContinuation<void> f_KickHost(NStr::CStr const &_HostID); 
			
			TCContinuation<CDistributedActorPublication> f_PublishActor
				(
					TCDistributedActor<CActor> &&_Actor
					, NStr::CStr const &_Namespace
					, CDistributedActorInheritanceHeirarchyPublish const &_ClassesToPublish
				)
			;
			
			CActorSubscription f_SubscribeActors
				(
					NContainer::TCVector<NStr::CStr> const &_NameSpaces /// Leave empty to subscribe to all actors
					, TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CDistributedActorIdentifier const &_RemovedActor)> &&_fOnRemovedActor
				)
			;

			CActorSubscription f_SubscribeHostInfoChanged
				(
					TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CHostInfo const &_HostInfo)> &&_fHostInfoChanged
				)
			;

			static CCallingHostInfo const &fs_GetCallingHostInfo();
			static NStr::CStr fs_GetCertificateHostID(NContainer::TCVector<uint8> const &_Certificate);
			static NStr::CStr fs_GetCertificateRequestHostID(NContainer::TCVector<uint8> const &_Certificate);

		private:
			void fp_RegisterRemoteSubscription
				(
					NPrivate::CDistributedActorStreamContextSending const &_Context
					, NStr::CStr const &_SubscriptionID 
				)
			;
			void fp_DestroyRemoteSubscription
				(
					NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
					, NStr::CStr const &_SubscriptionID
					, NStr::CStr const &_LastExecutionID
				)
			;
			void fp_CleanupRemoteContext(NFunction::TCFunction<void (CActorDistributionManagerInternal &_Internal)> const &_fCleanup);
			void fp_RemoveListen(NStr::CStr const &_ListenID);
			void fp_RemoveConnection(NStr::CStr const &_ConnectionID);
			TCContinuation<void> fp_UpdateConnectionSettings(NStr::CStr const &_ConnectionID, CActorDistributionConnectionSettings const &_Settings);
			void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
			void fp_RepublishActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID, NStr::CStr const &_HostID);
			TCContinuation<CDistributedActorConnectionStatus> fp_GetConnectionStatus(NStr::CStr const &_ConnectionID);
			CActorSubscription fp_OnRemoteDisconnect
				(
					TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fOnDisconnect
					, NStr::CStr const &_UniqueHostID
					, NStr::CStr const &_LastExecutionID
				)
			;
			
			friend struct CDistributedActorPublication;
			friend struct CDistributedActorListenReference;
			friend struct CDistributedActorConnectionReference;
			friend struct CCallingHostInfo;
			friend struct NPrivate::CDistributedActorStreamContextSendState;
			friend struct NPrivate::CStreamSubscriptionSend;
			
			NPtr::TCUniquePointer<CActorDistributionManagerInternal> mp_pInternal;
		};
		
		CCallingHostInfo const &fg_GetCallingHostInfo();
		NStr::CStr fg_InitDistributionManager(NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName = {});
		TCActor<CActorDistributionManager> const &fg_GetDistributionManager();
		void fg_InitDistributedActorSystem();
	}
}

#include "Malterlib_Concurrency_DistributedActor_Private.h"

#define DMibCallActor(d_Actor, d_Function, d_Args...) ::NMib::NConcurrency::fg_CallActor \
	< \
		decltype(&d_Function) \
		, &d_Function \
		, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
	> \
	(d_Actor, ##d_Args)
		
#define DMibPublishActorFunction(d_Function) DMibConcurrencyRegisterMemberFunctionWithStreamContext \
	( \
		decltype(&d_Function) \
		, &d_Function \
		, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
		, NMib::NConcurrency::NPrivate::CDistributedActorStreamContextReceiving \
	)
		
#ifndef DMibPNoShortCuts
#	define DCallActor DMibCallActor
#	define DPublishActorFunction DMibPublishActorFunction
#endif

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor.hpp"
