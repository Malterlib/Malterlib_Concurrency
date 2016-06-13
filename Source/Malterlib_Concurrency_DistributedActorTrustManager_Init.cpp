// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include <Mib/Process/Platform>
#include <Mib/Network/SSL>

namespace NMib
{
	namespace NConcurrency
	{
		void CDistributedActorTrustManager::f_Construct()
		{
			fg_InitDistributedActorSystem();
			auto &Internal = *mp_pInternal;
			Internal.m_TicketInterface = fg_ConstructDistributedActor<CInternal::CTicketInterface>(&Internal, fg_ThisActor(this));
			Internal.m_pInitOnce = fg_Construct
				(
					TCActor<CActor>(fg_ThisActor(this)).f_Weak()
					, [this]
					{
						auto &Internal = *mp_pInternal; 
						return Internal.f_InitAttempt();
					}
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
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumServerCertificates)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumListenConfigs)
				+ m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClientConnections)
				> [this, Continuation]
				(
					TCAsyncResult<CBasicConfig> &&_BasicConfig 
					, TCAsyncResult<NContainer::TCSet<NStr::CStr>> &&_ServerCertificates
					, TCAsyncResult<NContainer::TCSet<CListenConfig>> &&_ListenConfigs
					, TCAsyncResult<NContainer::TCSet<CDistributedActorTrustManager_Address>> &&_ClientConnections
				)
				{
					if (!_BasicConfig)
						return Continuation.f_SetException(fg_Move(_BasicConfig));
					if (!_ServerCertificates)
						return Continuation.f_SetException(fg_Move(_ServerCertificates));
					if (!_ListenConfigs)
						return Continuation.f_SetException(fg_Move(_ListenConfigs));
					if (!_ClientConnections)
						return Continuation.f_SetException(fg_Move(_ClientConnections));
					
					TCActorResultMap<NStr::CStr, CServerCertificate> ServerCertificates;
					TCActorResultMap<CDistributedActorTrustManager_Address, CClientConnection> ClientConnections;

					for (auto &HostName : *_ServerCertificates)
						m_Database(&ICDistributedActorTrustManagerDatabase::f_GetServerCertificate, HostName) > ServerCertificates.f_AddResult(HostName);

					for (auto &Address : *_ClientConnections)
						m_Database(&ICDistributedActorTrustManagerDatabase::f_GetClientConnection, Address) > ClientConnections.f_AddResult(Address);
					
					ServerCertificates.f_GetResults()
						+ ClientConnections.f_GetResults()
						> 
						[
							this
							, Continuation
							, BasicConfig = fg_Move(*_BasicConfig)
							, ListenConfigs = fg_Move(*_ListenConfigs)
						]
						(
							TCAsyncResult<NContainer::TCMap<NStr::CStr, TCAsyncResult<CServerCertificate>>> &&_ServerCertificates
							, TCAsyncResult<NContainer::TCMap<CDistributedActorTrustManager_Address, TCAsyncResult<CClientConnection>>> &&_ClientConnections
						) mutable
						{
							NContainer::TCMap<NStr::CStr, CServerCertificate> ServerCertificates;
							if 
								(
									!fg_CombineResults
									(
										Continuation
										, fg_Move(_ServerCertificates)
										, [&](NStr::CStr const &_HostName, CServerCertificate &&_ServerCertificate)
										{
											ServerCertificates[_HostName] = fg_Move(_ServerCertificate);
										}
									)
								)
							{
								return;
							}
							
							NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> ClientConnections;
							if 
								(
									!fg_CombineResults
									(
										Continuation
										, fg_Move(_ClientConnections)
										, [&](CDistributedActorTrustManager_Address const &_Address, CClientConnection &&_ClientConnection)
										{
											ClientConnections[_Address] = fg_Move(_ClientConnection);
										}
									)
								)
							{
								return;
							}
							
							f_Init(Continuation, BasicConfig, ListenConfigs, ServerCertificates, ClientConnections);
						}
					;
				}
			;
			
			return Continuation;
		}
		
		void CDistributedActorTrustManager::CInternal::f_Init
			(
				TCContinuation<NStr::CStr> &_Continuation
				, CBasicConfig const &_Basic
				, NContainer::TCSet<CListenConfig> const &_Listen
				, NContainer::TCMap<NStr::CStr, CServerCertificate> const &_ServerCertificates
				, NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> const &_ClientConnections
			)
		{
			auto pCleanup = fg_OnScopeExitShared
				(
					[this]
					{
						m_Listen.f_Clear();
						m_ClientConnections.f_Clear();
						m_Hosts.f_Clear();
					}
				)
			;
			
			m_BasicConfig = _Basic;
			
			TCActorResultVector<void> WriteDatabaseResults;

			if (m_BasicConfig.m_HostID.f_IsEmpty())
			{
				// Generate root CA
				m_BasicConfig.m_HostID = NCryptography::fg_HighEntropyRandomID();
				
				NNet::CSSLContext::CCertificateOptions Options;
				Options.m_KeyLength = m_KeySize;
				Options.m_Subject = fg_Format("Malterlib Distributed Actors Root - {}", m_BasicConfig.m_HostID).f_Left(64); 
				auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
				Extension.m_bCritical = false; 
				Extension.m_Value = m_BasicConfig.m_HostID;
				
				try
				{
					NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey
						(
							Options
							, m_BasicConfig.m_CACertificate
							, m_BasicConfig.m_CAPrivateKey
							, 1
							, 100*365
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
				m_ActorDistributionManager = m_fDistributionManagerFactory(m_BasicConfig.m_HostID);
			else
			{
				NStr::CStr ResultingHostID = fg_InitDistributionManager(m_BasicConfig.m_HostID);
				if (ResultingHostID != m_BasicConfig.m_HostID)
				{
					_Continuation.f_SetException(DMibErrorInstance("Default distribution manager already initialized with the wrong host ID"));
					return;
				}
				m_ActorDistributionManager = fg_GetDistributionManager();
			}
			m_AccessHandler = fg_ConstructActor<CActorDistributionManagerAccessHandler>(this, fg_ThisActor(m_pThis));
			m_ActorDistributionManager(&CActorDistributionManager::f_SetAccessHandler, m_AccessHandler)
				+ WriteDatabaseResults.f_GetResults()
				> 
				[
					this
					,_Continuation
					, _Listen
					, _ServerCertificates
					, _ClientConnections
					, pCleanup
				]
				(TCAsyncResult<void> &&_Result, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_WriteDatabaseResults)
				{
					if (!_Result)
					{
						_Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to set access handler : {}", _Result.f_GetExceptionStr())));
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
					
					
					for (auto &Listen : _Listen)
						m_Listen[Listen];
					m_ServerCertificates = _ServerCertificates;
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
						ListenSettings.m_bRetryOnListenFailure = true;
						
						m_ActorDistributionManager(&CActorDistributionManager::f_Listen, ListenSettings) > ListenResults.f_AddResult(Listen); 
					}
					
					TCActorResultMap<CDistributedActorTrustManager_Address, CActorDistributionManager::CConnectionResult> ConnectResults;
					
					for (auto iClientConnection = m_ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
					{
						auto &ClientConnection = iClientConnection->m_ClientConnection;
						auto &Address = iClientConnection.f_GetKey();
						
						CActorDistributionConnectionSettings ClientSettings;
						ClientSettings.m_ServerURL = Address.m_URL;
						ClientSettings.m_PublicServerCertificate = ClientConnection.m_PublicServerCertificate;
						ClientSettings.m_PublicClientCertificate = ClientConnection.m_PublicClientCertificate;
						ClientSettings.m_PrivateClientKey = m_BasicConfig.m_CAPrivateKey;
						ClientSettings.m_bRetryConnectOnFailure = true;

						m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ClientSettings) > ConnectResults.f_AddResult(Address); 
					}
					
					ListenResults.f_GetResults() 
						+ ConnectResults.f_GetResults() 
						> [this, _Continuation, pCleanup]
						(
							
							TCAsyncResult<NContainer::TCMap<CListenConfig, TCAsyncResult<CDistributedActorListenReference>>> &&_ListenResults
							, TCAsyncResult<NContainer::TCMap<CDistributedActorTrustManager_Address, TCAsyncResult<CActorDistributionManager::CConnectionResult>>> &&_ConnectionResult
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

							if 
								(
									!fg_CombineResults
									(
										_Continuation
										, fg_Move(_ConnectionResult)
										, [&](CDistributedActorTrustManager_Address const &_Address, CActorDistributionManager::CConnectionResult &&_ConnectionResult)
										{
											auto ConnectionMapped = m_ClientConnections(_Address);
											DMibFastCheck(!ConnectionMapped.f_WasCreated());
											auto &ClientConnection = *ConnectionMapped;
											
											ClientConnection.m_ConnectionReference = fg_Move(_ConnectionResult.m_ConnectionReference);
										}
									)
								)
							{
								return;
							}
							
							pCleanup->f_Clear();
							
							m_ActorDistributionManager
								(
									&CActorDistributionManager::f_PublishActor
									, m_TicketInterface
									, "Anonymous/MalterlibDistributedTrustManagerTicket"
									, CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy<CTicketInterface>()
								)
								> [this](TCAsyncResult<CDistributedActorPublication> &&_Result)
								{
									if (!_Result)
									{
										// TODO: Log
										return;
									}
									m_TicketInterfacePublication = fg_Move(*_Result);
								}
							; 
							
							_Continuation.f_SetResult(m_BasicConfig.m_HostID);
						}
					;
				}
			;
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
