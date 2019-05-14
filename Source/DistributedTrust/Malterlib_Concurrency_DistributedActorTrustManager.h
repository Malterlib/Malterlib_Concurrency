// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../DistributedActor/Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Private.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Proxy.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationCache.h"
#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	struct CCommandLineControl;
	class CDistributedActorTrustManager;
	struct CAuthenticationActorInfo;
	struct CDistributedActorTrustManagerAuthenticationCache;
	
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
		
		TCFuture<void> f_Destroy();
		bool f_IsEmpty() const;

		void f_OnActor(NFunction::TCFunctionMovable<void (TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> &&_fOnNewActor, bool _bReportCurrent = true);
		void f_OnRemoveActor(NFunction::TCFunctionMovable<void (TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor);
		
	private:
		friend class CDistributedActorTrustManager;
		
		struct CState : public NPrivate::CTrustedActorSubscriptionState
		{
			NFunction::TCFunctionMovable<void (TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> m_fOnNewActor;
			NFunction::TCFunctionMovable<void (TCWeakDistributedActor<CActor> const &_RemovedActor)> m_fOnRemovedActor;
			TCDispatchedWeakActorCall<void> f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors) override;
			TCDispatchedWeakActorCall<void> f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> const &_Actors) override;
			
			TCTrustedActorSubscription *m_pSubscription;
		};
		
		NStorage::TCSharedPointer<CState> mp_pState;
	};

	struct CPermissionQuery
	{
		enum EQueryResult
		{
			EQueryResult_NoPermission,
			EQueryResult_HasPermission,
			EQueryResult_NeedAuthentication,
		};

		CPermissionQuery();
		CPermissionQuery(TCInitializerList<NStr::CStr> const &);
		CPermissionQuery &&f_Description(NStr::CStr const &_Description) &&;
		CPermissionQuery &&f_Wildcard(bool _bIsWildcard) &&;
		EQueryResult f_MatchesPermission
			(
				NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
				, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements> &o_RequestedPermissions
			 	, CPermissionIdentifiers const &_Identity
			 	, CPermissionIdentifiers const &_FullIdentity
			 	, CDistributedActorTrustManagerAuthenticationCache &_AuthenticationCache
			) const
		;
		bool f_MatchesAuthentication
			(
				NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
				, NContainer::TCMap<NStr::CStr, NContainer::TCSet<NStr::CStr>> const &_AuthenticatedPermissions
			 	, NContainer::TCMap<NStr::CStr, int64> &o_AuthenticatedPermissions
			) const
		;

		NContainer::TCVector<NStr::CStr> m_Permissions;
		NStr::CStr m_Description;
		bool m_bIsWildcard = false;
	};

	struct CTrustedPermissionSubscription
	{
		CTrustedPermissionSubscription(CTrustedPermissionSubscription const &) = delete;
		CTrustedPermissionSubscription &operator =(CTrustedPermissionSubscription const &) = delete;
		CTrustedPermissionSubscription(CTrustedPermissionSubscription &&);
		CTrustedPermissionSubscription &operator =(CTrustedPermissionSubscription &&);
		CTrustedPermissionSubscription() = default;
		~CTrustedPermissionSubscription();

		TCFuture<bool> f_HasPermission
			(
			 	NStr::CStr const &_Description
			 	, NContainer::TCVector<NStr::CStr> const &_Permissions
			 	, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCFuture<bool> f_HasPermissions
			(
			 	NStr::CStr const &_Description
			 	, NContainer::TCVector<CPermissionQuery> const &_Permissions
			 	, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCFuture<NContainer::TCMap<NStr::CStr, bool>> f_HasPermissions
			(
			 	NStr::CStr const &_Description
			 	, NContainer::TCMap<NStr::CStr, NContainer::TCVector<CPermissionQuery>> const &_NamedPermissionQueries
			 	, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;

		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> const &f_GetPermissions() const;
		
		void f_Clear();

		void f_OnPermissionsAdded
			(
				NFunction::TCFunctionMovable<void
					(
						CPermissionIdentifiers const &_Identity
						, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_PermissionsAdded
					)> &&_fOnPermissionsAdded
			)
		;
		void f_OnPermissionsRemoved
			(
				NFunction::TCFunctionMovable<void (CPermissionIdentifiers const &_Identity, NContainer::TCSet<NStr::CStr> const &_PermissionsRemoved)> &&_fOnPermissionsRemoved
			)
		;
		
		
	private:
		TCFuture<bool> fp_AuthenticatePermissions
			(
			 	NContainer::TCVector<CPermissionQuery> _PermissionsQueries
			 	, ICDistributedActorAuthenticationHandler::CRequest _Request
			 	, CCallingHostInfo _CallingHostInfo
			) const
		;

		CPermissionQuery::EQueryResult fp_HasPermissionForQuery
			(
				NContainer::TCVector<CPermissionQuery> const &_Permissions
				, CCallingHostInfo const &_CallingHostInfo
				, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
			) const
		;
		CPermissionQuery::EQueryResult fp_HasPermissionForQuery
			(
				CPermissionQuery const &_Permission
				, CCallingHostInfo const &_CallingHostInfo
				, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
			) const
		;

		CPermissionQuery::EQueryResult fp_HasPermissionForQuery
			(
				CPermissionQuery const &_Permission
				, CCallingHostInfo const &_CallingHostInfo
			 	, NContainer::TCVector<CPermissionIdentifiers> const &_IdentityPowerSet
				, NContainer::TCVector<NContainer::TCVector<ICDistributedActorAuthenticationHandler::CPermissionWithRequirements>> &o_RequestedPermissions
			) const
		;
		friend class CDistributedActorTrustManager;
		friend struct NPrivate::CTrustedPermissionSubscriptionState;
		
		NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> mp_pState;
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> mp_Permissions;
		mutable CDistributedActorTrustManagerAuthenticationCache mp_AuthenticationCache;
	};
	
	class CDistributedActorTrustManager : public NConcurrency::CActor
	{
	public:
		using CNamespacePermissions = CDistributedActorTrustManagerInterface::CNamespacePermissions;
		using CTrustTicket = CDistributedActorTrustManagerInterface::CTrustTicket;
		using CTrustGenerateConnectionTicketResult = CDistributedActorTrustManagerInterface::CTrustGenerateConnectionTicketResult;
		using CClientConnectionInfo = CDistributedActorTrustManagerInterface::CClientConnectionInfo;

		struct CConcurrentConnectionState
		{
			NStr::CStr m_Error;
			NTime::CTime m_ErrorTime;
 			bool m_bConnected = false;
		};
		
		struct CAddressConnectionState
		{
			inline CDistributedActorTrustManager_Address const &f_GetAddress() const;

			NContainer::TCVector<CConcurrentConnectionState> m_States;
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
		
		struct COptions
		{
			NFunction::TCFunctionMovable
			<
				NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (CActorDistributionManagerInitSettings const &_Settings)
			> m_fConstructManager = nullptr;
			
			NCryptography::CPublicKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
			NNetwork::ENetFlag m_ListenFlags = NNetwork::ENetFlag_None;
			NStr::CStr m_FriendlyName;
			NStr::CStr m_Enclave;
			NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;
			fp64 m_InitialConnectionTimeout = 5.0;
			int32 m_DefaultConnectionConcurrency = 1;
			bool m_bRetryOnListenFailureDuringInit = true;
			bool m_bWaitForConnectionsDuringInit = true;
			bool m_bSupportAuthentication = true;
		};

		CDistributedActorTrustManager
			(
				NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
				, COptions &&_Options
			)
		;

		~CDistributedActorTrustManager();
		
		
		TCFuture<NStr::CStr> f_Initialize();
		TCFuture<void> f_WaitForInitialConnection();
		
		TCFuture<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens();
		TCFuture<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address);
		TCFuture<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address);
		TCFuture<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address);

		TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients();
		TCFuture<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket
			(
				CDistributedActorTrustManager_Address const &_Address
				, TCActorFunctor
				<
					TCFuture<void>
					(
						NStr::CStr const &_HostID
						, CCallingHostInfo const &_HostInfo
						, NContainer::CByteVector const &_CertificateRequest
					)
				> 
				&&_fOnUseTicket
			 	, TCActorFunctor<TCFuture<void> (NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo)> &&_fOnCertificateSigned
			)
		;
		TCFuture<void> f_RemoveClient(NStr::CStr const &_HostID);
		TCFuture<bool> f_HasClient(NStr::CStr const &_HostID);
		
		TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections();
		TCFuture<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1);
		TCFuture<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1);
		TCFuture<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1);
		TCFuture<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address);
		TCFuture<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address);

		TCFuture<CConnectionState> f_GetConnectionState();
		
		TCFuture<NStr::CStr> f_GetHostID() const;
		TCFuture<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> f_GetDistributionManager() const;
		
		TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo);
		TCFuture<void> f_AllowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		TCFuture<void> f_DisallowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		template <typename tf_CActor>
		TCFuture<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActors(NStr::CStr const &_Namespace, TCActor<CActor> const &_Actor);
		template <typename tf_CActor>
		TCFuture<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActorsWithVersion
			(
			 	NStr::CStr const &_Namespace
			 	, TCActor<CActor> const &_Actor
			 	, CDistributedActorProtocolVersions const &_Versions
			)
		;

		TCFuture<CDistributedActorTrustManagerInterface::CEnumPermissionsResult> f_EnumPermissions(bool _bIncludeHostInfo);
		TCFuture<void> f_AddPermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCFuture<void> f_RemovePermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCSet<NStr::CStr> const &_Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCFuture<void> f_RegisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);
		TCFuture<void> f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);

		TCFuture<NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo);
		TCFuture<NStorage::TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> f_TryGetUser(NStr::CStr const &_UserID);
		TCFuture<void> f_AddUser(NStr::CStr const &_UserID, NStr::CStr const &_UserName);
		TCFuture<void> f_RemoveUser(NStr::CStr const &_UserID);
		TCFuture<void> f_SetUserInfo
			(
				NStr::CStr const &_UserID
				, NStorage::TCOptional<NStr::CStr> const &_UserName
			 	, NContainer::TCSet<NStr::CStr> const &_RemoveMetadata
			 	, NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> const &_AddMetadata
			)
		;
		TCFuture<NStr::CStr> f_GetDefaultUser();
		TCFuture<void> f_SetDefaultUser(NStr::CStr const &_UserID);

		TCFuture<NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo>> f_EnumAuthenticationActors() const;

		TCFuture<NContainer::TCMap<NStr::CStr, CAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID) const;
		TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data);
		TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data);
		TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID);
		TCFuture<NStr::CStr> f_RegisterUserAuthenticationFactor
			(
			 	NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 	, NStr::CStr const &_UserID
			 	, NStr::CStr const &_Factor
			)
		;

		TCFuture<CTrustedPermissionSubscription> f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> const &_Wildcards, TCActor<CActor> const &_Actor);
		
		TCFuture<CActorDistributionListenSettings> f_GetCertificateData(CDistributedActorTrustManager_Address const &_Address) const;
		
		TCActor<ICActorDistributionManagerAccessHandler> f_GetAccessHandler() const;
		TCActor<ICDistributedActorAuthenticationHandler> f_GetAuthenticationHandler() const;
		static ICDistributedActorAuthenticationHandler::CChallenge fs_GenerateAuthenticationChallenge(NStr::CStr const &_UserID);
		TCFuture<NContainer::TCVector<bool>> f_VerifyAuthenticationResponses
			(
				ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, ICDistributedActorAuthenticationHandler::CRequest const &_Request
				, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CResponse> const &_Responses
			)
		;

		TCFuture<void> f_UpdateAuthenticationCache
			(
				CPermissionIdentifiers const &_Identify
				, NContainer::TCSet<NStr::CStr> &&_AuthenticatedPermissions
				, NTime::CTime const &_ExpirationTime
				, NTime::CTime const &_CacheTime
			)
		;

		// Handle renewal of certificates
		
	private:
		template <typename t_CActor>
		friend struct TCTrustedActorSubscription;
		friend struct CTrustedPermissionSubscription;

		TCFuture<void> fp_Destroy() override;
		
		void fp_Init();
		
		TCFuture<CHostInfo> fp_GetHostInfo(NStr::CStr const &_HostID);
		
		auto fp_SubscribeTrustedActors(NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
			-> TCFuture<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>
		;
		void fp_UnsubscribeTrustedActors(NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState);
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> fp_SubscribeToPermissions
			(
				NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
			)
		;
		CDistributedActorTrustManagerAuthenticationCache fp_FilterCachedAuthentications
			(
				NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
			)
		;
		void fp_UnsubscribeToPermissions(NStorage::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState);
		
		struct CInternal;
		NStorage::TCUniquePointer<CInternal> mp_pInternal;			
	};

	class CDistributedActorTrustManagerProxy;

	template<typename t_CType>
	TCFuture<NStr::CStr> fg_ExportUser(TCActor<t_CType> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	template<typename t_CType>
	TCFuture<NStr::CStr> fg_ImportUser(TCActor<t_CType> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCFuture<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	extern template TCFuture<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	extern template TCFuture<NStr::CStr> fg_ExportUser
		(
		 	TCDistributedActor<CDistributedActorTrustManagerInterface> const &_TrustManager
		 	, NStr::CStr const &_UserID
		 	, bool _bIncludePrivate
		)
	;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager.hpp"
