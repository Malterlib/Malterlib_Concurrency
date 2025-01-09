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
			auto CaptureScope = co_await (g_CaptureExceptions % "Fraudulent client sign attempt");
			NCryptography::CCertificate::fs_VerifyCertificateRequestSameKeyAsCertificate(_CertificateRequest, pExistingClient->m_PublicCertificate);
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
			NStr::CStr _Token
			, NContainer::CByteVector _CertificateRequest
		)
	{
		co_return co_await fg_Dispatch
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
			NStr::CStr _HostID
			, NContainer::TCVector<NContainer::CByteVector> _CertificateChain
		)
	{
		return m_pThis->f_ValidateClientAccess(_HostID, _CertificateChain);
	}

	TCFuture<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> CDistributedActorTrustManager::f_GenerateConnectionTicket
		(
			CDistributedActorTrustManager_Address _Address
			, TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest)> _fOnUseTicket
			, TCActorFunctor<TCFuture<void> (NStr::CStr _HostID, CCallingHostInfo _HostInfo)> _fOnCertificateSigned
		)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CListenConfig ListenConfig;
		ListenConfig.m_Address = _Address;
		auto *pListen = Internal.m_Listen.f_FindEqual(ListenConfig);
		if (!pListen)
			co_return DMibErrorInstance("Could not find listen with this address");

		auto *pServerCertificate = Internal.m_ServerCertificates.f_FindEqual(_Address.m_URL.f_GetHost());
		if (!pServerCertificate)
			co_return DMibErrorInstance("Could not find and server certificate for this address");

		CDistributedActorTrustManager::CTrustTicket TrustTicket;

		TrustTicket.m_Token = NCryptography::fg_RandomID();
		TrustTicket.m_ServerAddress = _Address;
		TrustTicket.m_ServerPublicCert = Internal.m_BasicConfig.m_CACertificate;

		auto &TicketState = Internal.m_Tickets[TrustTicket.m_Token];
		TicketState.m_CreationTime = Internal.m_TicketTimer.f_Elapsed();
		TicketState.m_fOnUseTicket = fg_Move(_fOnUseTicket);
		TicketState.m_fOnCertificateSigned = fg_Move(_fOnCertificateSigned);

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

					TCFutureVector<void> Destroys;
					fg_Move(pTicket->m_fOnUseTicket).f_Destroy() > Destroys;
					fg_Move(pTicket->m_fOnCertificateSigned).f_Destroy() > Destroys;

					Internal.m_Tickets.f_Remove(Token);

					co_await fg_AllDoneWrapped(Destroys);

					co_return {};
				}
			;
		}

		co_return fg_Move(Result);
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
			_ConnectionState.m_ConnectionReferences[0].f_Reconnect().f_DiscardResult();
			return;
		}

		auto Address = _ConnectionState.f_GetAddress();
		auto ConnectionSettings = f_GetConnectionSettings(_ConnectionState);

		for (int32 ToCreate = ConnectionConcurrency - _ConnectionState.m_ConnectionReferences.f_GetLen(); ToCreate > 0; --ToCreate)
		{
			if (!m_ActorDistributionManager)
				continue;

			m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0)
				> [=, this](TCAsyncResult<CActorDistributionManager::CConnectionResult> &&_ConnectionResult)
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
						return; // Settings changed asynchronously

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

	TCFuture<CHostInfo> CDistributedActorTrustManager::f_AddClientConnection(CTrustTicket _TrustTicket, fp64 _Timeout, int32 _ConnectionConcurrency)
	{
		// Connect to remote host with anonymous connection
		// Subscribe to internal interface (time out if no publication arrives)
		// Generate certificate request and send to remote host
		// Save client connection to database
		// Connect again to remote host with signed client certificate (if failure, remove from database again)

		auto CheckDestroy = co_await f_CheckDestroyedOnResume();

		using namespace NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		NStr::CStr ServerHostID;
		{
			auto CaptureScope = co_await (g_CaptureExceptions % "Error getting server host ID from certificate");
			ServerHostID = CActorDistributionManager::fs_GetCertificateHostID(_TrustTicket.m_ServerPublicCert);
		}

		if (ServerHostID.f_IsEmpty())
			co_return DMibErrorInstance("Trust ticket server certificate does not include Host ID or Host ID is invalid");

		auto *pClientConnection = Internal.m_ClientConnections.f_FindEqual(_TrustTicket.m_ServerAddress);
		if (pClientConnection)
			Internal.f_RemoveClientConnection(pClientConnection);

		NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = _TrustTicket.m_ServerAddress.m_URL;
		ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		ConnectionSettings.m_bRetryConnectOnFailure = false;
		ConnectionSettings.m_PublicServerCertificate = _TrustTicket.m_ServerPublicCert;

		auto ConnectionResult = co_await (Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, _Timeout) % "Failed to connect to server");

		NStr::CStr UniqueHostID = ConnectionResult.m_UniqueHostID;

		TCPromiseFuturePair<CAbstractDistributedActor> NewInterfacePromise;

		auto Subscription = co_await
			(
				Internal.m_ActorDistributionManager
				(
					&CActorDistributionManager::f_SubscribeActors
					, "Anonymous/com.malterlib/Concurrency/TrustManagerTicket"
					, fg_ThisActor(this)
					, [NewInterfacePromise = fg_Move(NewInterfacePromise.m_Promise), UniqueHostID](CAbstractDistributedActor _NewActor) -> TCFuture<void>
					{
						if (_NewActor.f_GetUniqueHostID() != UniqueHostID)
							co_return {};

						NewInterfacePromise.f_SetResult(fg_Move(_NewActor));

						co_return {};
					}
					, [](CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
					{

						co_return {};
					}
				)
				.f_Timeout(_Timeout, "Timed out waiting for remote trust manager")
				% "Failed to subscribe to remote ticket management"
			)

		;

		auto NewActor = co_await fg_Move(NewInterfacePromise.m_Future);

		auto TicketInterface = NewActor.f_GetActor<CInternal::CTicketInterface>();

		if (!TicketInterface)
		{
			using namespace NStr;
			CStr AvailableHashes;
			for (auto &Hash : NewActor.f_GetTypeHashes())
				fg_AddStrSep(AvailableHashes, "{nfh}"_f << Hash, ", ");

			auto TypeNameHelper = fg_GetTypeNameConstExpr<CInternal::CTicketInterface>();
			CStr TypeName(TypeNameHelper.m_pString, TypeNameHelper.m_Len);

			co_return DMibErrorInstance
				(
					"Failed to get ticket interface: {} [{nfh}] from available hashes: {}"_f
					<< TypeName
					<< DMibConstantTypeHash(CInternal::CTicketInterface)
					<< AvailableHashes
				)
			;
		}

		NCryptography::CCertificateOptions Options;
		Options.m_KeySetting = Internal.m_KeySetting;
		Options.m_CommonName = fg_Format("Malterlib Distributed Actors Client - {}", Internal.m_BasicConfig.m_HostID).f_Left(64);
		auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
		Extension.m_bCritical = false;
		Extension.m_Value = Internal.m_BasicConfig.m_HostID;

		NContainer::CByteVector CertificateRequest;
		{
			auto CaptureScope = co_await (g_CaptureExceptions % "Failed to generate certificate request for trust");

			NCryptography::CCertificate::fs_GenerateClientCertificateRequest
				(
					Options
					, CertificateRequest
					, Internal.m_BasicConfig.m_CAPrivateKey
				)
			;
		}

		auto PublicCertificate = co_await
			(
				TicketInterface.f_CallActor(&CInternal::CTicketInterface::f_SignCertificate)(_TrustTicket.m_Token, CertificateRequest) % "Failed to subscribe to remote ticket management"
			)
		;

		NDistributedActorTrustManagerDatabase::CClientConnection ClientConnection;
		ClientConnection.m_PublicServerCertificate = _TrustTicket.m_ServerPublicCert;
		ClientConnection.m_PublicClientCertificate = PublicCertificate;
		ClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

		TCFutureVector<void> DatabaseUpdates;

		for (auto &Connection : Internal.m_ClientConnections)
		{
			if (Connection.m_ClientConnection.m_PublicServerCertificate == _TrustTicket.m_ServerPublicCert)
			{
				Connection.m_ClientConnection.m_PublicClientCertificate = PublicCertificate;

				if (Internal.m_ClientConnectionsInDatabase.f_Exists(Connection.f_GetAddress()))
				{
					Internal.m_Database
						(
							&ICDistributedActorTrustManagerDatabase::f_SetClientConnection
							, Connection.f_GetAddress()
							, Connection.m_ClientConnection
						)
						> DatabaseUpdates
					;
				}

				auto ConnectionSettings = Internal.f_GetConnectionSettings(Connection);
				for (auto &ConnectionReference : Connection.m_ConnectionReferences)
				{
					if (ConnectionReference.f_IsValid())
						ConnectionReference.f_UpdateConnectionSettings(ConnectionSettings) > DatabaseUpdates;
				}
			}
		}

		bool bIsInDatabase = Internal.m_ClientConnectionsInDatabase.f_Exists(_TrustTicket.m_ServerAddress);
		Internal.m_ClientConnectionsInDatabase[_TrustTicket.m_ServerAddress];

		auto Address = _TrustTicket.m_ServerAddress;

		auto [SaveDummy, OtherConnections] = co_await
			(
				(
					Internal.m_Database
					(
						bIsInDatabase
						? &ICDistributedActorTrustManagerDatabase::f_SetClientConnection
						: &ICDistributedActorTrustManagerDatabase::f_AddClientConnection
						, _TrustTicket.m_ServerAddress
						, ClientConnection
					)
					% "Failed to save new client connection to database"
				)
				+ (fg_AllDoneWrapped(DatabaseUpdates) % "Failed update client certificate for other client connections")
			)
		;

		co_await (fg_Move(OtherConnections) | (g_Unwrap % "Failed update client certificate for other client connection"));

		CActorDistributionConnectionSettings FinalConnectionSettings;
		FinalConnectionSettings.m_ServerURL = Address.m_URL;
		FinalConnectionSettings.m_PublicServerCertificate = ClientConnection.m_PublicServerCertificate;
		FinalConnectionSettings.m_PublicClientCertificate = ClientConnection.m_PublicClientCertificate;
		FinalConnectionSettings.m_PrivateClientKey = Internal.m_BasicConfig.m_CAPrivateKey;
		FinalConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		FinalConnectionSettings.m_bRetryConnectOnFailure = true;

		auto FinalConnectionResult = co_await Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, FinalConnectionSettings, 3600.0).f_Wrap();
		if (!FinalConnectionResult)
		{
			Internal.m_ClientConnectionsInDatabase.f_Remove(Address);
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveClientConnection, Address);

			co_return DMibErrorInstance("Failed to establish final connection to trusted server: {}"_f << FinalConnectionResult.f_GetExceptionStr());
		}

		auto &FinalConnection = *FinalConnectionResult;
		DMibCheck(FinalConnection.m_HostInfo.m_HostID == ServerHostID);

		auto &LocalClientConnection = Internal.m_ClientConnections[Address];

		LocalClientConnection.m_ClientConnection = ClientConnection;
		auto &Host = Internal.m_Hosts[ServerHostID];
		Host.m_ClientConnections.f_Insert(LocalClientConnection);
		LocalClientConnection.m_pHost = &Host;
		LocalClientConnection.m_ConnectionReferences.f_Insert(fg_Move(FinalConnection.m_ConnectionReference));
		Host.m_FriendlyName = FinalConnection.m_HostInfo.m_FriendlyName;

		Internal.f_ApplyConnectionConcurrency(LocalClientConnection);

		co_return fg_Move(FinalConnection.m_HostInfo);
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetClientConnectionConcurrency(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pClientConnectionState = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnectionState)
			co_return DMibErrorInstance("No such client connection");

		if (_ConnectionConcurrency == pClientConnectionState->m_ClientConnection.m_ConnectionConcurrency)
			co_return {};

		auto NewClientConnection = pClientConnectionState->m_ClientConnection;
		NewClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

		co_await
			(
				Internal.m_Database
				(
					&ICDistributedActorTrustManagerDatabase::f_SetClientConnection
					, _Address
					, NewClientConnection
				)
				% "Failed to save client connection to database"
			)
		;

		auto &LocalClientConnection = Internal.m_ClientConnections[_Address];
		LocalClientConnection.m_ClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;
		Internal.f_ApplyConnectionConcurrency(LocalClientConnection);

		co_return {};
	}

	TCFuture<CHostInfo> CDistributedActorTrustManager::f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address _Address, int32 _ConnectionConcurrency)
	{
		// Connect with insecure connection to server to get server certificate
		// Connect with server certificate to verify that trust is correct
		// Connect with server certificate and client certificate at the end

		using namespace NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (pClientConnection)
			co_return DMibErrorInstance("Address already exists");

		CClientConnection NewClientConnection;
		NStr::CStr ServerHostID;
		// Anonymous connection
		{
			NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = _Address.m_URL;
			ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
			ConnectionSettings.m_bRetryConnectOnFailure = false;
			ConnectionSettings.m_bAllowInsecureConnection = true;

			auto ConnectionResult = co_await (Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0) % "Failed to connect to server");

			if (ConnectionResult.m_CertificateChain.f_IsEmpty())
				co_return DMibErrorInstance("Missing certificate chain in connection result");

			DMibFastCheck(!ConnectionResult.m_HostInfo.m_HostID.f_IsEmpty());
			DMibFastCheck(!ConnectionResult.m_UniqueHostID.f_IsEmpty());

			ServerHostID = ConnectionResult.m_HostInfo.m_HostID;

			auto &RootCertificate = ConnectionResult.m_CertificateChain.f_GetLast();

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
				co_return DMibErrorInstance("No existing connection found that matches the remote certificate");

			co_await ConnectionResult.m_ConnectionReference.f_Disconnect();
		}

		NewClientConnection.m_ConnectionConcurrency = _ConnectionConcurrency;

		// Verify connection
		{
			NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = _Address.m_URL;
			ConnectionSettings.m_PublicServerCertificate = NewClientConnection.m_PublicServerCertificate;
			ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			auto ConnectionResult = co_await
				(
					Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0) % "Failed to verify connection to server"
				)
			;

			co_await ConnectionResult.m_ConnectionReference.f_Disconnect();
		}

		// Final connection
		{
			NMib::NConcurrency::CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = _Address.m_URL;
			ConnectionSettings.m_PublicServerCertificate = NewClientConnection.m_PublicServerCertificate;
			ConnectionSettings.m_PublicClientCertificate = NewClientConnection.m_PublicClientCertificate;
			ConnectionSettings.m_PrivateClientKey = Internal.m_BasicConfig.m_CAPrivateKey;
			ConnectionSettings.m_bRetryConnectOnFirstFailure = true;
			ConnectionSettings.m_bRetryConnectOnFailure = true;

			auto ConnectionResult = co_await (Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 3600.0) % "Failed to connect to server");

			bool bIsInDatabase = Internal.m_ClientConnectionsInDatabase.f_Exists(_Address);
			Internal.m_ClientConnectionsInDatabase[_Address];

			co_await
				(
					Internal.m_Database
					(
						bIsInDatabase
						? &ICDistributedActorTrustManagerDatabase::f_SetClientConnection
						: &ICDistributedActorTrustManagerDatabase::f_AddClientConnection
						, _Address
						, NewClientConnection
					)
					% "Failed to save new client connection to database"
				)
			;

			auto &LocalClientConnection = Internal.m_ClientConnections[_Address];

			LocalClientConnection.m_ClientConnection = NewClientConnection;
			auto &Host = Internal.m_Hosts[ServerHostID];
			Host.m_ClientConnections.f_Insert(LocalClientConnection);
			LocalClientConnection.m_pHost = &Host;
			LocalClientConnection.m_ConnectionReferences.f_Insert(fg_Move(ConnectionResult.m_ConnectionReference));
			Host.m_FriendlyName = ConnectionResult.m_HostInfo.m_FriendlyName;

			Internal.f_ApplyConnectionConcurrency(LocalClientConnection);

			co_return fg_Move(ConnectionResult.m_HostInfo);
		}
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_BreakClientConnection(CDistributedActorTrustManager_Address _Address, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags)
	{
		using namespace NMib::NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			co_return DMibErrorInstance("Could not find client connection for '{}'"_f << _Address);

		TCFutureVector<void> BreakResults;
		for (auto &ConnectionReference : pClientConnection->m_ConnectionReferences)
			ConnectionReference.f_Debug_Break(_Timeout, _DebugFlags) > BreakResults;

		co_await (fg_AllDone(BreakResults) % "Failed to break client connections");

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_BreakListenConnections
		(
			CDistributedActorTrustManager_Address _Address
			, fp64 _Timeout
			, NNetwork::ESocketDebugFlag _DebugFlags
		)
	{
		using namespace NMib::NStr;

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CListenConfig Config{_Address};

		auto pListen = Internal.m_Listen.f_FindEqual(Config);
		if (!pListen)
			co_return DMibErrorInstance("Could not find listen for '{}'"_f << _Address);

		co_await (pListen->m_ListenReference.f_Debug_BreakAllConnections(_Timeout, _DebugFlags) % "Failed to break listen connections");

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_Debug_SetListenServerBroken(CDistributedActorTrustManager_Address _Address, bool _bBroken)
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

	TCFuture<void> CDistributedActorTrustManager::f_RemoveClientConnection(CDistributedActorTrustManager_Address _Address, bool _bPreserveHost)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		Internal.m_ClientConnectionsInDatabase.f_Remove(_Address);
		co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveClientConnection, _Address) % "Failed to remove client connection from database");

		auto pClientConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (!pClientConnection)
			co_return {};

		pClientConnection->m_bRemoving = true;

		TCFutureVector<void> DisconnectResults;
		for (auto &ConnectionReference : pClientConnection->m_ConnectionReferences)
			ConnectionReference.f_Disconnect(_bPreserveHost) > DisconnectResults;

		(co_await fg_AllDone(DisconnectResults).f_Wrap()) > fg_LogError("Mib/Concurrency/Trust", "Failed disconnect connections when removing client connection");

		auto pConnection = Internal.m_ClientConnections.f_FindEqual(_Address);
		if (pConnection)
			Internal.f_RemoveClientConnection(pConnection);

		co_return {};
	}

	auto CDistributedActorTrustManager::f_EnumClientConnections() -> TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnectionInfo>>
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

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
			co_return fg_Move(Addresses);

		auto ClientConnections = co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClientConnections, true) % "Failed to enum client connections in database");

		for (auto &ClientConnectionEntry : ClientConnections.f_Entries())
		{
			auto &ClientConnection = ClientConnectionEntry.f_Value();

			auto &Client = Addresses[ClientConnectionEntry.f_Key()];

			if (Client.m_HostInfo.m_HostID.f_IsEmpty())
			{
				try
				{
					Client.m_HostInfo.m_HostID = CActorDistributionManager::fs_GetCertificateHostID(ClientConnection.m_PublicServerCertificate);
				}
				catch (NException::CException const &)
				{
				}
				Client.m_HostInfo.m_FriendlyName = ClientConnection.m_LastFriendlyName;
				Client.m_ConnectionConcurrency = ClientConnection.m_ConnectionConcurrency;
			}
		}

		co_return fg_Move(Addresses);
	}

	TCFuture<bool> CDistributedActorTrustManager::f_HasClientConnection(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_return Internal.m_ClientConnections.f_FindEqual(_Address) != nullptr;
	}

	TCFuture<CDistributedActorTrustManager::CConnectionState> CDistributedActorTrustManager::f_GetConnectionState()
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		TCFutureMap<CDistributedActorTrustManager_Address, NContainer::TCVector<TCAsyncResult<CDistributedActorConnectionStatus>>> ConnectionResults;
		NContainer::TCMap<CDistributedActorTrustManager_Address, NStr::CStr> FriendlyNames;
		for (auto iConnection = Internal.m_ClientConnections.f_GetIterator(); iConnection; ++iConnection)
		{
			TCFutureVector<CDistributedActorConnectionStatus> StatusResults;
			for (auto &ConnectionReference : iConnection->m_ConnectionReferences)
				ConnectionReference.f_GetStatus() > StatusResults;

			fg_AllDoneWrapped(StatusResults) > ConnectionResults[iConnection->f_GetAddress()];

			FriendlyNames[iConnection->f_GetAddress()] = iConnection->m_ClientConnection.m_LastFriendlyName;
		}

		auto Results = co_await fg_AllDoneWrapped(ConnectionResults);
		CDistributedActorTrustManager::CConnectionState ConnectionState;
		for (auto &StatusesEntry : Results.f_Entries())
		{
			auto &Address = StatusesEntry.f_Key();
			auto &StatusResults = StatusesEntry.f_Value();

			if (!StatusResults)
				continue;

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

		co_return fg_Move(ConnectionState);
	}

	TCFuture<CDistributedActorTrustManagerInterface::CConnectionsDebugStats> CDistributedActorTrustManager::f_GetConnectionsDebugStats()
	{
		auto CheckDestroy = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		CDistributedActorTrustManagerInterface::CConnectionsDebugStats DebugStats;

		DebugStats.m_DebugStats = co_await Internal.m_ActorDistributionManager(&CActorDistributionManager::f_GetConnectionsDebugStats);

		co_return fg_Move(DebugStats);
	}

	TCFuture<NStr::CStr> CDistributedActorTrustManager::f_GetHostFriendlyName(NStr::CStr _HostID)
	{
		auto CheckDestroy = co_await f_CheckDestroyedOnResume();
		
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (_HostID == Internal.m_BasicConfig.m_HostID)
			co_return Internal.m_FriendlyName;

		auto FrientlyName = co_await Internal.m_ActorDistributionManager(&CActorDistributionManager::f_GetHostFriendlyName, _HostID);

		if (FrientlyName)
			co_return FrientlyName;

		if (auto *pHost = Internal.m_Hosts.f_FindEqual(_HostID))
			co_return pHost->m_FriendlyName;
		
		co_return co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_GetClientLastFriendlyName, _HostID);
	}
}
