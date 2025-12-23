// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../DistributedActor/Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedTrust_Shared.h"
#include "Malterlib_Concurrency_DistributedTrust_Database.h"
#include "Malterlib_Concurrency_DistributedTrust_Private.h"
#include "Malterlib_Concurrency_DistributedTrust_Interface.h"
#include "Malterlib_Concurrency_DistributedTrust_Proxy.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthActor.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthCache.h"
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

		TCTrustedActor() = default;
		TCTrustedActor(TCTrustedActor &&_Other) = default;
		TCTrustedActor(TCTrustedActor const &_Other);
	private:
		template <typename t_CActor2>
		friend struct TCTrustedActorSubscription;
		TCFuture<void> mp_OnNewActorFinished;
	};

	namespace NPrivate
	{
		void fg_HandleUndestroyedSubscription(TCFuture<void> &&_Result);
	}

	template <typename t_CActor>
	struct TCTrustedActorSubscription
	{
		TCTrustedActorSubscription(TCTrustedActorSubscription const &) = delete;
		TCTrustedActorSubscription &operator =(TCTrustedActorSubscription const &) = delete;
		TCTrustedActorSubscription(TCTrustedActorSubscription &&);
		TCTrustedActorSubscription &operator =(TCTrustedActorSubscription &&);
		TCTrustedActorSubscription() = default;
		~TCTrustedActorSubscription();

		TCTrustedActor<t_CActor> const *f_GetOneActor(NStr::CStr const &_HostID, NStr::CStr &o_Error) const;

		TCUnsafeFuture<void> f_Destroy();
		bool f_IsEmpty() const;
		bool f_HasActors() const;

		TCUnsafeFuture<void> f_OnActor
			(
				TCActorFunctor<TCFuture<void> (TCDistributedActor<t_CActor> _NewActor, CTrustedActorInfo _ActorInfo)> &&_fOnNewActor
				, TCActorFunctor<TCFuture<void> (TCWeakDistributedActor<CActor> _RemovedActor, CTrustedActorInfo _ActorInfo)> &&_fOnRemovedActor
				, NStr::CStr const &_ErrorCategory = {}
				, NStr::CStr const &_ErrorPrefix = {}
			)
		;

		NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<t_CActor>> m_Actors;

	public:
		friend class CDistributedActorTrustManager;

		struct CState : public NPrivate::CTrustedActorSubscriptionState
		{
			FUnitVoidFutureFunction f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> &&_Actors) override;
			FUnitVoidFutureFunction f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> &&_Actors) override;

			NStr::CStr f_NewActorErrorCategory() const;
			NStr::CStr f_NewActorErrorPrefix() const;
			NStr::CStr f_RemoveActorErrorCategory() const;
			NStr::CStr f_RemoveActorErrorPrefix() const;

			CActorSubscription m_Subscription;
			TCActorFunctor<TCFuture<void> (TCDistributedActor<t_CActor> _NewActor, CTrustedActorInfo _ActorInfo)> m_fOnNewActor;
			TCActorFunctor<TCFuture<void> (TCWeakDistributedActor<CActor> _RemovedActor, CTrustedActorInfo _ActorInfo)> m_fOnRemovedActor;
			NStr::CStr m_ErrorCategory;
			NStr::CStr m_ErrorPrefix;

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

		TCUnsafeFuture<bool> f_HasPermission
			(
				NStr::CStr _Description
				, NContainer::TCVector<NStr::CStr> _Permissions
				, CCallingHostInfo _CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCUnsafeFuture<bool> f_HasPermissions
			(
				NStr::CStr _Description
				, NContainer::TCVector<CPermissionQuery> _Permissions
				, CCallingHostInfo _CallingHostInfo = fg_GetCallingHostInfo()
			) const
		;
		TCUnsafeFuture<NContainer::TCMap<NStr::CStr, bool>> f_HasPermissions
			(
				NStr::CStr _Description
				, NContainer::TCMap<NStr::CStr, NContainer::TCVector<CPermissionQuery>> _NamedPermissionQueries
				, CCallingHostInfo _CallingHostInfo = fg_GetCallingHostInfo()
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
		TCUnsafeFuture<bool> fp_AuthenticatePermissions
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

	struct CDistributedActorTrustManagerHolder : public CDefaultActorHolder
	{
		CDistributedActorTrustManagerHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;

		template <typename tf_CActor>
		auto f_SubscribeTrustedActors(uint32 _MinSupportedVersion = 0, uint32 _MaxSupportedVersion = TCLimitsInt<uint32>::mc_Max, TCActor<CActor> &&_Actor = fg_CurrentActor());
	};

	class CDistributedActorTrustManager : public NConcurrency::CActor
	{
	public:
		using CActorHolder = CDistributedActorTrustManagerHolder;
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
			fp64 m_InitialConnectionTimeout = 5_seconds;
			fp64 m_HostTimeout = 10_minutes;
			fp64 m_HostDaemonTimeout = 4_hours;
			fp64 m_ReconnectDelay = 500_ms;
			int32 m_DefaultConnectionConcurrency = 1;
			bool m_bRetryOnListenFailureDuringInit = true;
			bool m_bWaitForConnectionsDuringInit = true;
			bool m_bSupportAuthentication = true;
			bool m_bTimeoutForUnixSockets = true;
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
		TCFuture<void> f_AddListen(CDistributedActorTrustManager_Address _Address);
		TCFuture<void> f_RemoveListen(CDistributedActorTrustManager_Address _Address);
		TCFuture<bool> f_HasListen(CDistributedActorTrustManager_Address _Address);
		TCFuture<void> f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address);
		TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> f_GetPrimaryListen();
#if DMibConfig_Tests_Enable
		TCFuture<void> f_Debug_BreakListenConnections(CDistributedActorTrustManager_Address _Address, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
		TCFuture<void> f_Debug_SetListenServerBroken(CDistributedActorTrustManager_Address _Address, bool _bBroken);
#endif

		TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>> f_EnumClients();
		TCFuture<CTrustGenerateConnectionTicketResult> f_GenerateConnectionTicket
			(
				CDistributedActorTrustManager_Address _Address
				, TCActorFunctor
				<
					TCFuture<void>
					(
						NStr::CStr _HostID
						, CCallingHostInfo _HostInfo
						, NContainer::CByteVector _CertificateRequest
					)
				>
				_fOnUseTicket
				, TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo)> _fOnCertificateSigned
			)
		;
		TCFuture<void> f_RemoveClient(NStr::CStr _HostID);
		TCFuture<bool> f_HasClient(NStr::CStr _HostID);

		TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> f_EnumClientConnections();
		TCFuture<CHostInfo> f_AddClientConnection(CTrustTicket _TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency = -1);
		TCFuture<CHostInfo> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency = -1);
		TCFuture<void> f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency = -1);
		TCFuture<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address _Address, bool _bPreserveHost = false);
		TCFuture<bool> f_HasClientConnection(CDistributedActorTrustManager_Address _Address);
#if DMibConfig_Tests_Enable
		TCFuture<void> f_Debug_BreakClientConnection(CDistributedActorTrustManager_Address _Address, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags);
#endif
		TCFuture<CConnectionState> f_GetConnectionState();

		TCFuture<NStr::CStr> f_GetHostID() const;
		TCFuture<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> f_GetDistributionManager() const;

		TCFuture<NContainer::TCMap<NStr::CStr, CNamespacePermissions>> f_EnumNamespacePermissions(bool _bIncludeHostInfo);
		TCFuture<void> f_AllowHostsForNamespace(NStr::CStr _Namespace, NContainer::TCSet<NStr::CStr> _Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		TCFuture<void> f_DisallowHostsForNamespace(NStr::CStr _Namespace, NContainer::TCSet<NStr::CStr> _Hosts, EDistributedActorTrustManagerOrderingFlag _OrderingFlags);
		template <typename tf_CActor>
		TCFuture<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActors
			(
				NStr::CStr _Namespace
				, TCActor<CActor> _Actor
				, uint32 _MinSupportedVersion
				, uint32 _MaxSupportedVersion
			)
		;
		template <typename tf_CActor>
		TCFuture<TCTrustedActorSubscription<tf_CActor>> f_SubscribeTrustedActorsWithVersion
			(
				NStr::CStr _Namespace
				, TCActor<CActor> _Actor
				, CDistributedActorProtocolVersions _Versions
			)
		;

		TCFuture<CDistributedActorTrustManagerInterface::CEnumPermissionsResult> f_EnumPermissions(bool _bIncludeHostInfo);
		TCFuture<void> f_AddPermissions
			(
				CPermissionIdentifiers _Identity
				, NContainer::TCMap<NStr::CStr, CPermissionRequirements> _Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCFuture<void> f_RemovePermissions
			(
				CPermissionIdentifiers _Identity
				, NContainer::TCSet<NStr::CStr> _Permissions
				, EDistributedActorTrustManagerOrderingFlag _OrderingFlags
			)
		;
		TCFuture<void> f_RegisterPermissions(NContainer::TCSet<NStr::CStr> _Permissions);
		TCFuture<void> f_UnregisterPermissions(NContainer::TCSet<NStr::CStr> _Permissions);

		TCFuture<NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo>> f_EnumUsers(bool _bIncludeFullInfo);
		TCFuture<NStorage::TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> f_TryGetUser(NStr::CStr _UserID);
		TCFuture<void> f_AddUser(NStr::CStr _UserID, NStr::CStr _UserName);
		TCFuture<void> f_RemoveUser(NStr::CStr _UserID);
		TCFuture<void> f_SetUserInfo
			(
				NStr::CStr _UserID
				, NStorage::TCOptional<NStr::CStr> _UserName
				, NContainer::TCSet<NStr::CStr> _RemoveMetadata
				, NContainer::TCMap<NStr::CStr, NEncoding::CEJsonSorted> _AddMetadata
			)
		;
		TCFuture<NStr::CStr> f_GetDefaultUser();
		TCFuture<void> f_SetDefaultUser(NStr::CStr _UserID);

		TCFuture<NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo>> f_EnumAuthenticationActors() const;

		TCFuture<NContainer::TCMap<NStr::CStr, CAuthenticationData>> f_EnumUserAuthenticationFactors(NStr::CStr _UserID) const;
		TCFuture<void> f_AddUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CAuthenticationData _Data);
		TCFuture<void> f_SetUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID, CAuthenticationData _Data);
		TCFuture<void> f_RemoveUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID);
		TCFuture<NStr::CStr> f_RegisterUserAuthenticationFactor
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _UserID
				, NStr::CStr _Factor
			)
		;

		TCFuture<CTrustedPermissionSubscription> f_SubscribeToPermissions(NContainer::TCVector<NStr::CStr> _Wildcards, TCActor<CActor> _Actor);

		TCFuture<CActorDistributionListenSettings> f_GetCertificateData(CDistributedActorTrustManager_Address _Address) const;

		TCFuture<TCActor<ICActorDistributionManagerAccessHandler>> f_GetAccessHandler() const;
		TCActor<ICDistributedActorAuthenticationHandler> f_GetAuthenticationHandler() const;
		static ICDistributedActorAuthenticationHandler::CChallenge fs_GenerateAuthenticationChallenge(NStr::CStr const &_UserID);
		TCFuture<NContainer::TCVector<bool>> f_VerifyAuthenticationResponses
			(
				ICDistributedActorAuthenticationHandler::CChallenge _Challenge
				, ICDistributedActorAuthenticationHandler::CRequest _Request
				, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CResponse> _Responses
			)
		;

		TCFuture<void> f_UpdateAuthenticationCache
			(
				CPermissionIdentifiers _Identify
				, NContainer::TCSet<NStr::CStr> _AuthenticatedPermissions
				, NTime::CTime _ExpirationTime
				, NTime::CTime _CacheTime
			)
		;

		TCFuture<CDistributedActorTrustManagerInterface::CConnectionsDebugStats> f_GetConnectionsDebugStats();
		TCFuture<NStr::CStr> f_GetHostFriendlyName(NStr::CStr _HostID);

	private:
		template <typename t_CActor>
		friend struct TCTrustedActorSubscription;
		friend struct CTrustedPermissionSubscription;

		struct CTrustedActorSubscription
		{
			NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> m_InitialActors;
			CActorSubscription m_Subscription;
		};

		TCFuture<void> fp_Destroy() override;

		void fp_Init();

		TCFuture<CHostInfo> fp_GetHostInfo(NStr::CStr _HostID);

		auto fp_SubscribeTrustedActors(NStorage::TCSharedPointer<NPrivate::CTrustedActorSubscriptionState> _pState)
			-> TCFuture<CTrustedActorSubscription>
		;
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
	TCFuture<NStr::CStr> fg_ExportUser(TCActor<t_CType> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
	template<typename t_CType>
	TCFuture<NStr::CStr> fg_ImportUser(TCActor<t_CType> _TrustManager, NStr::CStr _UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManager> _TrustManager, NStr::CStr _UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserData);
	extern template TCFuture<NStr::CStr> fg_ImportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserData);
	extern template TCFuture<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManager> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
	extern template TCFuture<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
	extern template TCFuture<NStr::CStr> fg_ExportUser
		(
			TCDistributedActor<CDistributedActorTrustManagerInterface> _TrustManager
			, NStr::CStr _UserID
			, bool _bIncludePrivate
		)
	;
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedTrust.hpp"
