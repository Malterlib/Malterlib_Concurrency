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
		struct CActorDistributionManager;
		namespace NPrivate
		{
			struct CDistributedActorData : public ICDistributedActorData
			{
				NStr::CStr m_ActorID;
				bool m_bWasDestroyed = false;
				bool m_bRemote = false;
				TCWeakActor<CActorDistributionManager> m_DistributionManager;
			};
		}

		template <typename t_CActor>
		struct TCDistributedActorWrapper : public t_CActor
		{
			//static_assert(NTraits::TCIsBaseOf<t_CActor, CDistributedActor>::mc_Value, "Need to be base of CDistributedActor");
			enum
			{
				mc_bAllowInternalAccess = true
			};
			
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
			bool m_bRetryOnListenFailure = true;
		};

		struct CAbstractDistributedActor
		{
			CAbstractDistributedActor
				(
					TCDistributedActor<CActor> const &_Actor
					, NContainer::TCVector<uint32> const &_InheritanceHierarchy
					, NStr::CStr const &_UniqueHostID
					, NStr::CStr const &_RealHostID
				)
			;
			
			template <typename tf_CType>
			TCDistributedActor<tf_CType> f_GetActor() const;
			template <typename tf_CType>
			TCDistributedActor<tf_CType> f_GetActorUnsafe() const;
			inline NStr::CStr const &f_GetUniqueHostID() const;
			inline NStr::CStr const &f_GetRealHostID() const;
		private:
			TCDistributedActor<CActor> mp_Actor;
			NContainer::TCVector<uint32> mp_InheritanceHierarchy;
			NStr::CStr mp_UniqueHostID;
			NStr::CStr mp_RealHostID;
		};
		
		namespace NPrivate
		{
			template <typename tf_CFirst>
			void fg_GetDistributedActorInheritanceHierarchy(NContainer::TCVector<uint32> &_Settings)
			{
				_Settings.f_Insert(fg_GetTypeHash<tf_CFirst>());
				fg_GetDistributedActorInheritanceHierarchy<typename NTraits::TCGetBase<tf_CFirst>::CType>(_Settings);
			}
			
			template <>
			inline_always_debug void fg_GetDistributedActorInheritanceHierarchy<CActor>(NContainer::TCVector<uint32> &_Settings)
			{
			}
		}
		
		template <typename tf_CFirst>
		NContainer::TCVector<uint32> fg_GetDistributedActorInheritanceHierarchy()
		{
			NContainer::TCVector<uint32> Return;
			
			NPrivate::fg_GetDistributedActorInheritanceHierarchy<tf_CFirst>(Return);
			
			return Return;
		}
		
		struct CDistributedActorInheritanceHeirarchyPublish
		{
			template <typename ...tfp_CToPublish>
			static CDistributedActorInheritanceHeirarchyPublish fs_GetHierarchy()
			{
				CDistributedActorInheritanceHeirarchyPublish Ret;
				
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							Ret.mp_Hierarchy.f_Insert(fg_GetTypeHash<tfp_CToPublish>());
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				
				return Ret;
			}
			NContainer::TCVector<uint32> const &f_GetHierarchy() const
			{
				return mp_Hierarchy;
			}
			
		private:
			CDistributedActorInheritanceHeirarchyPublish()
			{
			}
			NContainer::TCVector<uint32> mp_Hierarchy;
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
			
			CDistributedActorPublication();
			~CDistributedActorPublication();
			
			CDistributedActorPublication(CDistributedActorPublication const &) = delete;
			CDistributedActorPublication &operator = (CDistributedActorPublication const &) = delete;
			
			CDistributedActorPublication(CDistributedActorPublication &&);
			CDistributedActorPublication &operator = (CDistributedActorPublication &&);
			
			void f_Clear();
			
		private:
			CDistributedActorPublication(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID);
			
			TCWeakActor<CActorDistributionManager> mp_DistributionManager;
			NStr::CStr mp_Namespace;
			NStr::CStr mp_ActorID;
		};

		struct CDistributedActorListenReference
		{
			friend struct CActorDistributionManager;
			
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
			NStr::CStr m_HostID;
			NStr::CStr m_Error;
			bool m_bConnected = false;
		};
		
		struct CDistributedActorConnectionReference
		{
			friend struct CActorDistributionManager;
			
			CDistributedActorConnectionReference();
			~CDistributedActorConnectionReference();
			
			CDistributedActorConnectionReference(CDistributedActorConnectionReference const &) = delete;
			CDistributedActorConnectionReference &operator = (CDistributedActorConnectionReference const &) = delete;
			
			CDistributedActorConnectionReference(CDistributedActorConnectionReference &&);
			CDistributedActorConnectionReference &operator = (CDistributedActorConnectionReference &&);
			
			void f_Clear();
			TCDispatchedActorCall<void> f_Disconnect();  
			TCDispatchedActorCall<CDistributedActorConnectionStatus> f_GetStatus(); 
			
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
			virtual TCContinuation<NStr::CStr> f_ValidateClientAccess(NStr::CStr const &_HostID, NContainer::TCVector<NContainer::TCVector<uint8>> const &_CertificateChain) pure;
		};
		
		struct CActorDistributionManager : public CActor
		{
			struct CInternal;
			
			struct CConnectionResult
			{
				CDistributedActorConnectionReference m_ConnectionReference;
				NStr::CStr m_UniqueHostID;
				NStr::CStr m_RealHostID;
				NContainer::TCVector<NContainer::TCVector<uint8>> m_CertificateChain;
				
				CConnectionResult(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_ConnectionID)
					: m_ConnectionReference(_DistributionManager, _ConnectionID)
				{
				}
			};
			
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
			
			CActorCallback f_SubscribeActors
				(
					NContainer::TCVector<NStr::CStr> const &_NameSpaces /// Leave empty to subscribe to all actors
					, TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
					, NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor
				)
			;

			static NStr::CStr fs_GetCallingHostID();
			static NStr::CStr fs_GetCertificateHostID(NContainer::TCVector<uint8> const &_Certificate);
			static NStr::CStr fs_GetCertificateRequestHostID(NContainer::TCVector<uint8> const &_Certificate);
			
		private:
			void fp_RemoveListen(NStr::CStr const &_ListenID);
			void fp_RemoveConnection(NStr::CStr const &_ConnectionID);
			void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
			TCContinuation<CDistributedActorConnectionStatus> fp_GetConnectionStatus(NStr::CStr const &_ConnectionID);
			
			friend struct CDistributedActorPublication;
			friend struct CDistributedActorListenReference;
			friend struct CDistributedActorConnectionReference;
			NPtr::TCUniquePointer<CInternal> mp_pInternal;
		};
		
		NStr::CStr fg_GetCallingHostID();
		NStr::CStr fg_InitDistributionManager(NStr::CStr const &_HostID);
		TCActor<CActorDistributionManager> const &fg_GetDistributionManager();
		void fg_InitDistributedActorSystem();
		
#define DMibCallActor(d_Actor, d_Function, d_Args...) ::NMib::NConcurrency::fg_CallActor \
			< \
				decltype(&d_Function) \
				, &d_Function \
				, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
			> \
			(d_Actor, ##d_Args)
		
#define DMibPublishActorFunction(d_Function) DMibConcurrencyRegisterMemberFunction \
		( \
			decltype(&d_Function) \
			, &d_Function \
			, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
		)
		
#ifndef DMibPNoShortCuts
#	define DCallActor DMibCallActor
#	define DPublishActorFunction DMibPublishActorFunction
#endif
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor.hpp"
