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
		
		void f_Clear();
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
		
		NPtr::TCSharedPointer<CState> mp_pState;
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

		TCContinuation<bool> f_HasPermission
			(
			 	NStr::CStr const &_Description
			 	, NContainer::TCVector<NStr::CStr> const &_Permissions
			 	, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCContinuation<bool> f_HasPermissions
			(
			 	NStr::CStr const &_Description
			 	, NContainer::TCVector<CPermissionQuery> const &_Permissions
			 	, CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCContinuation<NContainer::TCMap<NStr::CStr, bool>> f_HasPermissions
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
		TCContinuation<bool> fp_AuthenticatePermissions
			(
			 	NContainer::TCVector<CPermissionQuery> const &_PermissionsQueries
			 	, ICDistributedActorAuthenticationHandler::CRequest const &_Request
			 	, CCallingHostInfo const &_CallingHostInfo
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
		
		NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> mp_pState;
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
			
			NNet::CSSLKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
			NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
			NStr::CStr m_FriendlyName;
			NStr::CStr m_Enclave;
			NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;
			fp64 m_InitialConnectionTimeout = 5.0;
			int32 m_DefaultConnectionConcurrency = 1;
			bool m_bRetryOnListenFailureDuringInit = true;
			bool m_bSupportAuthentication = true;
		};

		CDistributedActorTrustManager
			(
				NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
				, COptions &&_Options
			)
		;

		~CDistributedActorTrustManager();
		
		
		TCContinuation<NStr::CStr> f_Initialize(); 
		
		TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumListens();
		TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address);
		TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address);
		TCContinuation<bool> f_HasListen(CDistributedActorTrustManager_Address const &_Address);

		TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients();
		TCContinuation<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket
			(
				CDistributedActorTrustManager_Address const &_Address
				, TCActorFunctor
				<
					TCContinuation<void> 
					(
						NStr::CStr const &_HostID
						, CCallingHostInfo const &_HostInfo
						, NContainer::TCVector<uint8> const &_CertificateRequest
					)
				> 
				&&_fOnUseTicket
			 	, TCActorFunctor<TCContinuation<void> (NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo)> &&_fOnCertificateSigned
			)
		;
		TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID);
		TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID);
		
		TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections();
		TCContinuation<CHostInfo> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1);
		TCContinuation<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1);
		TCContinuation<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency = -1);
		TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address);
		TCContinuation<bool> f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address);

		TCContinuation<CConnectionState> f_GetConnectionState();
		
		TCContinuation<NStr::CStr> f_GetHostID() const; 
		TCContinuation<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> f_GetDistributionManager() const;
		
		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo);
		TCContinuation<void> f_AllowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		TCContinuation<void> f_DisallowHostsForNamespace(NStr::CStr const &_Namespace, NContainer::TCSet<NStr::CStr> const &_Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		template <typename tf_CActor>
		TCContinuation<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActors(NStr::CStr const &_Namespace, TCActor<CActor> const &_Actor);
		template <typename tf_CActor>
		TCContinuation<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActorsWithVersion
			(
			 	NStr::CStr const &_Namespace
			 	, TCActor<CActor> const &_Actor
			 	, CDistributedActorProtocolVersions const &_Versions
			)
		;

		TCContinuation<CDistributedActorTrustManagerInterface::CEnumPermissionsResult> f_EnumPermissions(bool _bIncludeHostInfo);
		TCContinuation<void> f_AddPermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCMap<NStr::CStr, CPermissionRequirements> const &_Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCContinuation<void> f_RemovePermissions
			(
				CPermissionIdentifiers const &_Identity
				, NContainer::TCSet<NStr::CStr> const &_Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCContinuation<void> f_RegisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);
		TCContinuation<void> f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> const &_Permissions);

		TCContinuation<NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo);
		TCContinuation<NStorage::TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> f_TryGetUser(NStr::CStr const &_UserID);
		TCContinuation<void> f_AddUser(NStr::CStr const &_UserID, NStr::CStr const &_UserName);
		TCContinuation<void> f_RemoveUser(NStr::CStr const &_UserID);
		TCContinuation<void> f_SetUserInfo
			(
				NStr::CStr const &_UserID
				, NStorage::TCOptional<NStr::CStr> const &_UserName
			 	, NContainer::TCSet<NStr::CStr> const &_RemoveMetadata
			 	, NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> const &_AddMetadata
			)
		;
		TCContinuation<NStr::CStr> f_GetDefaultUser();
		TCContinuation<void> f_SetDefaultUser(NStr::CStr const &_UserID);

		TCContinuation<NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo>> f_EnumAuthenticationActors() const;

		TCContinuation<NContainer::TCMap<NStr::CStr, CAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID) const;
		TCContinuation<void> f_AddUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data);
		TCContinuation<void> f_SetUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID, CAuthenticationData &&_Data);
		TCContinuation<void> f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID);
		TCContinuation<NStr::CStr> f_RegisterUserAuthenticationFactor(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_UserID, NStr::CStr const &_Factor);

		TCContinuation<CTrustedPermissionSubscription> f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> const &_Wildcards, TCActor<CActor> const &_Actor);
		
		TCContinuation<CActorDistributionListenSettings> f_GetCertificateData(CDistributedActorTrustManager_Address const &_Address) const; 
		
		TCActor<ICActorDistributionManagerAccessHandler> f_GetAccessHandler() const;
		TCActor<ICDistributedActorAuthenticationHandler> f_GetAuthenticationHandler() const;
		static ICDistributedActorAuthenticationHandler::CChallenge fs_GenerateAuthenticationChallenge(NStr::CStr const &_UserID);
		TCContinuation<NContainer::TCVector<bool>> f_VerifyAuthenticationResponses
			(
				ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, ICDistributedActorAuthenticationHandler::CRequest const &_Request
				, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CResponse> const &_Responses
			)
		;

		TCContinuation<void> f_UpdateAuthenticationCache
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

		TCContinuation<void> fp_Destroy() override;
		
		void fp_Init();
		
		TCContinuation<CHostInfo> fp_GetHostInfo(NStr::CStr const &_HostID);
		
		auto fp_SubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState)
			-> TCContinuation<NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>>>
		;
		void fp_UnsubscribeTrustedActors(NPtr::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> const &_pState);
		NContainer::TCMap<CPermissionIdentifiers, NContainer::TCMap<NStr::CStr, CPermissionRequirements>> fp_SubscribeToPermissions
			(
				NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
			)
		;
		CDistributedActorTrustManagerAuthenticationCache fp_FilterCachedAuthentications
			(
				NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState
			)
		;
		void fp_UnsubscribeToPermissions(NPtr::TCSharedPointer<NPrivate::CTrustedPermissionSubscriptionState> const &_pState);
		
		struct CInternal;
		NPtr::TCUniquePointer<CInternal> mp_pInternal;			
	};

	class CDistributedActorTrustManagerProxy;

	template<typename t_CType>
	TCContinuation<NStr::CStr> fg_ExportUser(TCActor<t_CType> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	template<typename t_CType>
	TCContinuation<NStr::CStr> fg_ImportUser(TCActor<t_CType> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCContinuation<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCContinuation<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCContinuation<NStr::CStr> fg_ImportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	extern template TCContinuation<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	extern template TCContinuation<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	extern template TCContinuation<NStr::CStr> fg_ExportUser
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
