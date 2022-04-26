// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Cryptography/Certificate>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	CDistributedActorTrustManager::CInternal::CTicketInterface::CTicketInterface
		(
			CDistributedActorTrustManager::CInternal *_pInternal
			, TCWeakActor<CDistributedActorTrustManager> const &_ThisActor
		)
		: mp_pInternal(_pInternal)
		, mp_ThisActor(_ThisActor)
	{
		DMibPublishActorFunction(CTicketInterface::f_SignCertificate);
	}

	TCFuture<NContainer::CByteVector> CDistributedActorTrustManager::CInternal::f_SignCertificate
		(
			NStr::CStr _Token
			, NContainer::CByteVector _CertificateRequest
			, CCallingHostInfo _HostInfo
		)
	{
		auto *pTicket = m_Tickets.f_FindEqual(_Token);

		if (!pTicket)
			co_return DMibErrorInstance("Invalid token");

		auto Age = m_TicketTimer.f_Elapsed() - pTicket->m_CreationTime;

		auto fOnUseTicket = fg_Move(pTicket->m_fOnUseTicket);
		auto fOnCertificateSigned = fg_Move(pTicket->m_fOnCertificateSigned);

		m_Tickets.f_Remove(_Token);

		if (Age.f_GetTime() > 60.0*60.0) // 1 hour ticket time
			co_return DMibErrorInstance("Invalid token");

		auto HostID = CActorDistributionManager::fs_GetCertificateRequestHostID(_CertificateRequest);

		if (HostID.f_IsEmpty())
			co_return DMibErrorInstance("Certificate request has no host ID");

		auto pExistingClient = co_await (m_Database(&ICDistributedActorTrustManagerDatabase::f_TryGetClient, HostID) % "Failed to check for existing client");
		bool bExistingClient = !pExistingClient.f_IsEmpty();
		if (bExistingClient)
		{
			try
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				NCryptography::CCertificate::fs_VerifyCertificateRequestSameKeyAsCertificate(_CertificateRequest, pExistingClient->m_PublicCertificate);
			}
			catch (NException::CException const &_Exception)
			{
				co_return DMibErrorInstance(fg_Format("Fraudulent client sign attempt: {}", _Exception.f_GetErrorStr()));
			}
		}

		if (fOnUseTicket)
			co_await (fOnUseTicket(HostID, _HostInfo, _CertificateRequest) % "Certificate validation failed");

		int32 Serial = co_await (m_Database(&ICDistributedActorTrustManagerDatabase::f_GetNewCertificateSerial) % "Failed to get new certificate serial");

		struct CResults
		{
			NContainer::CByteVector m_SignedCertificate;
			NStr::CStr m_HostID;
		};

		CResults Results = co_await
			(
				g_ConcurrentDispatch /
				[
					CaCertificate = m_BasicConfig.m_CACertificate
					, CaPrivateKey = m_BasicConfig.m_CAPrivateKey
					, _CertificateRequest
					, Serial
				]
				{
					CResults Results;
					try
					{
						NCryptography::CCertificateSignOptions SignOptions;
						SignOptions.m_Serial = Serial;
						SignOptions.m_Days = 10*365;

						NException::CDisableExceptionTraceScope DisableTrace;
						NCryptography::CCertificate::fs_SignClientCertificate
							(
								CaCertificate
								, CaPrivateKey
								, _CertificateRequest
								, Results.m_SignedCertificate
								, SignOptions
							)
						;

						Results.m_HostID = CActorDistributionManager::fs_GetCertificateHostID(Results.m_SignedCertificate);
						if (Results.m_HostID.f_IsEmpty())
							DMibError("Missing host ID in certificate request");
					}
					catch (NException::CException const &_Exception)
					{
						DMibError(fg_Format("Failed to sign client certificate request: {}", _Exception.f_GetErrorStr()));
					}

					return Results;
				}
			)
		;

		ICDistributedActorTrustManagerDatabase::CClient Client;
		Client.m_PublicCertificate = Results.m_SignedCertificate;
		co_await
			(
				m_Database
				(
					bExistingClient ? &ICDistributedActorTrustManagerDatabase::f_SetClient : &ICDistributedActorTrustManagerDatabase::f_AddClient
					, Results.m_HostID
					, Client
				)
				% "Failed to add client to trust database"
			)
		;

		if (!fOnCertificateSigned)
			co_return fg_Move(Results.m_SignedCertificate);

		fOnCertificateSigned(HostID, _HostInfo) > fg_LogError("Mib/Concurrency/Trust", "On certifigace signed notification failed");

		co_return fg_Move(Results.m_SignedCertificate);
	}

	TCFuture<NContainer::CByteVector> CDistributedActorTrustManager::CInternal::CTicketInterface::f_SignCertificate
		(
			NStr::CStr const &_Token
			, NContainer::CByteVector const &_CertificateRequest
		)
	{
		TCPromise<NContainer::CByteVector> Promise;
		return Promise <<= fg_Dispatch
			(
				mp_ThisActor
				, [pInternal = mp_pInternal, _Token, _CertificateRequest, CallingHostInfo = fg_GetCallingHostInfo()]
				{
					return pInternal->f_SignCertificate(_Token, _CertificateRequest, CallingHostInfo);
				}
			)
		;
	}

	CDistributedActorTrustManager::CInternal::CActorDistributionManagerAccessHandler::CActorDistributionManagerAccessHandler
		(
			CDistributedActorTrustManager::CInternal *_pInternal
		)
		: m_pThis(_pInternal)
	{
	}

	TCFuture<NStr::CStr> CDistributedActorTrustManager::CInternal::CActorDistributionManagerAccessHandler::f_ValidateClientAccess
		(
			NStr::CStr const &_HostID
			, NContainer::TCVector<NContainer::CByteVector> const &_CertificateChain
		)
	{
		return m_pThis->f_ValidateClientAccess(_HostID, _CertificateChain);
	}

	TCFuture<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> CDistributedActorTrustManager::f_GenerateConnectionTicket
		(
			CDistributedActorTrustManager_Address const &_Address
			, TCActorFunctor<TCFuture<void> (NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest)> &&_fOnUseTicket
			, TCActorFunctor<TCFuture<void> (NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo)> &&_fOnCertificateSigned
		)
	{
		TCPromise<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> Promise;

		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _Address, fOnUseTicket = fg_Move(_fOnUseTicket), fOnCertificateSigned = fg_Move(_fOnCertificateSigned)]() mutable
				{
					auto &Internal = *mp_pInternal;

					CListenConfig ListenConfig;
					ListenConfig.m_Address = _Address;
					auto *pListen = Internal.m_Listen.f_FindEqual(ListenConfig);
					if (!pListen)
					{
						Promise.f_SetException(DMibErrorInstance("Could not find listen with this address"));
						return;
					}

					auto *pServerCertificate = Internal.m_ServerCertificates.f_FindEqual(_Address.m_URL.f_GetHost());
					if (!pServerCertificate)
					{
						Promise.f_SetException(DMibErrorInstance("Could not find and server certificate for this address"));
						return;
					}

					CDistributedActorTrustManager::CTrustTicket TrustTicket;

					TrustTicket.m_Token = NCryptography::fg_RandomID();
					TrustTicket.m_ServerAddress = _Address;
					TrustTicket.m_ServerPublicCert = Internal.m_BasicConfig.m_CACertificate;

					auto &TicketState = Internal.m_Tickets[TrustTicket.m_Token];
					TicketState.m_CreationTime = Internal.m_TicketTimer.f_Elapsed();
					TicketState.m_fOnUseTicket = fg_Move(fOnUseTicket);
					TicketState.m_fOnCertificateSigned = fg_Move(fOnCertificateSigned);

					CTrustGenerateConnectionTicketResult Result;
					Result.m_Ticket = fg_Move(TrustTicket);
					if (TicketState.m_fOnUseTicket || TicketState.m_fOnCertificateSigned)
					{
						Result.m_NotificationsSubscription = g_ActorSubscription / [this, Token = TrustTicket.m_Token]() -> TCFuture<void>
							{
								auto &Internal = *mp_pInternal;

								auto *pTicket = Internal.m_Tickets.f_FindEqual(Token);
								if (!pTicket)
									co_return {};

								TCActorResultVector<void> Destroys;
								fg_Move(pTicket->m_fOnUseTicket).f_Destroy() > Destroys.f_AddResult();
								fg_Move(pTicket->m_fOnCertificateSigned).f_Destroy() > Destroys.f_AddResult();

								Internal.m_Tickets.f_Remove(Token);

								co_await Destroys.f_GetResults();

								co_return {};
							}
						;
					}

					Promise.f_SetResult(fg_Move(Result));
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	void CDistributedActorTrustManager::CInternal::f_RemoveClientConnection(CConnectionState *_pClientConnection)
	{
		auto pHost = _pClientConnection->m_pHost;
		if (pHost)
		{
			pHost->m_ClientConnections.f_Remove(*_pClientConnection);
			if (pHost->m_ClientConnections.f_IsEmpty())
				m_Hosts.f_Remove(pHost);
		}

		m_ClientConnections.f_Remove(_pClientConnection);
	}

	void CDistributedActorTrustManager::CInternal::f_ApplyConnectionConcurrency(CConnectionState &_ConnectionState)
	{
		mint ConnectionConcurrency = _ConnectionState.m_ClientConnection.f_GetEffectiveConnectionConcurrency(m_DefaultConnectionConcurrency);
		if (ConnectionConcurrency == _ConnectionState.m_ConnectionReferences.f_GetLen())
			return;

		if (ConnectionConcurrency <= _ConnectionState.m_ConnectionReferences.f_GetLen())
		{
			_ConnectionState.m_ConnectionReferences.f_SetLen(ConnectionConcurrency);
			_ConnectionState.m_ConnectionReferences[0].f_Reconnect() > fg_DiscardResult();
			return;
		}

		auto Address = _ConnectionState.f_GetAddress();
		auto ConnectionSettings = f_GetConnectionSettings(_ConnectionState);

		for (int32 ToCreate = ConnectionConcurrency - _ConnectionState.m_ConnectionReferences.f_GetLen(); ToCreate > 0; --ToCreate)
		{
			m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
				> [=](TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
				{
					auto *pConnectionState = m_ClientConnections.f_FindEqual(Address);
					if (!pConnectionState || pConnectionState->m_bRemoving)
						return;

					if (!_ConnectionResult)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Trust
								, Error
								, "Failed add additional connection concurrency: {}"
								, _ConnectionResult.f_GetExceptionStr()
							)
						;
						return;
					}

					mint ConnectionConcurrency = pConnectionState->m_ClientConnection.f_GetEffectiveConnectionConcurrency(m_DefaultConnectionConcurrency);
					if (pConnectionState->m_ConnectionReferences.f_GetLen() >= ConnectionConcurrency)
						return; // Settings changed asyncronously

					auto &ConnectionResult = *_ConnectionResult;

					if (pConnectionState->m_pHost && !ConnectionResult.m_HostInfo.m_HostID.f_IsEmpty())
					{
						DMibCheck(ConnectionResult.m_HostInfo.m_HostID == pConnectionState->m_pHost->f_GetHostID())
							(ConnectionResult.m_HostInfo.m_HostID)
							(pConnectionState->m_pHost->f_GetHostID())
						;
					}

					pConnectionState->m_ConnectionReferences.f_Insert(fg_Move(ConnectionResult.m_ConnectionReference));
				}
			;
		}
	}

	TCFuture<CHostInfo> CDistributedActorTrustManager::f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency)
	{
		TCPromise<CHostInfo> Promise;

		// Connect to remote host with anonymous connection
		// Subscribe to internal interface (time out if no publication arrives)
		// Generate certificate request and send to remote host
		// Save client connection to database
		// Connect again to remote host with signed client certificate (if failure, remove from database again)

		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _TrustTicket, _Timeout, _ConnectionConcurrency]
				{
					NStr::CStr ServerHostID;
					try
					{
						NException::CDisableExceptionTraceScope DisableTrace;
						ServerHostID = CActorDistributionManager::fs_GetCertificateHostID(_TrustTicket.m_ServerPublicCert);
					}
					catch (NException::CException const &_Exception)
					{
						Promise.f_SetException(DMibErrorInstance(fg_Format("Error getting server host ID from certificate: {}", _Exception.f_GetErrorStr())));
						return;
					}

					if (ServerHostID.f_IsEmpty())
					{
						Promise.f_SetException(DMibErrorInstance("Trust ticket server certificate does not include Host ID or Host ID is invalid"));
						return;
					}

					auto &Internal = *mp_pInternal;

					auto *pClientConnection = Internal.m_ClientConnections.f_FindEqual(_TrustTicket.m_ServerAddress);
					if (pClientConnection)
						Internal.f_RemoveClientConnection(pClientConnection);

					NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
					ConnectionSettings.m_ServerURL = _TrustTicket.m_ServerAddress.m_URL;
					ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
					ConnectionSettings.m_bRetryConnectOnFailure = false;
					ConnectionSettings.m_PublicServerCertificate = _TrustTicket.m_ServerPublicCert;

					Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, _Timeout)
						> [this, Promise, _Timeout, _TrustTicket, ServerHostID, _ConnectionConcurrency]
						(TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
						{
							if (!_ConnectionResult)
							{
								Promise.f_SetException(DMibErrorInstance(fg_Format("Failed to connect to server: {}", _ConnectionResult.f_GetExceptionStr())));
								return;
							}
							auto &Internal = *mp_pInternal;

							struct CConnectionState
							{
								bool m_bReplied = false;
								bool m_bDisableTimeout = false;
								CActorSubscription m_Subscription;
								CDistributedActorConnectionReference m_AnonymousConnection;
								TCDistributedActor<CInternal::CTicketInterface> m_TicketInterface;

								void f_Replied()
								{
									m_bReplied = true;
									m_Subscription.f_Clear();
									m_AnonymousConnection.f_Clear();
									m_TicketInterface.f_Clear();
								}
							};

							NStorage::TCSharedPointer<CConnectionState> pConnectionState = fg_Construct();
							pConnectionState->m_AnonymousConnection = fg_Move(_ConnectionResult->m_ConnectionReference);

							NStr::CStr UniqueHostID = _ConnectionResult->m_UniqueHostID;

							Internal.m_ActorDistributionManager
								(
									&CActorDistributionManager::f_SubscribeActors
									, "Anonymous/com.malterlib/Concurrency/TrustManagerTicket"
									, fg_ThisActor(this)
									, [this, Promise, pConnectionState, _TrustTicket, ServerHostID, UniqueHostID, _ConnectionConcurrency](CAbstractDistributedActor &&_NewActor)
									{
										if (_NewActor.f_GetUniqueHostID() != UniqueHostID)
											return;
										if (pConnectionState->m_bReplied)
											return;

										pConnectionState->m_bDisableTimeout = true;
										pConnectionState->m_TicketInterface = _NewActor.f_GetActor<CInternal::CTicketInterface>();

										if (!pConnectionState->m_TicketInterface)
										{
											using namespace NStr;
											CStr AvailableHashes;
											for (auto &Hash : _NewActor.f_GetTypeHashes())
												fg_AddStrSep(AvailableHashes, "{nfh}"_f << Hash, ", ");

											auto TypeNameHelper = fg_GetTypeNameConstExpr<CInternal::CTicketInterface>();
											CStr TypeName(TypeNameHelper.m_pString, TypeNameHelper.m_Len);

											pConnectionState->f_Replied();
											Promise.f_SetException
												(
													DMibErrorInstance
													(
													 	fg_Format
													 	(
															"Failed to get ticket interface: {} [{nfh}] from available hashes: {}"
															, TypeName
															, DMibConstantTypeHash(CInternal::CTicketInterface)
															, AvailableHashes
														)
													)
												)
											;
											return;
										}

										auto &Internal = *mp_pInternal;

										NCryptography::CCertificateOptions Options;
										Options.m_KeySetting = Internal.m_KeySetting;
										Options.m_CommonName = fg_Format("Malterlib Distributed Actors Client - {}", Internal.m_BasicConfig.m_HostID).f_Left(64);
										auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
										Extension.m_bCritical = false;
										Extension.m_Value = Internal.m_BasicConfig.m_HostID;

										NContainer::CByteVector CertificateRequest;
										try
										{
											NException::CDisableExceptionTraceScope DisableTrace;
											NCryptography::CCertificate::fs_GenerateClientCertificateRequest
												(
													Options
													, CertificateRequest
													, Internal.m_BasicConfig.m_CAPrivateKey
												)
											;
										}
										catch (NException::CException const &_Exception)
										{
											pConnectionState->f_Replied();
											Promise.f_SetException(DMibErrorInstance(fg_Format("Failed to generate certificate request for trust: {}", _Exception.f_GetErrorStr())));
											return;
										}

										pConnectionState->m_TicketInterface.f_CallActor(&CInternal::CTicketInterface::f_SignCertificate)(_TrustTicket.m_Token, CertificateRequest)
											> [this, Promise, pConnectionState, _TrustTicket, ServerHostID, _ConnectionConcurrency]
											(TCAsyncResult<NContainer::CByteVector> &&_PublicCertificate)
											{
												if (pConnectionState->m_bReplied)
													return;
												if (!_PublicCertificate)
												{
													pConnectionState->f_Replied();
													Promise.f_SetException
														(
															DMibErrorInstance(fg_Format("Failed to subscribe to remote ticket management: {}", _PublicCertificate.f_GetExceptionStr()))
														)
													;
													return;
												}

												NDistributedActorTrustManagerDatabase::CClientConnection ClientConnection;
												ClientConnection.m_PublicServerCertificate = _TrustTicket.m_ServerPublicCert;
												ClientConnection.m_PublicClientCertificate = *_PublicCertificate;
												ClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

												auto &Internal = *mp_pInternal;

												TCActorResultVector<void> DatabaseUpdates;

												for (auto &Connection : Internal.m_ClientConnections)
												{
													if (Connection.m_ClientConnection.m_PublicServerCertificate == _TrustTicket.m_ServerPublicCert)
													{
														Connection.m_ClientConnection.m_PublicClientCertificate = *_PublicCertificate;

														if (Internal.m_ClientConnectionsInDatabase.f_Exists(Connection.f_GetAddress()))
														{
															Internal.m_Database
																(
																	&ICDistributedActorTrustManagerDatabase::f_SetClientConnection
																	, Connection.f_GetAddress()
																	, Connection.m_ClientConnection
																)
																> DatabaseUpdates.f_AddResult()
															;
														}
														auto ConnectionSettings = Internal.f_GetConnectionSettings(Connection);
														for (auto &ConnectionReference : Connection.m_ConnectionReferences)
														{
															if (ConnectionReference.f_IsValid())
																ConnectionReference.f_UpdateConnectionSettings(ConnectionSettings) > DatabaseUpdates.f_AddResult();
														}
													}
												}

												bool bIsInDatabase = Internal.m_ClientConnectionsInDatabase.f_Exists(_TrustTicket.m_ServerAddress);
												Internal.m_ClientConnectionsInDatabase[_TrustTicket.m_ServerAddress];

												auto Address = _TrustTicket.m_ServerAddress;

												Internal.m_Database
													(
														bIsInDatabase
														? &ICDistributedActorTrustManagerDatabase::f_SetClientConnection
														: &ICDistributedActorTrustManagerDatabase::f_AddClientConnection
														, _TrustTicket.m_ServerAddress
														, ClientConnection
													)
													+ DatabaseUpdates.f_GetResults()
													>
													[
														this
														, pConnectionState
														, Promise
														, Address
														, ClientConnection
														, ServerHostID
													]
													(TCAsyncResult<void> &&_Result, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_OtherConnections)
													{
														if (pConnectionState->m_bReplied)
															return;
														if (!_Result)
														{
															pConnectionState->f_Replied();
															Promise.f_SetException
																(
																	DMibErrorInstance(fg_Format("Failed to save new client connection to database: {}", _Result.f_GetExceptionStr()))
																)
															;
															return;
														}

														if (!_OtherConnections)
														{
															DMibLogWithCategory
																(
																	Mib/Concurrency/Trust
																	, Error
																	, "Failed update client certificate for other client connections: {}"
																	, _OtherConnections.f_GetExceptionStr()
																)
															;
														}
														else
														{
															for (auto &OtherConnectionResult : *_OtherConnections)
															{
																if (!OtherConnectionResult)
																{
																	DMibLogWithCategory
																		(
																			Mib/Concurrency/Trust
																			, Error
																			, "Failed update client certificate for other client connection: {}"
																			, OtherConnectionResult.f_GetExceptionStr()
																		)
																	;
																}
															}
														}

														auto &Internal = *mp_pInternal;
														CActorDistributionConnectionSettings ConnectionSettings;
														ConnectionSettings.m_ServerURL = Address.m_URL;
														ConnectionSettings.m_PublicServerCertificate = ClientConnection.m_PublicServerCertificate;
														ConnectionSettings.m_PublicClientCertificate = ClientConnection.m_PublicClientCertificate;
														ConnectionSettings.m_PrivateClientKey = Internal.m_BasicConfig.m_CAPrivateKey;
														ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
														ConnectionSettings.m_bRetryConnectOnFailure = true;

														auto Address2 = Address;

														Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
															>
															[
																this
																, Promise
																, pConnectionState
																, Address2
																, ClientConnection
																, ServerHostID
															]
															(TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
															{
																if (pConnectionState->m_bReplied)
																	return;

																auto &Internal = *mp_pInternal;
																if (!_ConnectionResult)
																{
																	pConnectionState->f_Replied();
																	Internal.m_ClientConnectionsInDatabase.f_Remove(Address2);
																	Internal.m_Database
																		(
																			&ICDistributedActorTrustManagerDatabase::f_RemoveClientConnection
																			, Address2
																		)
																		> fg_DiscardResult();
																	;

																	Promise.f_SetException
																		(
																			DMibErrorInstance
																			(
																				fg_Format
																				(
																					"Failed to establish final connection to trusted server: {}"
																					, _ConnectionResult.f_GetExceptionStr()
																				)
																			)
																		)
																	;
																	return;
																}

																auto &ConnectionResult = *_ConnectionResult;
																DMibCheck(ConnectionResult.m_HostInfo.m_HostID == ServerHostID);

																auto &LocalClientConnection = Internal.m_ClientConnections[Address2];

																LocalClientConnection.m_ClientConnection = ClientConnection;
																auto &Host = Internal.m_Hosts[ServerHostID];
																Host.m_ClientConnections.f_Insert(LocalClientConnection);
																LocalClientConnection.m_pHost = &Host;
																LocalClientConnection.m_ConnectionReferences.f_Insert(fg_Move(ConnectionResult.m_ConnectionReference));
																Host.m_FriendlyName = ConnectionResult.m_HostInfo.m_FriendlyName;
																pConnectionState->f_Replied();

																Promise.f_SetResult(ConnectionResult.m_HostInfo);

																Internal.f_ApplyConnectionConcurrency(LocalClientConnection);
															}
														;
													}
												;
											}
										;
									}
									, [](CDistributedActorIdentifier const &_RemovedActor)
									{
									}
								)
								> [pConnectionState, Promise](auto &&_Subscription)
								{
									if (pConnectionState->m_bReplied)
										return;
									if (!_Subscription)
									{
										pConnectionState->f_Replied();
										Promise.f_SetException(DMibErrorInstance(fg_Format("Failed to subscribe to remote ticket management: {}", _Subscription.f_GetExceptionStr())));
									}
									pConnectionState->m_Subscription = fg_Move(*_Subscription);
								}
							;

							fg_TimerActor()
								(
									&CTimerActor::f_OneshotTimer
									, _Timeout
									, fg_ThisActor(this)
									, [pConnectionState, Promise]
									{
										if (pConnectionState->m_bReplied || pConnectionState->m_bDisableTimeout)
											return;
										pConnectionState->f_Replied();

										Promise.f_SetException(DMibErrorInstance("Timed out waiting for remote trust manager"));
									}
									, true
								)
								> fg_DiscardResult()
							;
						}
					;
				}
			)
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency)
	{
		TCPromise<void> Promise;
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _Address, _ConnectionConcurrency]
				{
					auto &Internal = *mp_pInternal;

					auto *pClientConnectionState = Internal.m_ClientConnections.f_FindEqual(_Address);
					if (!pClientConnectionState)
					{
						Promise.f_SetException(DMibErrorInstance("No such client connection"));
						return;
					}

					if (_ConnectionConcurrency == pClientConnectionState->m_ClientConnection.m_ConnectionConcurrency)
					{
						Promise.f_SetResult();
						return;
					}

					auto NewClientConnection = pClientConnectionState->m_ClientConnection;
					NewClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

					Internal.m_Database
						(
							&ICDistributedActorTrustManagerDatabase::f_SetClientConnection
							, _Address
							, NewClientConnection
						)
						> Promise % "Failed to save client connection to database"
						/
						[
							this
							, Promise
							, _Address
							, _ConnectionConcurrency
						]
						() mutable
						{
							auto &Internal = *mp_pInternal;

							auto &LocalClientConnection = Internal.m_ClientConnections[_Address];
							LocalClientConnection.m_ClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;
							Internal.f_ApplyConnectionConcurrency(LocalClientConnection);
							Promise.f_SetResult();
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<CHostInfo> CDistributedActorTrustManager::f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address, int32 _ConnectionConcurrency)
	{
		TCPromise<CHostInfo> Promise;
		// Connect with insecure connection to server to get server certificate
		// Connect with server certificate to verify that trust is correct
		// Connect with server certificate and client certificate at the end
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _Address, _ConnectionConcurrency]
				{
					auto &Internal = *mp_pInternal;

					auto *pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
					if (pClientConnection)
					{
						Promise.f_SetException(DMibErrorInstance("Address already exists"));
						return;
					}

					NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
					ConnectionSettings.m_ServerURL = _Address.m_URL;
					ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
					ConnectionSettings.m_bRetryConnectOnFailure = false;
					ConnectionSettings.m_bAllowInsecureConnection = true;

					Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
						> Promise % "Failed to connect to server"
						/ [this, Promise, _Address, _ConnectionConcurrency](CActorDistributionManager::CConnectionResult &&_ConnectionResult)
						{
							auto &Internal = *mp_pInternal;

							auto &ConnectionResult = _ConnectionResult;
							if (ConnectionResult.m_CertificateChain.f_IsEmpty())
							{
								Promise.f_SetException(DMibErrorInstance("Missing certificate chain in connection result"));
								return;
							}

							DMibFastCheck(!ConnectionResult.m_HostInfo.m_HostID.f_IsEmpty());
							DMibFastCheck(!ConnectionResult.m_UniqueHostID.f_IsEmpty());

							NStr::CStr ServerHostID = ConnectionResult.m_HostInfo.m_HostID;

							auto &RootCertificate = ConnectionResult.m_CertificateChain.f_GetLast();

							CClientConnection NewClientConnection;
							bool bFoundCertificate = false;

							for (auto &Connection : Internal.m_ClientConnections)
							{
								if (Connection.m_ClientConnection.m_PublicServerCertificate == RootCertificate)
								{
									NewClientConnection = Connection.m_ClientConnection;
									bFoundCertificate = true;
									break;
								}
							}

							if (!bFoundCertificate)
							{
								Promise.f_SetException(DMibErrorInstance("No existing connection found that matches the remote certificate"));
								return;
							}

							NewClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

							NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
							ConnectionSettings.m_ServerURL = _Address.m_URL;
							ConnectionSettings.m_PublicServerCertificate = NewClientConnection.m_PublicServerCertificate;
							ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
							ConnectionSettings.m_bRetryConnectOnFailure = false;

							Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
								> Promise % "Failed to verify connection to server"
								/ [this, Promise, _Address, NewClientConnection, ServerHostID](CActorDistributionManager::CConnectionResult &&_ConnectionResult)
								{
									auto &Internal = *mp_pInternal;

									NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
									ConnectionSettings.m_ServerURL = _Address.m_URL;
									ConnectionSettings.m_PublicServerCertificate = NewClientConnection.m_PublicServerCertificate;
									ConnectionSettings.m_PublicClientCertificate = NewClientConnection.m_PublicClientCertificate;
									ConnectionSettings.m_PrivateClientKey = Internal.m_BasicConfig.m_CAPrivateKey;
									ConnectionSettings.m_bRetryConnectOnFirstFailure = true;
									ConnectionSettings.m_bRetryConnectOnFailure = true;

									Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
										> Promise % "Failed to connect to server"
										/
										[
											this
											, Promise
											, _Address
											, NewClientConnection
											, ServerHostID
										]
										(CActorDistributionManager::CConnectionResult &&_ConnectionResult)
										{
											auto &Internal = *mp_pInternal;

											bool bIsInDatabase = Internal.m_ClientConnectionsInDatabase.f_Exists(_Address);
											Internal.m_ClientConnectionsInDatabase[_Address];

											Internal.m_Database
												(
													bIsInDatabase
													? &ICDistributedActorTrustManagerDatabase::f_SetClientConnection
													: &ICDistributedActorTrustManagerDatabase::f_AddClientConnection
													, _Address
													, NewClientConnection
												)
												> Promise % "Failed to save new client connection to database"
												/
												[
													this
													, Promise
													, _Address
													, NewClientConnection
													, ServerHostID
													, ConnectionReference = fg_Move(_ConnectionResult.m_ConnectionReference)
													, HostInfo = _ConnectionResult.m_HostInfo
												]
												() mutable
												{
													auto &Internal = *mp_pInternal;

													auto &LocalClientConnection = Internal.m_ClientConnections[_Address];

													LocalClientConnection.m_ClientConnection = NewClientConnection;
													auto &Host = Internal.m_Hosts[ServerHostID];
													Host.m_ClientConnections.f_Insert(LocalClientConnection);
													LocalClientConnection.m_pHost = &Host;
													LocalClientConnection.m_ConnectionReferences.f_Insert(fg_Move(ConnectionReference));
													Host.m_FriendlyName = HostInfo.m_FriendlyName;
													Promise.f_SetResult(HostInfo);

													Internal.f_ApplyConnectionConcurrency(LocalClientConnection);
												}
											;
										}
									;
								}
							;
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_BreakClientConnection(CDistributedActorTrustManager_Address const &_Address, fp64 _Timeout)
	{
		using namespace NMib::NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			co_return DMibErrorInstance("Could not find client connection for '{}'"_f << _Address);

		TCActorResultVector<void> BreakResults;
		for (auto &ConnectionReference : pClientConnection->m_ConnectionReferences)
			ConnectionReference.f_Debug_Break(_Timeout) > BreakResults.f_AddResult();

		co_await (BreakResults.f_GetResults() % "Failed to break client connections") | g_Unwrap;

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_BreakListenConnections(CDistributedActorTrustManager_Address const &_Address, fp64 _Timeout)
	{
		using namespace NMib::NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CListenConfig Config{_Address};

		auto pListen = Internal.m_Listen.f_FindEqual(Config);
		if (!pListen)
			co_return DMibErrorInstance("Could not find listen for '{}'"_f << _Address);

		co_await (pListen->m_ListenReference.f_Debug_BreakAllConnections(_Timeout) % "Failed to break listen connections");

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_SetListenServerBroken(CDistributedActorTrustManager_Address const &_Address, bool _bBroken)
	{
		using namespace NMib::NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CListenConfig Config{_Address};

		auto pListen = Internal.m_Listen.f_FindEqual(Config);
		if (!pListen)
			co_return DMibErrorInstance("Could not find listen for '{}'"_f << _Address);

		co_await (pListen->m_ListenReference.f_Debug_SetServerBroken(_bBroken) % "Failed to set listen server broken");

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address, bool _bPreserveHost)
	{
		TCPromise<void> Promise;
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _Address, _bPreserveHost]
				{
					auto &Internal = *mp_pInternal;
					Internal.m_ClientConnectionsInDatabase.f_Remove(_Address);
					Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveClientConnection, _Address)
						> Promise % "Failed to remove client connection from database" / [this, Promise, _Address, _bPreserveHost]
						{
							auto &Internal = *mp_pInternal;
							auto pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
							if (!pClientConnection)
								return Promise.f_SetResult();

							pClientConnection->m_bRemoving = true;

							TCActorResultVector<void> DisconnectResults;
							for (auto &ConnectionReference : pClientConnection->m_ConnectionReferences)
								ConnectionReference.f_Disconnect(_bPreserveHost) > DisconnectResults.f_AddResult();

							DisconnectResults.f_GetResults() > Promise % "Failed to disconnect client connection"
								/ [this, _Address, Promise](NContainer::TCVector<TCAsyncResult<void>> &&_Results)
								{
									for (auto &Result : _Results)
									{
										if (!Result)
										{
											DMibLogWithCategory
												(
													Mib/Concurrency/Trust
													, Error
													, "Failed disconnect connection when removing client connection: {}"
													, Result.f_GetExceptionStr()
												)
											;
										}
									}
									auto &Internal = *mp_pInternal;
									auto pConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
									if (pConnection)
										Internal.f_RemoveClientConnection(pConnection);
									Promise.f_SetResult();
								}
							;
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	auto CDistributedActorTrustManager::f_EnumClientConnections() -> TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>>
	{
		TCPromise<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>> Promise;
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise]
				{
					auto &Internal = *mp_pInternal;
					NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo> Addresses;
					bool bMissingHost = false;
					for (auto iClientConnection = Internal.m_ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
					{
						auto &ClientConnection = *iClientConnection;
						auto &Address = Addresses[iClientConnection.f_GetKey()];
						if (ClientConnection.m_pHost)
						{
							Address.m_HostInfo.m_HostID = ClientConnection.m_pHost->f_GetHostID();
							Address.m_HostInfo.m_FriendlyName = ClientConnection.m_pHost->m_FriendlyName;
							Address.m_ConnectionConcurrency = ClientConnection.m_ClientConnection.m_ConnectionConcurrency;
						}
						else
							bMissingHost = true;
					}

					if (!bMissingHost)
					{
						Promise.f_SetResult(fg_Move(Addresses));
						return;
					}

					Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClientConnections, true)
						> (Promise % "Failed to enum client connections in database")
						/ [Promise, Addresses = fg_Move(Addresses)](NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> &&_ClientConnections) mutable
						{
							for (auto iClientConnection = _ClientConnections.f_GetIterator(); iClientConnection; ++iClientConnection)
							{
								auto &Client = Addresses[iClientConnection.f_GetKey()];
								if (Client.m_HostInfo.m_HostID.f_IsEmpty())
								{
									try
									{
										Client.m_HostInfo.m_HostID = CActorDistributionManager::fs_GetCertificateHostID(iClientConnection->m_PublicServerCertificate);
									}
									catch (NException::CException const &)
									{
									}
									Client.m_HostInfo.m_FriendlyName = iClientConnection->m_LastFriendlyName;
									Client.m_ConnectionConcurrency = iClientConnection->m_ConnectionConcurrency;
								}
							}
							Promise.f_SetResult(fg_Move(Addresses));
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<bool> CDistributedActorTrustManager::f_HasClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		TCPromise<bool> Promise;
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _Address]
				{
					auto &Internal = *mp_pInternal;
					Promise.f_SetResult(Internal.m_ClientConnections.f_FindEqual(_Address) != nullptr);
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<CDistributedActorTrustManager::CConnectionState> CDistributedActorTrustManager::f_GetConnectionState()
	{
		TCPromise<CDistributedActorTrustManager::CConnectionState> Promise;
		auto &Internal = *mp_pInternal;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise]
				{
					auto &Internal = *mp_pInternal;
					TCActorResultMap<CDistributedActorTrustManager_Address, NContainer::TCVector<TCAsyncResult<CDistributedActorConnectionStatus>>> ConnectionResults;
					NContainer::TCMap<CDistributedActorTrustManager_Address, NStr::CStr> FriendlyNames;
					for (auto iConnection = Internal.m_ClientConnections.f_GetIterator(); iConnection; ++iConnection)
					{
						TCActorResultVector<CDistributedActorConnectionStatus> StatusResults;
						for (auto &ConnectionReference : iConnection->m_ConnectionReferences)
							ConnectionReference.f_GetStatus() > StatusResults.f_AddResult();

						StatusResults.f_GetResults() > ConnectionResults.f_AddResult(iConnection->f_GetAddress());

						FriendlyNames[iConnection->f_GetAddress()] = iConnection->m_ClientConnection.m_LastFriendlyName;
					}

					ConnectionResults.f_GetResults()
						> Promise / [Promise, FriendlyNames = fg_Move(FriendlyNames)]
						(NContainer::TCMap<CDistributedActorTrustManager_Address, TCAsyncResult<NContainer::TCVector<TCAsyncResult<CDistributedActorConnectionStatus>>>> &&_Results)
						{
							CDistributedActorTrustManager::CConnectionState ConnectionState;
							for (auto iStatuses = _Results.f_GetIterator(); iStatuses; ++iStatuses)
							{
								auto &Address = iStatuses.f_GetKey();
								auto &StatusResults = *iStatuses;

								if (!StatusResults)
								{
									continue;
								}
								for (auto &StatusResult : *StatusResults)
								{
									if (!StatusResult)
									{
										auto &OutAddressState = ConnectionState.m_Hosts["Unknown"].m_Addresses[Address];
										auto &OutStatus = OutAddressState.m_States.f_Insert();
										OutStatus.m_bConnected = false;
										OutStatus.m_Error = StatusResult.f_GetExceptionStr();
										OutStatus.m_ErrorTime = NTime::CTime::fs_NowUTC();

										if (auto *pFriendly = FriendlyNames.f_FindEqual(Address))
										{
											OutAddressState.m_HostInfo.m_HostID = "Unknown";
											OutAddressState.m_HostInfo.m_FriendlyName = *pFriendly;
										}
										continue;
									}
									auto &Status = *StatusResult;
									auto &OutAddressState = ConnectionState.m_Hosts[Status.m_HostInfo.m_HostID].m_Addresses[Address];
									OutAddressState.m_HostInfo = Status.m_HostInfo;
									if (OutAddressState.m_HostInfo.m_FriendlyName.f_IsEmpty())
									{
										if (auto *pFriendly = FriendlyNames.f_FindEqual(Address))
											OutAddressState.m_HostInfo.m_FriendlyName = *pFriendly;
									}
									auto &OutStatus = OutAddressState.m_States.f_Insert();
									OutStatus.m_bConnected = Status.m_bConnected;
									OutStatus.m_Error = Status.m_Error;
									OutStatus.m_ErrorTime = Status.m_ErrorTime;
								}
							}
							Promise.f_SetResult(ConnectionState);
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<CDistributedActorTrustManagerInterface::CConnectionsDebugStats> CDistributedActorTrustManager::f_GetConnectionsDebugStats()
	{
		auto &Internal = *mp_pInternal;

		co_await Internal.f_WaitForInit();

		CDistributedActorTrustManagerInterface::CConnectionsDebugStats DebugStats;

		DebugStats.m_DebugStats = co_await Internal.m_ActorDistributionManager(&CActorDistributionManager::f_GetConnectionsDebugStats);

		co_return fg_Move(DebugStats);
	}
}
