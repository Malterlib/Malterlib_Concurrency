// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"
#include <Mib/Process/Platform>
#include <Mib/Cryptography/Certificate>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	TCFuture<void> CDistributedActorTrustManager::CInternal::f_WaitForInit()
	{
		using namespace NStr;

		if (m_Initialize == EInitialize_Success)
			co_return {};
		else if (m_Initialize == EInitialize_Failure)
			co_return m_pInitializeException;

		auto SequenceSubscription = co_await m_InitSequencer.f_Sequence();

		if (m_Initialize == EInitialize_Success)
			co_return {};
		else if (m_Initialize == EInitialize_Failure)
			co_return m_pInitializeException;

		auto Result = co_await f_Init().f_Wrap();

		if (Result)
			m_Initialize = EInitialize_Success;
		else
		{
			m_Initialize = EInitialize_Failure;
			m_pInitializeException = NException::fg_MakeException
				(
					DMibErrorInstanceWrapped("Trust manager failed to initialize: {}"_f << Result.f_GetExceptionStr(), fg_Move(Result).f_GetException(), false)
				)
			;
			co_return m_pInitializeException;
		}

		co_return {};
	}

	void CDistributedActorTrustManager::fp_Init()
	{
		fg_InitDistributedActorSystem();
		auto &Internal = *mp_pInternal;

		Internal.f_WaitForInit() > fg_LogError("Mib/Concurrency/Trust", "Failed to initialize trust manager");
	}

	TCFuture<NStr::CStr> CDistributedActorTrustManager::f_Initialize()
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_return Internal.m_BasicConfig.m_HostID;
	}

	CActorDistributionConnectionSettings CDistributedActorTrustManager::CInternal::f_GetConnectionSettings(CConnectionState const &_State)
	{
		auto &ClientConnection = _State.m_ClientConnection;

		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = _State.f_GetAddress().m_URL;
		ConnectionSettings.m_PublicServerCertificate = ClientConnection.m_PublicServerCertificate;
		ConnectionSettings.m_PublicClientCertificate = ClientConnection.m_PublicClientCertificate;
		ConnectionSettings.m_PrivateClientKey = m_BasicConfig.m_CAPrivateKey;
		ConnectionSettings.m_bRetryConnectOnFirstFailure = true;
		ConnectionSettings.m_bRetryConnectOnFailure = true;

		return ConnectionSettings;
	}

	TCFuture<void> CDistributedActorTrustManager::f_WaitForInitialConnection()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_bConnectionsInitialized)
			co_return {};
		co_return co_await Internal.m_AwaitingConnection.f_Insert().f_Future();
	}

	TCFuture<void> CDistributedActorTrustManager::CInternal::f_Init()
	{
		auto CheckDestroy = co_await m_pThis->f_CheckDestroyedOnResume();

		auto BasicConfig = co_await m_Database(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig);

		auto [DefaultUser, ServerCertificates, ListenConfigs, PrimaryListen, ClientConnections, Namespaces, Permissions, Users, AuthenticationFactors] = co_await
			(
				m_Database(&ICDistributedActorTrustManagerDatabase::f_GetDefaultUser)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumServerCertificates, true)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumListenConfigs)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_GetPrimaryListen)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClientConnections, true)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumNamespaces, true)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumPermissions, true)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumUsers, true)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumAuthenticationFactor, true)
			)
		;

		auto pCleanup = fg_OnScopeExitShared
			(
				[this, pDestroyed = m_pDestroyed]
				{
					if (*pDestroyed)
						return;
					m_Listen.f_Clear();
					m_pPrimaryListen = nullptr;
					m_ClientConnections.f_Clear();
					m_ClientConnectionsInDatabase.f_Clear();
					m_Hosts.f_Clear();
				}
			)
		;

		m_BasicConfig = fg_Move(BasicConfig);
		m_DefaultUser = fg_Move(DefaultUser);

		TCFutureVector<void> WriteDatabaseResults;

		if (m_BasicConfig.m_HostID.f_IsEmpty() || m_BasicConfig.m_CAPrivateKey.f_IsEmpty())
		{
			// Generate root CA
			if (m_BasicConfig.m_HostID.f_IsEmpty())
				m_BasicConfig.m_HostID = NCryptography::fg_HighEntropyRandomID();

			NCryptography::CCertificateOptions Options;
			Options.m_KeySetting = m_KeySetting;
			Options.m_CommonName = fg_Format("Malterlib Distributed Actors Root - {}", m_BasicConfig.m_HostID).f_Left(64);
			auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
			Extension.m_bCritical = false;
			Extension.m_Value = m_BasicConfig.m_HostID;

			NCryptography::CCertificateSignOptions SignOptions;
			SignOptions.m_Serial = 1;
			SignOptions.m_Days = 100*365;

			try
			{
				NCryptography::CCertificate::fs_GenerateSelfSignedCertAndKey
					(
						Options
						, m_BasicConfig.m_CACertificate
						, m_BasicConfig.m_CAPrivateKey
						, SignOptions
					)
				;
			}
			catch (NException::CException const &_Exception)
			{
				co_return DMibErrorInstance(fg_Format("Failed to generate root CA: {}", _Exception.f_GetErrorStr()));
			}

			m_Database(&ICDistributedActorTrustManagerDatabase::f_SetBasicConfig, m_BasicConfig) > WriteDatabaseResults;
		}

		if (m_BasicConfig.m_CACertificate.f_IsEmpty() || m_BasicConfig.m_CAPrivateKey.f_IsEmpty())
			co_return DMibErrorInstance("Invalid trust manager basic config. Broken database?");

		CActorDistributionManagerInitSettings DistributionManagerInitSettings{m_BasicConfig.m_HostID, m_Enclave, m_FriendlyName};

		DistributionManagerInitSettings.m_HostTimeout = m_HostTimeout;
		DistributionManagerInitSettings.m_HostDaemonTimeout = m_HostDaemonTimeout;
		DistributionManagerInitSettings.m_ReconnectDelay = m_ReconnectDelay;
		DistributionManagerInitSettings.m_bTimeoutForUnixSockets = m_bTimeoutForUnixSockets;

		if (m_fDistributionManagerFactory)
			m_ActorDistributionManager = m_fDistributionManagerFactory(fg_Move(DistributionManagerInitSettings));
		else
		{
			CActorDistributionManagerInitSettings ResultingSettings
				= fg_InitDistributionManager(fg_Move(DistributionManagerInitSettings))
			;
			if (ResultingSettings.m_HostID != m_BasicConfig.m_HostID)
				co_return DMibErrorInstance("Default distribution manager already initialized with the wrong host ID");

			if (ResultingSettings.m_Enclave != m_Enclave)
				co_return DMibErrorInstance("Default distribution manager already initialized with another enclave");

			m_ActorDistributionManager = fg_GetDistributionManager();
		}

		m_AccessHandler = fg_ConstructActor<CActorDistributionManagerAccessHandler>(fg_Construct(fg_ThisActor(m_pThis)), this);
		auto [ResultSetAccessHandler, ResultSetTranslateHostnames, HostInfoChangedSubscription, WriteDatabaseResultsResult] = co_await
			(
				m_ActorDistributionManager(&CActorDistributionManager::f_SetAccessHandler, m_AccessHandler) % "Failed to set access handler"
				+ m_ActorDistributionManager(&CActorDistributionManager::f_SetHostnameTranslate, m_TranslateHostnames) % "Failed to set translate hostnames"
				+ m_ActorDistributionManager
				(
					&CActorDistributionManager::f_SubscribeHostInfoChanged
					, g_ActorFunctorWeak / [this](CHostInfo _HostInfo) -> TCFuture<void>
					{
						auto *pHost = m_Hosts.f_FindEqual(_HostInfo.m_HostID);
						if (pHost)
						{
							pHost->m_FriendlyName = _HostInfo.m_FriendlyName;
							for (auto &ClientConnection : pHost->m_ClientConnections)
							{
								if (ClientConnection.m_ClientConnection.m_LastFriendlyName != _HostInfo.m_FriendlyName)
								{
									ClientConnection.m_ClientConnection.m_LastFriendlyName = _HostInfo.m_FriendlyName;
									m_Database
										(
											&ICDistributedActorTrustManagerDatabase::f_SetClientConnection
											, ClientConnection.f_GetAddress()
											, ClientConnection.m_ClientConnection
										)
										> [](TCAsyncResult<void> &&_Result)
										{
											if (!_Result)
											{
												DMibLogWithCategory
													(
														Mib/Concurrency/Trust
														, Error
														, "Failed to set client connection in database when updating host info: {}"
														, _Result.f_GetExceptionStr()
													)
												;
												return;
											}
										}
									;
								}
							}
						}

						m_Database
							(
								&ICDistributedActorTrustManagerDatabase::f_TryGetClient
								, _HostInfo.m_HostID
							)
							> [this, _HostInfo](TCAsyncResult<NStorage::TCUniquePointer<CClient>> &&_Client)
							{
								if (!_Client)
								{
									DMibLogWithCategory
										(
											Mib/Concurrency/Trust
											, Error
											, "Failed to get client from database when updating host info: {}"
											, _Client.f_GetExceptionStr()
										)
									;
									return;
								}
								if (!*_Client)
									return;
								auto &Client = **_Client;
								if (Client.m_LastFriendlyName != _HostInfo.m_FriendlyName)
								{
									Client.m_LastFriendlyName = _HostInfo.m_FriendlyName;
									m_Database
										(
											&ICDistributedActorTrustManagerDatabase::f_SetClient
											, _HostInfo.m_HostID
											, Client
										)
										> [](TCAsyncResult<void> &&_Result)
										{
											if (!_Result)
											{
												DMibLogWithCategory
													(
														Mib/Concurrency/Trust
														, Error
														, "Failed to set client in database when updating host info: {}"
														, _Result.f_GetExceptionStr()
													)
												;
												return;
											}
										}
									;
								}
							}
						;

						co_return {};
					}
				) % "Failed to subscribe to info changes"
				+ fg_AllDone(WriteDatabaseResults)
			)
		;
		m_HostInfoChangedSubscription = fg_Move(HostInfoChangedSubscription);

		for (auto &Listen : ListenConfigs)
		{
			auto &NewListen = m_Listen[Listen];
			if (PrimaryListen && *PrimaryListen == Listen.m_Address)
				m_pPrimaryListen = &NewListen;
		}

		m_ServerCertificates = fg_Move(ServerCertificates);

		for (auto &NamespaceEntry : Namespaces.f_Entries())
		{
			auto &NamespaceName = NamespaceEntry.f_Key();
			if (!CActorDistributionManager::fs_IsValidNamespaceName(NamespaceName))
				continue;
			auto &NamespaceState = m_Namespaces[NamespaceName];
			NamespaceState.m_Namespace = fg_Move(NamespaceEntry.f_Value());
			NamespaceState.m_bExistsInDatabase = true;
		}

		for (auto &PermissionEntry : Permissions.f_Entries())
		{
			auto &HostID = PermissionEntry.f_Key();
			auto &PermissionState = m_Permissions[HostID];
			PermissionState.m_Permissions = fg_Move(PermissionEntry.f_Value());
			PermissionState.m_bExistsInDatabase = true;
		}

		for (auto &UserInfoEntry : Users.f_Entries())
		{
			auto &UserID = UserInfoEntry.f_Key();
			auto &UserState = m_Users[UserID];
			UserState.m_UserInfo = fg_Move(UserInfoEntry.f_Value());
			UserState.m_bExistsInDatabase = true;
		}

		for (auto &UserAuthenticationFactorsEntry : AuthenticationFactors.f_Entries())
		{
			auto &UserID = UserAuthenticationFactorsEntry.f_Key();
			for (auto &FactorEntry : UserAuthenticationFactorsEntry.f_Value().f_Entries())
			{
				auto &AuthenticationFactor = m_UserAuthenticationFactors[UserID][FactorEntry.f_Key()];
				AuthenticationFactor.m_AuthenticationFactor = fg_Move(FactorEntry.f_Value());
				AuthenticationFactor.m_bExistsInDatabase = true;
			}
		}

		for (auto &ClientConnectionEntry : ClientConnections.f_Entries())
		{
			auto &ClientConnectionKey = ClientConnectionEntry.f_Key();

			NStr::CStr HostID;
			try
			{
				HostID = CActorDistributionManager::fs_GetCertificateHostID(ClientConnectionEntry.f_Value().m_PublicServerCertificate);
			}
			catch (NException::CException const &_Exception)
			{
				co_return DMibErrorInstance(fg_Format("Error getting host ID from certificate. Broken database? :{}", _Exception.f_GetErrorStr()));
			}

			if (HostID.f_IsEmpty())
				co_return DMibErrorInstance("Client connection has certificate with missing host id. Broken database?");

			m_ClientConnectionsInDatabase[ClientConnectionKey];

			auto &ClientConnection = m_ClientConnections[ClientConnectionKey];

			ClientConnection.m_ClientConnection = fg_Move(ClientConnectionEntry.f_Value());

			auto &Host = m_Hosts[HostID];
			Host.m_ClientConnections.f_Insert(ClientConnection);
			Host.m_FriendlyName = ClientConnection.m_ClientConnection.m_LastFriendlyName;
			ClientConnection.m_pHost = &Host;
		}

		TCFutureMap<CListenConfig, CDistributedActorListenReference> ListenResults;

		for (auto &Listen : m_Listen.f_Keys())
		{
			CActorDistributionListenSettings ListenSettings(NContainer::fg_CreateVector<NWeb::NHTTP::CURL>(Listen.m_Address.m_URL));

			auto *pServerCert = m_ServerCertificates.f_FindEqual(Listen.m_Address.m_URL.f_GetHost());
			if (!pServerCert)
				co_return DMibErrorInstance("Invalid trust manager address in listen. No server certificate found. Broken database?");

			ListenSettings.m_PrivateKey = pServerCert->m_PrivateKey;
			ListenSettings.m_PublicCertificate = pServerCert->m_PublicCertificate;
			ListenSettings.m_CACertificate = m_BasicConfig.m_CACertificate;
			ListenSettings.m_bRetryOnListenFailure = m_bRetryOnListenFailureDuringInit;
			ListenSettings.m_ListenFlags = m_ListenFlags;

			m_ActorDistributionManager(&CActorDistributionManager::f_Listen, ListenSettings) > ListenResults[Listen];
		}

		TCFutureMap<CDistributedActorTrustManager_Address, void> ConnectResults;

		for (auto &ClientConnectionEntry : m_ClientConnections.f_Entries())
		{
			auto &Address = ClientConnectionEntry.f_Key();
			TCPromiseFuturePair<void> ConnectPromise;

			m_ActorDistributionManager(&CActorDistributionManager::f_Connect, f_GetConnectionSettings(ClientConnectionEntry.f_Value()), m_InitialConnectionTimeout)
				> [this, Address, ConnectPromise = fg_Move(ConnectPromise.m_Promise)](TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
				{
					if (!_ConnectionResult)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Trust
								, Error
								, "Failed to connect to '{}' in init: {}"
								, Address
								, _ConnectionResult.f_GetExceptionStr()
							)
						;
						ConnectPromise.f_SetResult();
						return;
					}

					auto ConnectionMapped = m_ClientConnections(Address);
					DMibFastCheck(!ConnectionMapped.f_WasCreated());
					auto &ClientConnection = *ConnectionMapped;

					ClientConnection.m_ConnectionReferences.f_Insert(fg_Move(_ConnectionResult->m_ConnectionReference));
					f_ApplyConnectionConcurrency(ClientConnection);

					ConnectPromise.f_SetResult();
				}
			;

			fg_Move(ConnectPromise.m_Future) > ConnectResults[Address];
		}

		TCPromiseFuturePair<void> WaitForConnectionsPromise;

		if (m_bWaitForConnectionsDuringInit)
			m_AwaitingConnection.f_Insert(WaitForConnectionsPromise.m_Promise);
		else
			WaitForConnectionsPromise.m_Promise.f_SetResult();

		fg_AllDoneWrapped(ConnectResults) > [this, pCleanup]
			(
				TCAsyncResult<NContainer::TCMap<CDistributedActorTrustManager_Address, TCAsyncResult<void>>> &&_ConnectionResult
			)
			{
				if (!_ConnectionResult)
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/Trust
							, Error
							, "Failed to wait for connection results: {}"
							, _ConnectionResult.f_GetExceptionStr()
						)
					;
					return;
				}
				for (auto &ConnectionResult : *_ConnectionResult)
				{
					if (ConnectionResult)
						continue;
					DMibLogWithCategory
						(
							Mib/Concurrency/Trust
							, Error
							, "Initial connection for '{}' failed with: {}"
							, (*_ConnectionResult).fs_GetKey(ConnectionResult)
							, ConnectionResult.f_GetExceptionStr()
						)
					;
				}

				m_bConnectionsInitialized = true;

				for (auto &Waiting : m_AwaitingConnection)
					Waiting.f_SetResult();

				m_AwaitingConnection.f_Clear();
			}
		;

		auto [ListenResultsUnwrapped, ConnectionResults] = co_await (fg_AllDone(ListenResults) + fg_Move(WaitForConnectionsPromise.m_Future));

		for (auto &ListenResult : ListenResultsUnwrapped.f_Entries())
		{
			auto ListenMapped = m_Listen(ListenResult.f_Key());
			DMibFastCheck(!ListenMapped.f_WasCreated());
			auto &Listen = *ListenMapped;

			Listen.m_ListenReference = fg_Move(ListenResult.f_Value());
		}

		pCleanup->f_Clear();

		m_AuthenticationActors = ICDistributedActorTrustManagerAuthenticationActor::fs_GetRegisteredAuthenticationFactors(fg_ThisActor(m_pThis));
		m_TicketInterface = m_ActorDistributionManager->f_ConstructActor<CInternal::CTicketInterface>(this, fg_ThisActor(m_pThis));

		TCFuture<void> PublishFuture;
		if (m_bSupportAuthentication)
		{
			PublishFuture = m_AuthenticationInterface.f_Publish<ICDistributedActorAuthentication>
				(
					m_ActorDistributionManager
					, m_pThis
				)
			;
		}
		else
			PublishFuture = g_Void;

		auto [TicketPublishResult, AuthenticationPublishResult] = co_await
			(
				m_TicketInterface->f_Publish<CTicketInterface>("Anonymous/com.malterlib/Concurrency/TrustManagerTicket", 0.0)
				+ fg_Move(PublishFuture)
			).f_Wrap()
		;

		if (!TicketPublishResult)
			DMibLogWithCategory(Mib/Concurrency/App, Error, "Publishing ticket interface failed: {}", TicketPublishResult.f_GetExceptionStr());
		else
			m_TicketInterfacePublication = fg_Move(*TicketPublishResult);

		if (!AuthenticationPublishResult)
			DMibLogWithCategory(Mib/Concurrency/App, Error, "Publishing authentication interface failed: {}", AuthenticationPublishResult.f_GetExceptionStr());
		else if (m_bSupportAuthentication)
			DMibLogWithCategory(Mib/Concurrency/App, Debug, "Authentication interface published successfully");

		co_return {};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
