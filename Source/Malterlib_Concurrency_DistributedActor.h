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

#include <initializer_list>

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			struct CDistributedActorData : public ICDistributedActorData
			{
				NStr::CStr m_ActorID;
				bool m_bWasDestroyed = false;
				bool m_bRemote = false;
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
			CActorDistributionCryptographySettings();
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
			
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateCertificate;
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
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateClientCertificate;
			bool m_bRetryConnectOnFailure = true;
		};

		struct CActorDistributionListenSettings
		{
			CActorDistributionListenSettings(uint16 _Port);			
			CActorDistributionListenSettings(NContainer::TCVector<NNet::CNetAddress> const &_Address);
			~CActorDistributionListenSettings();

			void f_SetCryptography(CActorDistributionCryptographySettings const &_Settings);

			NContainer::TCVector<NNet::CNetAddress> m_ListenAddresses;
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateCertificate;
			NContainer::TCVector<uint8> m_PublicCertificate;
			bool m_bRetryOnListenFailure = true;
		};

		struct CAbstractDistributedActor
		{
			template <typename tf_CType>
			TCDistributedActor<tf_CType> f_GetActor() const
			{
				uint32 Hash = fg_GetTypeHash<tf_CType>();
				if (mp_InheritanceHierarchy.f_BinarySearch(Hash) < 0)
				{
					DMibFastCheck(false);
					return TCDistributedActor<tf_CType>();
				}
				return (TCDistributedActor<tf_CType> &)mp_Actor;
			}
			template <typename tf_CType>
			TCDistributedActor<tf_CType> f_GetActorUnsafe() const
			{
				return (TCDistributedActor<tf_CType> &)mp_Actor;
			}
			CAbstractDistributedActor(TCDistributedActor<CActor> const &_Actor, NContainer::TCVector<uint32> const &_InheritanceHierarchy);
		private:
			TCDistributedActor<CActor> mp_Actor;
			NContainer::TCVector<uint32> mp_InheritanceHierarchy;
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
				
				std::initializer_list<bool> Dummy = 
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
			CDistributedActorPublication(TCActor<CActorDistributionManager> _DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID);
			
			TCActor<CActorDistributionManager> mp_DistributionManager;
			NStr::CStr mp_Namespace;
			NStr::CStr mp_ActorID;
		};
		
		struct CActorDistributionManager : public CActor
		{
			CActorDistributionManager();
			~CActorDistributionManager();
			
			void f_SetSecurity(CDistributedActorSecurity const &_Security);
			
			TCContinuation<void> f_Connect(CActorDistributionConnectionSettings const &_Settings);
			TCContinuation<void> f_Listen(CActorDistributionListenSettings const &_Settings);
			
			TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_CallRemote
				(
					NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
					, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_CallData 
				)
			;
			
			TCContinuation<CDistributedActorPublication> f_PublishActor(TCDistributedActor<CActor> &&_Actor, NStr::CStr const &_Namespace, CDistributedActorInheritanceHeirarchyPublish const &_ClassesToPublish);
			
			CActorCallback f_SubscribeActors
				(
					NContainer::TCVector<NStr::CStr> const &_NameSpaces /// Leave empty to subscribe to all actors
					, TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
					, NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor
				)
			;
			
		private:
			
			void fp_RemoveActorPublication(NStr::CStr const &_NamespaceID, NStr::CStr const &_ActorID);
			
			friend struct CDistributedActorPublication;
			struct CInternal;
			NPtr::TCUniquePointer<CInternal> mp_pInternal;
		};
		
		TCActor<CActorDistributionManager> const &fg_GetDistributionManager();
		
#define DMibCallActor(d_Actor, d_Function, d_Args...) ::NMib::NConcurrency::fg_CallActor \
			< \
				decltype(&d_Function) \
				, &d_Function \
				, ::NMib::fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function)) \
			> \
			(d_Actor, ##d_Args)
		
#ifndef DMibPNoShortCuts
#	define DCallActor DMibCallActor
#endif
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor.hpp"
