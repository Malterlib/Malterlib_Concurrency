// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Private.h"

namespace NMib
{
	namespace NConcurrency
	{
		class CDistributedActorTrustManager;
		
		struct CTrustedActorInfo
		{
			CHostInfo m_HostInfo;
			NStr::CStr m_UniqueHostID;
		};

		template <typename t_CActor>
		struct TCTrustedActor
		{
			inline CDistributedActorIdentifier const &f_GetIdentifier() const;
			TCDistributedActor<t_CActor> m_Actor;
			CTrustedActorInfo m_TrustInfo;
			uint32 m_ProtocolVersion = TCLimitsInt<uint32>::mc_Max; 
		};
		
		template <typename t_CActor>
		struct TCTrustedActorSubscription
		{
			TCTrustedActorSubscription(TCTrustedActorSubscription const &) = delete;
			TCTrustedActorSubscription &operator =(TCTrustedActorSubscription const &) = delete;
			TCTrustedActorSubscription(TCTrustedActorSubscription &&);
			TCTrustedActorSubscription &operator =(TCTrustedActorSubscription &&);
			TCTrustedActorSubscription() = default;
			~TCTrustedActorSubscription();
			
			NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<t_CActor>> m_Actors;
			
			TCTrustedActor<t_CActor> const *f_GetOneActor(NStr::CStr const &_HostID, NStr::CStr &o_Error) const;
			
			void f_Clear();

			void f_OnNewActor(NFunction::TCFunction<void (NFunction::CThisTag &, TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> &&_fOnNewActor);
			void f_OnRemoveActor(NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor);
			
		private:
			friend class CDistributedActorTrustManager;
			
			struct CState : public NPrivate::CTrustedActorSubscriptionState
			{
				NFunction::TCFunction<void (NFunction::CThisTag &, TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> m_fOnNewActor;
				NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> m_fOnRemovedActor;
				void f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors) override;
				void f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> const &_Actors) override;
				
				TCTrustedActorSubscription *m_pSubscription;
			};
			
			NPtr::TCSharedPointer<CState> mp_pState;
		};
		
		struct CTrustedPermissionSubscription
		{
			CTrustedPermissionSubscription(CTrustedPermissionSubscription const &) = delete;
			CTrustedPermissionSubscription &operator =(CTrustedPermissionSubscription const &) = delete;
			CTrustedPermissionSubscription(CTrustedPermissionSubscription &&);
			CTrustedPermissionSubscription &operator =(CTrustedPermissionSubscription &&);
			CTrustedPermissionSubscription() = default;
			~CTrustedPermissionSubscription();

			template <typename ...tfp_CPermission>
			bool f_HostHasAnyPermission(NStr::CStr const &_Host, tfp_CPermission const &...p_Permission) const;
			
			bool f_HostHasPermission(NStr::CStr const &_Host, NStr::CStr const &_Permission) const;
			NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> const &f_GetPermissions() const;
			
			void f_Clear();

			void f_OnPermissionsAdded
				(
					NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsAdded)> &&_fOnPermissionsAdded
				)
			;
			void f_OnPermissionsRemoved
				(
					NFunction::TCFunction<void (NFunction::CThisTag &, NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> &&_fOnPermissionsRemoved
				)
			;
			
			
		private:
			friend class CDistributedActorTrustManager;
			friend struct NPrivate::CTrustedPermissionSubscriptionState;
			
			NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> mp_pState;
			NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> mp_Permissions;
		};
		
		class CDistributedActorTrustManager : public NConcurrency::CActor
		{
		public:
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
				
				NStr::CStr f_ToStringTicket() const;
				static CTrustTicket fs_FromStringTicket(NStr::CStr const &_StringTicket);

				CDistributedActorTrustManager_Address m_ServerAddress;
				NContainer::TCVector<uint8> m_ServerPublicCert;
				NStr::CStr m_Token;
			};
			
			struct CAddressConnectionState
			{
				inline CDistributedActorTrustManager_Address const &f_GetAddress() const;

				bool m_bConnected = false;
				NStr::CStr m_Error;
				NTime::CTime m_ErrorTime;
				CHostInfo m_HostInfo;
			};
			
			struct CHostConnectionState
			{
				inline NStr::CStr const &f_GetHostID() const;
				
				NContainer::TCMap<CDistributedActorTrustManager_Address, CAddressConnectionState> m_Addresses;
			};
			
			struct CConnectionState
			{
				NContainer::TCMap<NStr::CStr, CHostConnectionState> m_Hosts;
			};
			
			struct CNamespacePermissions
			{
				inline NStr::CStr const &f_GetName() const;
				inline bool operator ==(CNamespacePermissions const &_Right) const;
				inline bool operator <(CNamespacePermissions const &_Right) const;
				template <typename tf_CString>
				void f_Format(tf_CString &o_String) const;
				
				NContainer::TCMap<NStr::CStr, CHostInfo> m_AllowedHosts;
				NContainer::TCMap<NStr::CStr, CHostInfo> m_DisallowedHosts;
			};
			
			CDistributedActorTrustManager
				(
					NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
					, NFunction::TCFunction
					<
						NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
					> const &_fConstructManager = nullptr
					,  uint32 _KeySize = 4096
					, NNet::ENetFlag _ListenFlags = NNet::ENetFlag_None
					, NStr::CStr const &_FriendlyName = NStr::CStr()
				)
			;
			
			~CDistributedActorTrustManager();
			
			void f_Construct() override;
			TCContinuation<void> f_Destroy() override;
			
			TCContinuation<NStr::CStr> f_Initialize(); 
			
			TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens();
			TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address);

			TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients();
			TCContinuation<CTrustTicket> f_GenerateConnectionTicket(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID);
			TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID);
			
			TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CHostInfo>> f_EnumClientConnections();
			TCContinuation<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout);
			TCContinuation<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address);

			TCContinuation<CConnectionState> f_GetConnectionState();
			
			TCContinuation<NStr::CStr> f_GetHostID() const; 
			TCContinuation<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> f_GetDistributionManager() const;
			
			TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo);
			TCContinuation<void> f_AllowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts);
			TCContinuation<void> f_DisallowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts);
			template <typename tf_CActor>
			TCContinuation<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActors(NStr::CStr const &_Namespace, TCActor<CActor> const &_Actor);
			
			TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>>> f_EnumHostPermissions(bool _bIncludeHostInfo);
			TCContinuation<void> f_AddHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions);
			TCContinuation<void> f_RemoveHostPermissions(NStr::CStr const &_HostID, NContainer::TCSet<NStr::CStr> const &_Permissions);
			TCContinuation<void> f_RegisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);
			TCContinuation<void> f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);

			TCContinuation<CTrustedPermissionSubscription> f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> const &_Wildcards, TCActor<CActor> const &_Actor);
			
			// Handle renewal of certificates
			
		private:
			template <typename t_CActor>
			friend struct TCTrustedActorSubscription;
			friend struct CTrustedPermissionSubscription;
			
			TCContinuation<CHostInfo> fp_GetHostInfo(NStr::CStr const &_HostID);
			
			auto fp_SubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
				-> TCContinuation<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>
			;
			void fp_UnsubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState);
			TCContinuation<NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>>> fp_SubscribeToPermissions
				(
					NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
				)
			;
			void fp_UnsubscribeToPermissions(NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState);
			
			struct CInternal;
			NPtr::TCUniquePointer<CInternal> mp_pInternal;			
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager.hpp"
