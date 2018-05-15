// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include <Mib/Process/Platform>
#include <Mib/Network/SSL>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib::NConcurrency
{
	void CDistributedActorTrustManager::fp_Init()
	{
		fg_InitDistributedActorSystem();
		auto &Internal = *mp_pInternal;
		Internal.m_pInitOnce = fg_Construct
			(
				self
				, [this]
				{
					auto &Internal = *mp_pInternal; 
					return Internal.f_InitAttempt();
				}
				, false
			)
		;
		
		(*Internal.m_pInitOnce)() > [this](TCAsyncResult<NStr::CStr> &&_Result)
			{
				auto &Internal = *mp_pInternal; 
				if (_Result)
					Internal.m_Initialize = EInitialize_Success;
				else
				{
					Internal.m_Initialize = EInitialize_Failure;
					Internal.m_InitializeError = _Result.f_GetExceptionStr();
				}
			}
		;
	}

	TCContinuation<NStr::CStr> CDistributedActorTrustManager::f_Initialize()
	{
		TCContinuation<NStr::CStr> Continuation;

		auto &Internal = *mp_pInternal;
		
		(*Internal.m_pInitOnce)() > [Continuation](TCAsyncResult<NStr::CStr> &&_Result)
			{
				Continuation.f_SetResult(fg_Move(_Result));
			}
		;
		
		return Continuation;
	}

	TCContinuation<NStr::CStr> CDistributedActorTrustManager::CInternal::f_InitAttempt()
	{
		TCContinuation<NStr::CStr> Continuation;
		
		m_Database(&ICDistributedActorTrustManagerDatabase::f_GetBasicConfig)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_GetDefaultUser)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumServerCertificates, true)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumListenConfigs)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClientConnections, true)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumNamespaces, true)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumPermissions, true)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumUsers, true)
			+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumAuthenticationFactor, true)
			> [this, Continuation]
			(
				TCAsyncResult<CBasicConfig> &&_BasicConfig
				, TCAsyncResult<CDefaultUser> &&_DefaultUser
				, TCAsyncResult<NContainer::TCMap<NStr::CStr, CServerCertificate>> &&_ServerCertificates
				, TCAsyncResult<NContainer::TCSet<CListenConfig>> &&_ListenConfigs
				, TCAsyncResult<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>> &&_ClientConnections
				, TCAsyncResult<NContainer::TCMap<NStr::CStr, CNamespace>> &&_Namespaces
				, TCAsyncResult<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> &&_Permissions
				, TCAsyncResult<NContainer::TCMap<NStr::CStr, CUserInfo>> &&_Users
			 	, TCAsyncResult<NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>>> &&_AuthenticationFactors

			) mutable
			{
				if (!_BasicConfig)
					return Continuation.f_SetException(fg_Move(_BasicConfig));
				if (!_DefaultUser)
					return Continuation.f_SetException(fg_Move(_DefaultUser));
				if (!_ServerCertificates)
					return Continuation.f_SetException(fg_Move(_ServerCertificates));
				if (!_ListenConfigs)
					return Continuation.f_SetException(fg_Move(_ListenConfigs));
				if (!_ClientConnections)
					return Continuation.f_SetException(fg_Move(_ClientConnections));
				if (!_Namespaces)
					return Continuation.f_SetException(fg_Move(_Namespaces));
				if (!_Permissions)
					return Continuation.f_SetException(fg_Move(_Permissions));
				if (!_Users)
					return Continuation.f_SetException(fg_Move(_Users));
				if (!_AuthenticationFactors)
					return Continuation.f_SetException(fg_Move(_AuthenticationFactors));

				f_Init
					(
						Continuation
						, *_BasicConfig
						, *_DefaultUser
						, *_ListenConfigs
						, *_ServerCertificates
						, *_ClientConnections
						, *_Namespaces
						, *_Permissions
						, *_Users
						, *_AuthenticationFactors
					)
				;

				m_AuthenticationActors = ICDistributedActorTrustManagerAuthenticationActor::fs_GetRegisteredAuthenticationFactors(fg_ThisActor(m_pThis));
			}
		;
		
		return Continuation;
	}

	CActorDistributionConnectionSettings CDistributedActorTrustManager::CInternal::f_GetConnectionSettings(CConnectionState const &_State)
	{
		auto &ClientConnection = _State.m_ClientConnection;
		
		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = _State.f_GetAddress().m_URL;
		ConnectionSettings.m_PublicServerCertificate = ClientConnection.m_PublicServerCertificate;
		ConnectionSettings.m_PublicClientCertificate = ClientConnection.m_PublicClientCertificate;
		ConnectionSettings.m_PrivateClientKey = m_BasicConfig.m_CAPrivateKey;
		ConnectionSettings.m_bRetryConnectOnFailure = true;
		
		return ConnectionSettings;
	}
	
	void CDistributedActorTrustManager::CInternal::f_Init
		(
			TCContinuation<NStr::CStr> &_Continuation
			, CBasicConfig const &_Basic
			, CDefaultUser const &_DefaultUser
			, NContainer::TCSet<CListenConfig> const &_Listen
			, NContainer::TCMap<NStr::CStr, CServerCertificate> const &_ServerCertificates
			, NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> const &_ClientConnections
			, NContainer::TCMap<NStr::CStr, CNamespace> const &_Namespaces
			, NContainer::TCMap<CPermissionIdentifiers, CPermissions> const &_Permissions
			, NContainer::TCMap<NStr::CStr, NDistributedActorTrustManagerDatabase::CUserInfo> const &_Users
			, NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CUserAuthenticationFactor>> &_AuthenticationFactors
		)
	{
		auto pCleanup = fg_OnScopeExitShared
			(
				[this, pDestroyed = m_pDestroyed]
				{
					if (*pDestroyed)
						return;
					m_Listen.f_Clear();
					m_ClientConnections.f_Clear();
					m_ClientConnectionsInDatabase.f_Clear();
					m_Hosts.f_Clear();
				}
			)
		;
		
		m_BasicConfig = _Basic;
		m_DefaultUser = _DefaultUser;
		
		TCActorResultVector<void> WriteDatabaseResults;

		if (m_BasicConfig.m_HostID.f_IsEmpty() || m_BasicConfig.m_CAPrivateKey.f_IsEmpty())
		{
			// Generate root CA
			if (m_BasicConfig.m_HostID.f_IsEmpty())
				m_BasicConfig.m_HostID = NCryptography::fg_HighEntropyRandomID();
			
			NNet::CSSLContext::CCertificateOptions Options;
			Options.m_KeySetting = m_KeySetting;
			Options.m_CommonName = fg_Format("Malterlib Distributed Actors Root - {}", m_BasicConfig.m_HostID).f_Left(64); 
			auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
			Extension.m_bCritical = false; 
			Extension.m_Value = m_BasicConfig.m_HostID;
			
			NNet::CSignOptions SignOptions;
			SignOptions.m_Serial = 1;
			SignOptions.m_Days = 100*365;
			
			try
			{
				NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey
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
				_Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to generate root CA: {}", _Exception.f_GetErrorStr())));
				return;
			}
				
			m_Database(&ICDistributedActorTrustManagerDatabase::f_SetBasicConfig, m_BasicConfig) > WriteDatabaseResults.f_AddResult();
		}
		
		if (m_BasicConfig.m_CACertificate.f_IsEmpty() || m_BasicConfig.m_CAPrivateKey.f_IsEmpty())
		{
			_Continuation.f_SetException(DMibErrorInstance("Invalid trust manager basic config. Broken database?"));
			return;
		}

		if (m_fDistributionManagerFactory)
			m_ActorDistributionManager = m_fDistributionManagerFactory(CActorDistributionManagerInitSettings{m_BasicConfig.m_HostID, m_Enclave, m_FriendlyName});
		else
		{
			CActorDistributionManagerInitSettings ResultingSettings 
				= fg_InitDistributionManager(CActorDistributionManagerInitSettings{m_BasicConfig.m_HostID, m_Enclave, m_FriendlyName})
			;
			if (ResultingSettings.m_HostID != m_BasicConfig.m_HostID)
			{
				_Continuation.f_SetException(DMibErrorInstance("Default distribution manager already initialized with the wrong host ID"));
				return;
			}
			if (ResultingSettings.m_Enclave != m_Enclave)
			{
				_Continuation.f_SetException(DMibErrorInstance("Default distribution manager already initialized with another enclave"));
				return;
			}
			m_ActorDistributionManager = fg_GetDistributionManager();
		}
		m_AccessHandler = fg_ConstructActor<CActorDistributionManagerAccessHandler>(fg_Construct(fg_ThisActor(m_pThis)), this);
		m_ActorDistributionManager(&CActorDistributionManager::f_SetAccessHandler, m_AccessHandler)
			+ m_ActorDistributionManager(&CActorDistributionManager::f_SetHostnameTraslate, m_TranslateHostnames)
			+ m_ActorDistributionManager
			(
				&CActorDistributionManager::f_SubscribeHostInfoChanged
				, fg_ThisActor(m_pThis)
				, [this](CHostInfo const &_HostInfo)
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
						> [this, _HostInfo](TCAsyncResult<NPtr::TCUniquePointer<CClient>> &&_Client)
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
				}
			)
			+ WriteDatabaseResults.f_GetResults()
			> 
			[
				this
				,_Continuation
				, _Listen
				, _ServerCertificates
				, _ClientConnections
				, _Namespaces
				, _Permissions
				, _Users
			 	, _AuthenticationFactors
				, pCleanup
			]
			(
				TCAsyncResult<void> &&_ResultSetAccessHandler
				, TCAsyncResult<void> &&_ResultSetTranslateHostnames
				, TCAsyncResult<CActorSubscription> &&_HostInfoChangedSubscription
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_WriteDatabaseResults
			)
			{
				if (!_ResultSetAccessHandler)
				{
					_Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to set access handler: {}", _ResultSetAccessHandler.f_GetExceptionStr())));
					return;
				}
				if (!_ResultSetTranslateHostnames)
				{
					_Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to set translate hostnames: {}", _ResultSetTranslateHostnames.f_GetExceptionStr())));
					return;
				}
				if 
					(
						!fg_CombineResults
						(
							_Continuation
							, fg_Move(_WriteDatabaseResults)
						)
					)
				{
					return;
				}
				
				if (!_HostInfoChangedSubscription)
				{
					_Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to subscribe to info changes: {}", _HostInfoChangedSubscription.f_GetExceptionStr())));
					return;
				}					

				m_HostInfoChangedSubscription = fg_Move(*_HostInfoChangedSubscription);
				
				for (auto &Listen : _Listen)
					m_Listen[Listen];
				m_ServerCertificates = _ServerCertificates;
				
				for (auto &Namespace : _Namespaces)
				{
					auto &NamespaceName = _Namespaces.fs_GetKey(Namespace);
					if (!CActorDistributionManager::fs_IsValidNamespaceName(NamespaceName))
						continue;
					auto &NamespaceState = m_Namespaces[NamespaceName];
					NamespaceState.m_Namespace = Namespace;
					NamespaceState.m_bExistsInDatabase = true;
				}

				for (auto &Permission : _Permissions)
				{
					auto &HostID = _Permissions.fs_GetKey(Permission);
					auto &PermissionState = m_Permissions[HostID];
					PermissionState.m_Permissions = Permission;
					PermissionState.m_bExistsInDatabase = true;
				}

				for (auto &UserInfo : _Users)
				{
					auto &UserID = _Users.fs_GetKey(UserInfo);
					auto &UserState = m_Users[UserID];
					UserState.m_UserInfo = UserInfo;
					UserState.m_bExistsInDatabase = true;
				}

				for (auto &UserAuthenticationFactors : _AuthenticationFactors)
				{
					auto &UserID = _AuthenticationFactors.fs_GetKey(UserAuthenticationFactors);
					for (auto &Factor: UserAuthenticationFactors)
					{
						auto &AuthenticationFactor = m_UserAuthenticationFactors[UserID][UserAuthenticationFactors.fs_GetKey(Factor)];
						AuthenticationFactor.m_AuthenticationFactor = Factor;
						AuthenticationFactor.m_bExistsInDatabase = true;
					}
				}

				for (auto iClientConnection = _ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
				{
					NStr::CStr HostID;
					try
					{
						HostID = CActorDistributionManager::fs_GetCertificateHostID(iClientConnection->m_PublicServerCertificate);
					}
					catch (NException::CException const &_Exception)
					{
						_Continuation.f_SetException(DMibErrorInstance(fg_Format("Error getting host ID from certificate. Broken database? :{}", _Exception.f_GetErrorStr())));
						return;
					}
					if (HostID.f_IsEmpty())
					{
						_Continuation.f_SetException(DMibErrorInstance("Client connection has certificate with missing host id. Broken database?"));
						return;
					}

					m_ClientConnectionsInDatabase[iClientConnection.f_GetKey()];

					auto &ClientConnection = m_ClientConnections[iClientConnection.f_GetKey()];
					
					ClientConnection.m_ClientConnection = *iClientConnection;
					
					auto &Host = m_Hosts[HostID];
					Host.m_ClientConnections.f_Insert(ClientConnection);
					ClientConnection.m_pHost = &Host;
				}

				TCActorResultMap<CListenConfig, CDistributedActorListenReference> ListenResults;
				
				for (auto iListen = m_Listen.f_GetIterator(); iListen; ++iListen)
				{
					auto &Listen = iListen.f_GetKey();
					CActorDistributionListenSettings ListenSettings(NContainer::fg_CreateVector<NHTTP::CURL>(Listen.m_Address.m_URL));
					
					auto *pServerCert = m_ServerCertificates.f_FindEqual(Listen.m_Address.m_URL.f_GetHost());
					if (!pServerCert)
					{
						_Continuation.f_SetException(DMibErrorInstance("Invalid trust manager address in listen. No server certificate found. Broken database?"));
						return;
					}
					
					ListenSettings.m_PrivateKey = pServerCert->m_PrivateKey;
					ListenSettings.m_PublicCertificate = pServerCert->m_PublicCertificate;
					ListenSettings.m_CACertificate = m_BasicConfig.m_CACertificate;
					ListenSettings.m_bRetryOnListenFailure = m_bRetryOnListenFailureDuringInit;
					ListenSettings.m_ListenFlags = m_ListenFlags;
					
					m_ActorDistributionManager(&CActorDistributionManager::f_Listen, ListenSettings) > ListenResults.f_AddResult(Listen); 
				}

				TCActorResultMap<CDistributedActorTrustManager_Address, void> ConnectResults;
				
				NContainer::TCMap<CDistributedActorTrustManager_Address, TCContinuation<void>> ConnectionsToTimeout;
				
				for (auto iClientConnection = m_ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
				{
					auto &Address = iClientConnection.f_GetKey();
					TCContinuation<void> ConnectContinuation;
					
					m_ActorDistributionManager(&CActorDistributionManager::f_Connect, f_GetConnectionSettings(*iClientConnection))
						> [this, Address, ConnectContinuation](TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
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
								if (!ConnectContinuation.f_IsSet())
									ConnectContinuation.f_SetResult();
								return;
							}
							
							auto ConnectionMapped = m_ClientConnections(Address);
							DMibFastCheck(!ConnectionMapped.f_WasCreated());
							auto &ClientConnection = *ConnectionMapped;
							
							ClientConnection.m_ConnectionReferences.f_Insert(fg_Move(_ConnectionResult->m_ConnectionReference));
							f_ApplyConnectionConcurrency(ClientConnection);

							if (!ConnectContinuation.f_IsSet())
								ConnectContinuation.f_SetResult();
						}
					;
					
					ConnectionsToTimeout[Address] = ConnectContinuation;
					ConnectContinuation > ConnectResults.f_AddResult(Address);
				}
				
				fg_Timeout(m_InitialConnectionTimeout) > [this, ConnectionsToTimeout = fg_Move(ConnectionsToTimeout)]
					{
						for (auto &ConnectionContinuation : ConnectionsToTimeout)
						{
							if (ConnectionContinuation.f_IsSet())
								continue;

							ConnectionContinuation.f_SetException(DMibErrorInstance(NStr::fg_Format("Abandonned wait for initial connection after {} seconds", m_InitialConnectionTimeout)));
						}
					}
				;
				
				ListenResults.f_GetResults()
					+ ConnectResults.f_GetResults()
					> [this, _Continuation, pCleanup]
					(
						TCAsyncResult<NContainer::TCMap<CListenConfig, TCAsyncResult<CDistributedActorListenReference>>> &&_ListenResults
						, TCAsyncResult<NContainer::TCMap<CDistributedActorTrustManager_Address, TCAsyncResult<void>>> &&_ConnectionResult
					)
					{
						if (!_ListenResults)
						{
							_Continuation.f_SetException(_ListenResults);
							return;
						}

						if (!_ConnectionResult)
						{
							_Continuation.f_SetException(_ConnectionResult);
							return;
						}
						
						if
							(
								!fg_CombineResults
								(
									_Continuation
									, fg_Move(_ListenResults)
									, [&](CListenConfig const &_Config, CDistributedActorListenReference &&_ListenReference)
									{
										auto ListenMapped = m_Listen(_Config);
										DMibFastCheck(!ListenMapped.f_WasCreated());
										auto &Listen = *ListenMapped;
										
										Listen.m_ListenReference = fg_Move(_ListenReference);
									}
								)
							)
						{
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
						
						pCleanup->f_Clear();

						TCContinuation<void> PublishContinuation;
						if (m_bSupportAuthentication)
						{
							m_AuthenticationInterface.f_Publish<ICDistributedActorAuthentication>
								(
								 	m_ActorDistributionManager
								 	, m_pThis
								 	, ICDistributedActorAuthentication::mc_pDefaultNamespace
								) > PublishContinuation;
							;
						}
						else
							PublishContinuation.f_SetResult();

						m_TicketInterface = m_ActorDistributionManager->f_ConstructActor<CInternal::CTicketInterface>(this, fg_ThisActor(m_pThis));
						
						m_TicketInterface->f_Publish<CTicketInterface>("Anonymous/com.malterlib/Concurrency/TrustManagerTicket")
							+ PublishContinuation
							> [this, _Continuation]
							(TCAsyncResult<CDistributedActorPublication> &&_Result, TCAsyncResult<void> &&_Published)
							{
								if (!_Published)
									DMibLogWithCategory(Mib/Concurrency/App, Error, "Publishing authentication interface failed: {}", _Published.f_GetExceptionStr());
								else if (m_bSupportAuthentication)
									DMibLogWithCategory(Mib/Concurrency/App, Info, "Authentication interface published successfully");

								_Continuation.f_SetResult(m_BasicConfig.m_HostID);
								if (!_Result)
								{
									DMibLogWithCategory(Mib/Concurrency/App, Error, "Publishing ticket interface failed: {}", _Published.f_GetExceptionStr());
									return;
								}
								m_TicketInterfacePublication = fg_Move(*_Result);
							}
						; 
					}
				;
			}
		;
	}
}

#ifndef DMibPNoShortCuts
using namespace NMib::NConcurrency;
#endif
