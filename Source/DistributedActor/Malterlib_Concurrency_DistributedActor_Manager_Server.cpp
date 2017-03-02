// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/Actor/Timer>

namespace NMib
{
	namespace NConcurrency
	{
		CDistributedActorListenReference::CDistributedActorListenReference(CDistributedActorListenReference &&) = default;
		CDistributedActorListenReference &CDistributedActorListenReference::operator = (CDistributedActorListenReference &&) = default;
			
		CDistributedActorListenReference::CDistributedActorListenReference(TCWeakActor<CActorDistributionManager> const &_DistributionManager, NStr::CStr const &_ListenID)
			: mp_DistributionManager(_DistributionManager)
			, mp_ListenID(_ListenID) 
		{
		}

		CDistributedActorListenReference::CDistributedActorListenReference() = default;

		CDistributedActorListenReference::~CDistributedActorListenReference()
		{
			f_Clear();
		}
		
		void CDistributedActorListenReference::f_Clear()
		{
			if (mp_DistributionManager)
			{
				if (auto DistributionManager = mp_DistributionManager.f_Lock())
					DistributionManager(&CActorDistributionManager::fp_RemoveListen, mp_ListenID) > fg_DiscardResult();
				
				mp_DistributionManager.f_Clear();
				mp_ListenID.f_Clear();
			}
		}
		
		auto CDistributedActorListenReference::f_Stop() 
			-> TCActorCall
			<
				TCActor<CConcurrentActor>
				, TCContinuation<void> (CActor::*)(NFunction::TCFunctionMovable<TCContinuation<void> ()> &&)
				, NContainer::TCTuple<NFunction::TCFunctionMovable<TCContinuation<void> ()>>
				, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCContinuation<void> ()>> 
			>
		{
			bool bAlreadyStopped = mp_DistributionManager.f_IsEmpty();
			auto DistributionManager = mp_DistributionManager.f_Lock();
			mp_DistributionManager.f_Clear();
			return fg_ConcurrentActor().f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<void>>
					, NFunction::TCFunctionMovable<TCContinuation<void> ()>
					(
						[bAlreadyStopped, DistributionManager = fg_Move(DistributionManager), ListenID = fg_Move(mp_ListenID)]
						{
							TCContinuation<void> Continuation;
							if (DistributionManager)
							{
								DistributionManager(&CActorDistributionManager::fp_RemoveListen, ListenID) > [Continuation](TCAsyncResult<void> &&_Result)
									{
										Continuation.f_SetResult(fg_Move(_Result));
									}
								;
								return Continuation;
							}
							if (bAlreadyStopped)
								Continuation.f_SetException(DMibErrorInstance("Listen has already been stopped"));
							else
								Continuation.f_SetException(DMibErrorInstance("Distribution manager has been deleted"));
							
							return Continuation;
						}
					)
				)
			;
		}		
		
		void CActorDistributionManagerInternal::fp_DestroyServerConnection(CServerConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error)
		{
			auto *pConnection = m_ServerConnections.f_FindEqual(_Connection.m_ConnectionID);
			if (pConnection)
			{
				_Connection.f_Destroy(_Error);
				
				auto &pHost = _Connection.m_pHost;
				if (!_bSaveHost && pHost && pHost->m_bAnonymous)
				{
					fp_DestroyHost(*pHost, &_Connection);
					pHost = nullptr;
				}
				m_ServerConnections.f_Remove(pConnection);
			}
		}

		TCContinuation<CActorSubscription> CActorDistributionManager::f_RegisterWebsocketHandler
			(
				NStr::CStr const &_Path  
				, TCActor<> const &_Actor
				, NFunction::TCFunctionMutable
				<
					TCContinuation<void> (NPtr::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pNewServerConnection, NStr::CStr const &_RealHostID)
				>
				&&_fNewWebsocketConnection
			)
		{
			auto &Internal = *mp_pInternal;
			if (Internal.m_WebsocketHandlers.f_FindEqual(_Path))
				return DMibErrorInstance("Path already registered");
			auto &Handler = Internal.m_WebsocketHandlers[_Path];
			Handler.m_Actor = _Actor.f_Weak();
			Handler.m_fNewWebsocketConnection = fg_Move(_fNewWebsocketConnection);
			
			return fg_Explicit
				(
					g_ActorSubscription > [this, _Path]
					{
						auto &Internal = *mp_pInternal;
						Internal.m_WebsocketHandlers.f_Remove(_Path);
					}
				)
			;
		}
		
		void CActorDistributionManagerInternal::fp_Listen
			(
				NStr::CStr const &_ListenID
				, CActorDistributionListenSettings const &_Settings
				, NPtr::TCSharedPointer<TCContinuation<CDistributedActorListenReference>> const &_pContinuation
			)
		{
			TCActorResultVector<NMib::NNet::CNetAddress> ResolvedAddresses;
			for (auto &Address : _Settings.m_ListenAddresses)
				m_ResolveActor(&NNet::CResolveActor::f_Resolve, fp_TranslateHostname(Address.f_GetHost()), NNet::ENetAddressType_None) > ResolvedAddresses.f_AddResult(); 

			ResolvedAddresses.f_GetResults() > [this, _Settings, _pContinuation, _ListenID](TCAsyncResult<NContainer::TCVector<TCAsyncResult<NMib::NNet::CNetAddress>>> &&_Results)
				{
					auto fReportListenFailure = [this, _Settings, _pContinuation, _ListenID](CExceptionPointer _Error, NStr::CStr const &_ErrorString)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/Actors
									, Error
									, "{}"
									, _ErrorString
								)
							;
							if (_Settings.m_bRetryOnListenFailure)
							{
								m_Listens[_ListenID];
								
								fg_TimerActor()
									(
										&CTimerActor::f_OneshotTimer
										, 0.5
										, fg_ThisActor(m_pThis)
										, [this, _pContinuation, _Settings, _ListenID]() mutable
										{
											if (!m_Listens.f_FindEqual(_ListenID))
												return; // Removed already
											fp_Listen(_ListenID, _Settings, nullptr);
										}
									)
									> fg_DiscardResult()
								;
								
								if (_pContinuation)
									_pContinuation->f_SetResult(CDistributedActorListenReference(fg_ThisActor(m_pThis), _ListenID));
							}
							else if (_pContinuation)
								_pContinuation->f_SetException(fg_Move(_Error));
							return;
						}
					;
					
					if (!_Results)
					{
						fReportListenFailure(_Results.f_GetException(), fg_Format("Failed to resolve addresses: {}", _Results.f_GetExceptionStr()));
						return;
					}
					
					NContainer::TCVector<NNet::CNetAddress> Addresses;
					
					mint iResult = 0;
					for (auto &Result : *_Results)
					{
						if (!Result)
						{
							fReportListenFailure(Result.f_GetException(), fg_Format("Failed to resolve address: {}", Result.f_GetExceptionStr()));
							return;
						}
						auto Address = fg_Move(*Result);
						auto Port = _Settings.m_ListenAddresses[iResult].f_GetPortFromScheme();
						++iResult;
						if (Port)
							Address.f_SetPort(Port); 
						Addresses.f_Insert(fg_Move(Address));
					}
					
					NNet::CSSLSettings ServerSettings;

					ServerSettings.m_PublicCertificateData = _Settings.m_PublicCertificate;
					ServerSettings.m_PrivateKeyData = _Settings.m_PrivateKey;
					ServerSettings.m_CACertificateData = _Settings.m_CACertificate;
					ServerSettings.m_VerificationFlags |= NNet::CSSLSettings::EVerificationFlag_AllowMissingPeerCertificate;
					
					NPtr::TCSharedPointer<NNet::CSSLContext> pServerContext;
					
					try
					{
						 pServerContext = fg_Construct(NNet::CSSLContext::EType_Server, ServerSettings);
					}
					catch (NException::CException const &_Exception)
					{
						if (_pContinuation)
						{
							_pContinuation->f_SetException(DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr())));
						}
						return;
					}
					
					NPtr::TCSharedPointer<CListen> pListenState = fg_Construct(); 
					
					pListenState->m_WebsocketServer = fg_ConstructActor<NWeb::CWebSocketServerActor>();
					pListenState->m_WebsocketServer
						(
							&NWeb::CWebSocketServerActor::f_StartListenAddress
							, fg_Move(Addresses)
							, _Settings.m_ListenFlags
							, fg_ThisActor(m_pThis)
							, [this](NWeb::CWebSocketNewServerConnection &&_NewServerConnection)
							{
								NPtr::TCSharedPointer<NWeb::CWebSocketNewServerConnection> pNewServerConnection = fg_Construct(fg_Move(_NewServerConnection));
								
								NWeb::CWebSocketNewServerConnection &NewServerConnection = *pNewServerConnection;
								
								auto fReject = [pNewServerConnection](NStr::CStr const &_Error)
									{
										DMibLogWithCategory(Mib/Concurrency/Actors, Error, "Rejected connection: {} - {}", pNewServerConnection->m_Info.m_PeerAddress.f_GetString(), _Error);
										pNewServerConnection->f_Reject(_Error);
									}
								;
								
								if (m_WebsocketHandlers.f_IsEmpty() && NewServerConnection.m_Protocols.f_Contains("MalterlibDistributedActors") < 0)
								{
									fReject("Unsupported protocol, only MalterlibDistributedActors is supported");
									return;
								}
								
								if (!NewServerConnection.m_Info.m_pSocketInfo.f_Get())
								{
									fReject("Missing socket info");
									return;
								}
								
								auto pSocketInfo = static_cast<NNet::CSocketConnectionInfo_SSL const *>(NewServerConnection.m_Info.m_pSocketInfo.f_Get());
								NStr::CStr UniqueHostID;
								NStr::CStr RealHostID;
								
								NStr::CStr Enclave;
								NStr::CStr WebPath;
								if (NewServerConnection.m_Info.m_pRequest)
								{
									auto &Request = *NewServerConnection.m_Info.m_pRequest; 
									if (auto *pValue = NewServerConnection.m_Info.m_pRequest->f_GetEntityFields().f_GetUnknownField("MalterlibDistributedActors_Enclave"))
										Enclave = *pValue;
									WebPath = Request.f_GetRequestLine().f_GetURI().f_GetFullPath();
								}
								
								if (!Enclave.f_IsEmpty() && !CActorDistributionManager::fs_IsValidEnclave(Enclave))
								{
									fReject("Invalid enclave");
									return;
								}
								
								bool bAnonymous = false;
								
								if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
								{
									bAnonymous = true;
									UniqueHostID = RealHostID = fg_Format("Anonymous_{}", NCryptography::fg_RandomID());
								}
								else
								{
									try
									{
										NException::CDisableExceptionTraceScope DisableTrace;
										UniqueHostID = RealHostID = CActorDistributionManager::fs_GetCertificateHostID(pSocketInfo->m_PeerCertificate);
										if (RealHostID.f_IsEmpty())
										{
											fReject("Missing or incorrect Host ID in peer certificate");
											return;
										}
										if (!Enclave.f_IsEmpty())
										{
											UniqueHostID += "_";
											UniqueHostID += Enclave;
										}
									}
									catch (NException::CException const &_Exception)
									{
										fReject("Incorrect peer certificate");
										return;
									}
								}

								auto fFinishConnection = [this, UniqueHostID, RealHostID, bAnonymous, fReject, pNewServerConnection, WebPath]() mutable
									{
										NWeb::CWebSocketNewServerConnection &NewServerConnection = *pNewServerConnection;

										auto *pHandler = m_WebsocketHandlers.f_FindLargestLessThanEqual(WebPath);
										
										if (pHandler && WebPath.f_StartsWith(pHandler->f_GetWebPath()))
										{
											auto Actor = pHandler->m_Actor.f_Lock();
											if (!Actor)
											{
												fReject("Web socket handler no longer exists");
												return;
											}
											fg_Dispatch
												(
													Actor
													,[fNewWebsocketConnection = pHandler->m_fNewWebsocketConnection, pNewServerConnection, RealHostID]() mutable
													{
														return fNewWebsocketConnection(pNewServerConnection, RealHostID);
													}
												)
												> [fReject](TCAsyncResult<void> &&_Result)
												{
													if (!_Result)
														fReject("Internal error calling web socket handler");
												}
											;
											return;
										}
										
										if (NewServerConnection.m_Protocols.f_Contains("MalterlibDistributedActors") < 0)
										{
											fReject("Unsupported protocol, only MalterlibDistributedActors is supported");
											return;
										}
										
										mint ConnectionID = m_NextConnectionID++;
										
										NPtr::TCSharedPointer<CServerConnection, NPtr::CSupportWeakTag> pConnection = fg_Construct(ConnectionID);
										
										auto MappedHost = m_Hosts(UniqueHostID);
										if (MappedHost.f_WasCreated())
										{
											*MappedHost = fg_Construct(*m_pThis);
											auto &Host = **MappedHost;
											Host.m_RealHostID = RealHostID;
											Host.m_UniqueHostID = UniqueHostID;
											Host.m_bAnonymous = bAnonymous;
											m_HostsByRealHostID[RealHostID].f_Insert(Host);
										}
										
										auto &Host = **MappedHost;
										if (Host.m_bAnonymous != bAnonymous)
										{
											fReject("Mismatching anonymous for host");
											return;
										}
										if (Host.m_RealHostID != RealHostID)
										{
											fReject("Mismatching real host ID for host");
											return;
										}

										m_ServerConnections[ConnectionID] = pConnection;
										pConnection->m_pHost = *MappedHost;
										Host.m_ServerConnections.f_Insert(*pConnection);

										Host.m_bIncoming = true;
										pConnection->m_bIncoming = true;
										
										NewServerConnection.m_fOnClose = [this, pConnectionWeak = pConnection.f_Weak(), Address = NewServerConnection.m_Info.m_PeerAddress]
											(NWeb::EWebSocketStatus _Reason, NStr::CStr const& _Message, NWeb::EWebSocketCloseOrigin _Origin)
											{
												auto pConnection = pConnectionWeak.f_Lock();
												if (!pConnection)
													return;
												if (!pConnection->m_pHost)
													return;
												NStr::CStr CloseMessage = fg_Format
													(
														"Lost incoming connection({}) from '{}' <{}>: {} {} {}"
														, pConnection->f_GetConnectionID()
														, Address.f_GetString()
														, pConnection->f_GetHostInfo().f_GetDesc()
														, _Origin == NWeb::EWebSocketCloseOrigin_Local ? "Local" : "Remote"
														, _Reason
														, _Message
													)
												;
												
												if (_Reason == NWeb::EWebSocketStatus_NormalClosure)
													DMibLogWithCategory(Mib/Concurrency/Actors, Info, "{}", CloseMessage);
												else
													DMibLogWithCategory(Mib/Concurrency/Actors, Error, "{}", CloseMessage);
												fp_DestroyServerConnection(*pConnection, false, CloseMessage);
											}
										;
										
										NewServerConnection.m_fOnReceiveBinaryMessage = [this, pConnectionWeak = pConnection.f_Weak()]
											(NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage)
											{
												auto pConnection = pConnectionWeak.f_Lock();
												if (!pConnection)
													return;
												if (!pConnection->m_pHost)
													return;
												
												if (!fp_HandleProtocolIncoming(pConnection.f_Get(), _pMessage))
												{
													// TODO: Assume malicious client
												}
											}
										;
										
										pConnection->m_IdentifyContinuation = TCContinuation<void>();
										NHTTP::CResponseHeader ResponseHeader;
										ResponseHeader.f_SetStatus(NHTTP::EStatus_SwitchingProtocols);
										auto &EntityFields = ResponseHeader.f_GetEntityFields();
										EntityFields.f_SetUnknownField("MalterlibDistributedActors_Enclave", m_Enclave);

										pConnection->m_Connection = NewServerConnection.f_Accept
											(
												"MalterlibDistributedActors"
												, fg_ThisActor(m_pThis) / [this, pConnectionWeak = pConnection.f_Weak(), Address = NewServerConnection.m_Info.m_PeerAddress]
												(NConcurrency::TCAsyncResult<NConcurrency::CActorSubscription> &&_Subscription)
												{
													if (_Subscription)
													{
														auto pConnection = pConnectionWeak.f_Lock();
														if (!pConnection)
															return;
														if (!pConnection->m_pHost)
															return;
														if (!pConnection->m_Connection)
															return;
														pConnection->m_IdentifyContinuation.f_Dispatch() > [Address, pConnectionWeak](TCAsyncResult<void> &&_Result) mutable
															{
																auto pConnection = pConnectionWeak.f_Lock();
																if (!pConnection)
																	return;
																
																if (!_Result)
																{
																	DMibLogWithCategory
																		(
																			Mib/Concurrency/Actors
																			, Error
																			, "Error identifying connection({}) from '{}': {}"
																			, pConnection->f_GetConnectionID()
																			, Address.f_GetString()
																			, _Result.f_GetExceptionStr()
																		)
																	;
																	return;
																}
																if (!pConnection->m_pHost)
																	return;
																DMibLogWithCategory
																	(
																		Mib/Concurrency/Actors
																		, Info
																		, "Accepted connection({}) from '{}' <{}>"
																		, pConnection->f_GetConnectionID()
																		, Address.f_GetString()
																		, pConnection->f_GetHostInfo().f_GetDesc()
																	)
																;
															}
														;
														pConnection->m_ConnectionSubscription = fg_Move(*_Subscription);
														fp_Identify(pConnection.f_Get());
													}
													else
													{
														DMibLogWithCategory
															(
																Mib/Concurrency/Actors
																, Error
																, "Error accepting connection from '{}': {}"
																, Address.f_GetString()
																, _Subscription.f_GetExceptionStr()
															)
														;
													}
												}
												, fg_Move(ResponseHeader)
											)
										;
									}
								;
								
								if (!bAnonymous && m_AccessHandler)
								{
									m_AccessHandler(&ICActorDistributionManagerAccessHandler::f_ValidateClientAccess, RealHostID, pSocketInfo->m_CertificateChain) 
										> [fReject, fFinishConnection](TCAsyncResult<NStr::CStr> &&_Result) mutable
										{
											if (!_Result)
											{
												fReject(_Result.f_GetExceptionStr());
												return;
											}
											auto &Error = *_Result;
											if (!Error.f_IsEmpty())
											{
												fReject(Error);
												return;
											}
											
											fFinishConnection();
										}
									;
								}
								else
									fFinishConnection();
							}
							, [](NWeb::CWebSocketActor::CConnectionInfo &&_ConnectionInfo)
							{
								DMibLogWithCategory
									(
										Mib/Concurrency/Actors
										, Error
										, "Websocket rejected connection: {} - {} {}"
										, _ConnectionInfo.m_PeerAddress.f_GetString()
										, _ConnectionInfo.m_ErrorStatus
										, _ConnectionInfo.m_Error
									)
								;
							}
							, NNet::CSocket_SSL::fs_GetFactory(pServerContext)
						)
						> [this, _ListenID, _pContinuation, _Settings, pListenState, fReportListenFailure](TCAsyncResult<CActorSubscription> &&_Result) mutable
						{
							if (!_Result)
							{
								
								fReportListenFailure(_Result.f_GetException(), fg_Format("Failed to start listen on ({vs,vb}): {}", _Settings.m_ListenAddresses, _Result.f_GetExceptionStr()));
								return;
							}
							
							auto &Listen = m_Listens[_ListenID] = fg_Move(*pListenState);
							pListenState.f_Clear();
							
							Listen.m_ListenCallbackSubscription = fg_Move(*_Result);
							if (_pContinuation)
								_pContinuation->f_SetResult(CDistributedActorListenReference(fg_ThisActor(m_pThis), _ListenID));
						}
					;
				}
			;
		}
		
		TCContinuation<CDistributedActorListenReference> CActorDistributionManager::f_Listen(CActorDistributionListenSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<CDistributedActorListenReference> Continuation;
			
			Internal.fp_Listen(NCryptography::fg_RandomID(), _Settings, fg_Construct(Continuation));
			
			return Continuation;
		}
		
		void CActorDistributionManager::fp_RemoveListen(NStr::CStr const &_ListenID)
		{
			auto &Internal = *mp_pInternal;
			auto *pListen = Internal.m_Listens.f_FindEqual(_ListenID);
			if (pListen)
			{
				pListen->m_ListenCallbackSubscription.f_Clear();
				if (pListen->m_WebsocketServer)
				{
					pListen->m_WebsocketServer->f_DestroyNoResult(DMibPFile, DMibPLine);
					pListen->m_WebsocketServer.f_Clear();
				}
				Internal.m_Listens.f_Remove(pListen);
			}
		}
	}
}
