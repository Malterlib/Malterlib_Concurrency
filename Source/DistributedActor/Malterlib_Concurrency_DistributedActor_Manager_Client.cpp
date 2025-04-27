// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Network/Sockets/SSL>

namespace NMib::NConcurrency
{
	CDistributedActorConnectionReference::CDistributedActorConnectionReference(CDistributedActorConnectionReference &&) = default;
	CDistributedActorConnectionReference &CDistributedActorConnectionReference::operator = (CDistributedActorConnectionReference &&) = default;

	CDistributedActorConnectionReference::CDistributedActorConnectionReference
		(
			TCWeakActor<CActorDistributionManager> const &_DistributionManager
			, NStr::CStr const &_ConnectionID
		)
		: mp_DistributionManager(_DistributionManager)
		, mp_ConnectionID(_ConnectionID)
	{
	}

	CDistributedActorConnectionReference::CDistributedActorConnectionReference() = default;

	CDistributedActorConnectionReference::~CDistributedActorConnectionReference()
	{
		f_Clear();
	}

	bool CDistributedActorConnectionReference::f_IsValid() const
	{
		return !mp_DistributionManager.f_IsEmpty();
	}

	void CDistributedActorConnectionReference::f_Clear()
	{
		if (mp_DistributionManager)
		{
			if (auto DistributionManager = mp_DistributionManager.f_Lock())
				DistributionManager.f_Bind<&CActorDistributionManager::fp_RemoveConnection>(mp_ConnectionID, false).f_DiscardResult();

			mp_DistributionManager.f_Clear();
			mp_ConnectionID.f_Clear();
		}
	}

	TCFuture<CDistributedActorConnectionStatus> CDistributedActorConnectionReference::f_GetStatus()
	{
		if (mp_DistributionManager.f_IsEmpty())
			return DMibErrorInstance("Connection has been disconnected");

		auto DistributionManager = mp_DistributionManager.f_Lock();

		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager has been deleted");

		return DistributionManager(&CActorDistributionManager::fp_GetConnectionStatus, mp_ConnectionID);
	}

	TCFuture<void> CDistributedActorConnectionReference::f_Debug_Break(fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags)
	{
		if (mp_DistributionManager.f_IsEmpty())
			return DMibErrorInstance("Connection has been disconnected");

		auto DistributionManager = mp_DistributionManager.f_Lock();

		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager has been deleted");

		return DistributionManager(&CActorDistributionManager::fp_Debug_BreakConnection, mp_ConnectionID, _Timeout, _DebugFlags);
	}

	TCFuture<void> CDistributedActorConnectionReference::f_Reconnect()
	{
		if (mp_DistributionManager.f_IsEmpty())
			return DMibErrorInstance("Connection has been disconnected");

		auto DistributionManager = mp_DistributionManager.f_Lock();

		mp_DistributionManager.f_Clear();

		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager has been deleted");

		return DistributionManager(&CActorDistributionManager::fp_Reconnect, mp_ConnectionID);
	}

	TCFuture<void> CDistributedActorConnectionReference::f_Disconnect(bool _bPreserveHost)
	{
		if (mp_DistributionManager.f_IsEmpty())
			return DMibErrorInstance("Connection has been disconnected");

		auto DistributionManager = mp_DistributionManager.f_Lock();

		mp_DistributionManager.f_Clear();

		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager has been deleted");

		return DistributionManager(&CActorDistributionManager::fp_RemoveConnection, mp_ConnectionID, _bPreserveHost);
	}

	TCFuture<void> CDistributedActorConnectionReference::f_UpdateConnectionSettings(CActorDistributionConnectionSettings const &_Settings)
	{
		if (mp_DistributionManager.f_IsEmpty())
			return DMibErrorInstance("Connection has been disconnected");

		auto DistributionManager = mp_DistributionManager.f_Lock();

		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager has been deleted");

		return DistributionManager(&CActorDistributionManager::fp_UpdateConnectionSettings, mp_ConnectionID, _Settings);
	}

	void CActorDistributionManagerInternal::fp_ScheduleReconnect
		(
			NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
			, bool _bRetry
			, mint _Sequence
			, NStr::CStr const &_ConnectionError
			, bool _bResetHost
		)
	{
		_pConnection->f_Reset(_bResetHost, *this, "Reconnect");
		_pConnection->f_SetLastError(_ConnectionError);

		fg_Timeout(m_ReconnectDelay, true)(fg_ThisActor(m_pThis)) > [this, _pConnection, _Sequence, _bRetry]() mutable -> TCFuture<void>
			{
				if (!m_ClientConnections.f_FindEqual(_pConnection->m_ConnectionID))
					co_return {};

				if (_Sequence != _pConnection->m_ConnectionSequence || m_bPreShutdown)
					co_return {};

				fp_Reconnect(_pConnection, nullptr, _bRetry, _bRetry, 3600.0);

				co_return {};
			}
		;
	}

	void CActorDistributionManagerInternal::fp_DestroyClientConnection
		(
			CClientConnection &_Connection
			, bool _bSaveHost
			, NStr::CStr const &_Error
			, bool _bLastActiveNormalClosure
			, TCPromise<void> *_pPromise
		)
	{
		auto pHost = _Connection.m_pHost;
		if (pHost)
		{
			pHost->m_LastError = _Error;
			pHost->m_LastErrorTime = NTime::CTime::fs_NowUTC();
		}

		bool bDestroyAnon = !_bSaveHost && pHost && pHost->m_HostInfo.m_bAnonymous;
		bool bDestroyNormal = pHost && _bLastActiveNormalClosure;

		_Connection.f_Destroy(_Error, *this, _pPromise);

		if (bDestroyAnon)
		{
			fp_DestroyHost(*pHost, &_Connection, "Anonymous client connection disconnected");
			_Connection.m_pHost = nullptr;
		}
		else if (bDestroyNormal)
		{
			fp_DestroyHost(*pHost, &_Connection, "Last active client connection disconnected");
			_Connection.m_pHost = nullptr;
		}

		m_ClientConnections.f_Remove(_Connection.m_ConnectionID);
	}

	NStr::CStr CActorDistributionManagerInternal::fp_TranslateHostname(NStr::CStr const &_Hostname) const
	{
		if (m_TranslateHostnames.f_IsEmpty())
			return _Hostname;

		if (auto *pTranslate = m_TranslateHostnames.f_FindEqual(_Hostname))
			return *pTranslate;

		return _Hostname;;
	}

	NWeb::NHTTP::CURL CActorDistributionManagerInternal::fp_TranslateURL(NWeb::NHTTP::CURL const &_URL) const
	{
		if (m_TranslateHostnames.f_IsEmpty())
			return _URL;

		auto Return = _URL;
		if (auto *pTranslate = m_TranslateHostnames.f_FindEqual(_URL.f_GetHost()))
			Return.f_SetHost(*pTranslate);

		return Return;
	}

	void CActorDistributionManagerInternal::fp_Reconnect
		(
			NStorage::TCSharedPointer<CClientConnection, NStorage::CSupportWeakTag> const &_pConnection
			, NStorage::TCSharedPointer<TCPromise<CActorDistributionManager::CConnectionResult>> const &_pPromise
			, bool _bRetry
			, bool _bRetryOnFirst
			, fp64 _Timeout
		)
	{
		if (!_pConnection->m_pSSLContext || !m_WebsocketClientConnector || m_pThis->f_IsDestroyed())
			return;

		mint Sequence = ++_pConnection->m_ConnectionSequence;
		NWeb::NHTTP::CRequest Request;
		auto &EntityFields = Request.f_GetEntityFields();
		EntityFields.f_SetUnknownField("MalterlibDistributedActors_Enclave", m_Enclave);

		auto ToConnectTo = fp_TranslateURL(_pConnection->m_ServerURL);

		m_WebsocketClientConnector
			(
				&NWeb::CWebSocketClientActor::f_Connect
				, ToConnectTo.f_GetHost()
				, ""
				, NNetwork::ENetAddressType_None
				, ToConnectTo.f_GetPort()
				, ToConnectTo.f_GetFullPath()
				, ToConnectTo.f_Encode()
				, NContainer::fg_CreateVector<NStr::CStr>("MalterlibDistributedActors")
				, fg_Move(Request)
				, NNetwork::CSocket_SSL::fs_GetFactory(_pConnection->m_pSSLContext)
			)
			.f_Timeout(_Timeout, "Timed out waiting for websocket connection")
			> [this, pConnectionWeak = _pConnection.f_Weak(), _pPromise, Sequence, _bRetry, _bRetryOnFirst]
			(NConcurrency::TCAsyncResult<NWeb::CWebSocketNewClientConnection> &&_Result) mutable
			{
				auto pConnection = pConnectionWeak.f_Lock();
				if (!pConnection)
				{
					if (_pPromise)
						_pPromise->f_SetException(DMibErrorInstance("Connection deleted"));
					return;
				}

				auto fReportError = [this, _bRetry, _bRetryOnFirst, _pPromise, pConnectionWeak, Sequence](NStr::CStr const &_Error, NException::CExceptionPointer const &_Exception)
					{
						auto pConnection = pConnectionWeak.f_Lock();
						if (!pConnection)
						{
							if (_pPromise)
								_pPromise->f_SetException(DMibErrorInstance("Connection deleted"));
							return;
						}

						if (!_bRetryOnFirst)
						{
							if (_pPromise)
								_pPromise->f_SetException(_Exception);
							return;
						}
						if (_pPromise)
							_pPromise->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), pConnection->m_ConnectionID));
						fp_ScheduleReconnect(pConnection, _bRetry, Sequence, _Error);
					}
				;

				auto &Connection = *pConnection;

				if (!_Result)
				{
					auto Error = fg_Format("Error connecting to '{}': {}", Connection.m_ServerURL.f_Encode(), _Result.f_GetExceptionStr());
					if (Connection.m_LastLoggedError != Error && !m_pThis->f_IsDestroyed())
					{
						Connection.m_LastLoggedError = Error;
						DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "{}", Error);
					}
					fReportError(Error, _Result.f_GetException());
					return;
				}

				Connection.m_LastLoggedError.f_Clear();

				if (Sequence != Connection.m_ConnectionSequence)
				{
					if (_pPromise)
						_pPromise->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
					return;
				}

				auto &Result = *_Result;

				auto pSocketInfo = static_cast<NNetwork::CSocketConnectionInfo_SSL const *>(Result.m_pSocketInfo.f_Get());

				if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
				{
					NStr::CStr Error = "Missing peer certificate";
					fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
					return;
				}

				NStr::CStr RealHostID;
				try
				{
					NException::CDisableExceptionTraceScope DisableTrace;
					RealHostID = CActorDistributionManager::fs_GetCertificateHostID(pSocketInfo->m_PeerCertificate);
					if (RealHostID.f_IsEmpty())
					{
						NStr::CStr Error = "Missing or incorrect Host ID in server certificate";
						fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
						return;
					}
				}
				catch (NException::CException const &)
				{
					NStr::CStr Error = "Incorrect peer certificate";
					fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
					return;
				}

				if (!Connection.m_ExpectedRealHostID.f_IsEmpty() && Connection.m_ExpectedRealHostID != RealHostID)
				{
					NStr::CStr Error = "Host ID mismatch";
					fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
					return;
				}

				NStr::CStr Enclave;
				if (auto *pValue = _Result->m_Response.f_GetEntityFields().f_GetUnknownField("MalterlibDistributedActors_Enclave"))
					Enclave = *pValue;

				if (!Enclave.f_IsEmpty() && !CActorDistributionManager::fs_IsValidEnclave(Enclave))
				{
					NStr::CStr Error = "Invalid enclave";
					fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
					return;
				}

				NStr::CStr UniqueHostID;

				if (Connection.m_ExpectedRealHostID.f_IsEmpty())
					RealHostID = "Insecure_" + RealHostID;

				if (Connection.m_bAnonymous)
					UniqueHostID = fg_Format("Anonymous_{}_{}", RealHostID, NCryptography::fg_RandomID());
				else
					UniqueHostID = RealHostID;

				if (!Enclave.f_IsEmpty())
				{
					UniqueHostID += "_";
					UniqueHostID += Enclave;
				}

				auto &pHost = m_Hosts[UniqueHostID];
				if (!pHost)
				{
					pHost = fg_Construct(*m_pThis, CDistributedActorHostInfo{UniqueHostID, RealHostID, Connection.m_bAnonymous});
					m_HostsByRealHostID[RealHostID].f_Insert(*pHost);

					fp_CleanupMarkInactive(*pHost);
				}

				auto &Host = *pHost;

				if (Host.m_HostInfo.m_RealHostID != RealHostID || Host.m_HostInfo.m_UniqueHostID != UniqueHostID || Host.m_HostInfo.m_bAnonymous != Connection.m_bAnonymous)
				{
					NStr::CStr Error = "Host IDs mismatch";
					fReportError(Error, NException::fg_MakeException(DMibErrorInstance(Error)));
					return;
				}

				Connection.m_pHost = pHost;
				Host.m_ClientConnections.f_Insert(Connection);

				Host.m_bOutgoing = true;

				Result.m_fOnClose = g_ActorFunctorWeak / [this, pConnectionWeak, Sequence, _bRetry]
					(NWeb::EWebSocketStatus _Reason, NStr::CStr _Message, NWeb::EWebSocketCloseOrigin _Origin)
					-> TCFuture<void>
					{
						auto pConnection = pConnectionWeak.f_Lock();
						if (!pConnection)
							co_return {};

						if (Sequence != pConnection->m_ConnectionSequence)
							co_return {};

						if (!pConnection->m_pHost)
							co_return {};

						bool bLast = pConnection->m_Link.f_IsAloneInList();

						NStr::CStr Error = fg_Format
							(
								"<{}> Lost {} outgoing connection to '{}' {{{}}: {} {} {}"
								, pConnection->f_GetHostInfo()
								, bLast ? "last" : "additional"
								, pConnection->m_ServerURL.f_Encode()
								, pConnection->f_GetConnectionID()
								, _Origin == NWeb::EWebSocketCloseOrigin_Local ? "Local" : "Remote"
								, _Reason
								, _Message
							)
						;
						if (_Reason == NWeb::EWebSocketStatus_NormalClosure)
							DMibLogWithCategory(Mib/Concurrency/Actors, Info, "{}", Error);
						else
							DMibLogWithCategory(Mib/Concurrency/Actors, Warning, "{}", Error);

						auto &pHost = pConnection->m_pHost;
						if (pHost && pHost->m_HostInfo.m_bAnonymous)
							fp_DestroyClientConnection(*pConnection, false, Error, _Reason == NWeb::EWebSocketStatus_NormalClosure && bLast, nullptr);
						else
						{
							auto pHost = pConnection->m_pHost;
							bool bDestroyNormal = pHost && _Reason == NWeb::EWebSocketStatus_NormalClosure && bLast && (!_Message.f_StartsWith("Invalid connection:") || !_bRetry);
							fp_ScheduleReconnect(pConnection, true, Sequence, Error, bDestroyNormal);
							if (bDestroyNormal)
								fp_DestroyHost(*pHost, pConnection.f_Get(), "Last active client connection disconnected");
						}

						co_return {};
					}
				;

				Result.m_fOnReceiveBinaryMessage = g_ActorFunctorWeak / [this, pConnectionWeak, Sequence](NStorage::TCSharedPointer<NContainer::CIOByteVector> _pMessage)
					-> TCFuture<void>
					{
						auto pConnection = pConnectionWeak.f_Lock();
						if (!pConnection)
							co_return {};

						if (Sequence != pConnection->m_ConnectionSequence)
						{
							DMibLog
								(
									SubscriptionLogVerbosity
									, " ---- {} Invalid connection sequence. {} != {}"
									, pConnection->f_GetConnectionID()
									, Sequence
									, pConnection->m_ConnectionSequence
								)
							;
							co_return {};
						}

						if (!pConnection->m_pHost)
						{
							DMibLog(SubscriptionLogVerbosity, " ---- {} No host", pConnection->f_GetConnectionID());
							co_return {};
						}

						if (!fp_HandleProtocolIncoming(pConnection.f_Get(), _pMessage))
							fp_OnInvalidConnection(pConnection.f_Get(), DMibErrorInstance("Failed to handle incoming message").f_ExceptionPointer());

						co_return {};
					}
				;

				pConnection->f_DiscardIdentifyPromise("Reconnected");
				pConnection->m_IdentifyPromise = TCPromise<bool>();
				pConnection->m_bPulishFinished = false;
				pConnection->m_Connection = Result.f_Accept
					(
						fg_ThisActor(m_pThis)
						/
						[
							this
							, pConnectionWeak
							, _pPromise
							, Sequence
							, RealHostID
							, UniqueHostID
							, CertificateChain = pSocketInfo->m_CertificateChain
							, fReportError
						]
						(NConcurrency::TCAsyncResult<NConcurrency::CActorSubscription> &&_Callback) mutable
						{
							auto pConnection = pConnectionWeak.f_Lock();
							if (!pConnection)
							{
								if (_pPromise)
									_pPromise->f_SetException(DMibErrorInstance("Connection deleted"));
								return;
							}

							if (Sequence != pConnection->m_ConnectionSequence)
							{
								if (_pPromise)
									_pPromise->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
								return;
							}

							if (!pConnection->m_Connection)
							{
								NStr::CStr Error = fg_Format
									(
										"Disconnected: {}"
										, pConnection->m_LastConnectionError
									)
								;
								fReportError(Error, fg_MakeException(DMibErrorInstance("Disconnected")));
								return;
							}

							if (!_Callback)
							{
								NStr::CStr Error = fg_Format
									(
										"Error accepting connection to '{}': {}"
										, pConnection->m_ServerURL.f_Encode()
										, _Callback.f_GetExceptionStr()
									)
								;
								fReportError(Error, _Callback.f_GetException());
								return;
							}

							pConnection->m_ConnectionSubscription = fg_Move(*_Callback);

							pConnection->m_IdentifyPromise.f_Future()
								>
								[
									this
									, RealHostID
									, UniqueHostID
									, Sequence
									, pConnectionWeak
									, _pPromise
									, CertificateChain = fg_Move(CertificateChain)
									, fReportError
								]
								(TCAsyncResult<bool> &&_Result) mutable
								{
									auto pConnection = pConnectionWeak.f_Lock();
									if (!pConnection)
									{
										if (_pPromise)
											_pPromise->f_SetException(DMibErrorInstance("Connection deleted"));
										return;
									}

									if (Sequence != pConnection->m_ConnectionSequence)
									{
										if (_pPromise)
											_pPromise->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
										return;
									}
									if (!_Result)
									{
										fReportError(fg_Format("Error identifying connection: {}", _Result.f_GetExceptionStr()), _Result.f_GetException());
										return;
									}
									if (!pConnection->m_pHost)
										return;

									NStr::CStr ToLog = NStr::fg_Format
										(
											"<{}> Connected '{}' {{{}}"
											, pConnection->f_GetHostInfo()
											, pConnection->m_ServerURL.f_Encode()
											, pConnection->f_GetConnectionID()
										)
									;

									if (*_Result)
										DMibLogWithCategory(Mib/Concurrency/Actors, Info, "{}", ToLog);
									else
										DMibLogWithCategory(Mib/Concurrency/Actors, SubscriptionLogVerbosity, "{} Identify false", ToLog);

									if (_pPromise)
									{
										CActorDistributionManager::CConnectionResult ConnectionResult{fg_ThisActor(m_pThis), pConnection->m_ConnectionID};
										ConnectionResult.m_CertificateChain = fg_Move(CertificateChain);
										ConnectionResult.m_UniqueHostID = UniqueHostID;
										ConnectionResult.m_HostInfo.m_HostID = RealHostID;
										ConnectionResult.m_HostInfo.m_FriendlyName = pConnection->m_pHost->m_FriendlyName;
										_pPromise->f_SetResult(fg_Move(ConnectionResult));
									}
									pConnection->m_bConnected = true;
								}
							;
						}
					)
				;
				fp_Identify(pConnection.f_Get());
			}
		;
	}

	NException::CExceptionPointer CActorDistributionManagerInternal::fp_DecodeClientConnectionSettings
		(
			CActorDistributionConnectionSettings const &_Settings
			, CDecodedClientConnectionSetting &o_DecodedSettings
		)
	{
		bool &bAnonymous = o_DecodedSettings.m_bAnonymous;
		NStr::CStr &RealHostID = o_DecodedSettings.m_RealHostID;
		NNetwork::CSSLSettings &ClientSettings = o_DecodedSettings.m_ClientSettings;

		bAnonymous = _Settings.m_PrivateClientKey.f_IsEmpty();

		if ((_Settings.m_bRetryConnectOnFailure || _Settings.m_bRetryConnectOnFirstFailure) && bAnonymous)
			return DMibErrorInstance("Anonymous connections cannot reconnect on failure");

		if (_Settings.m_PublicServerCertificate.f_IsEmpty())
		{
			if (_Settings.m_bAllowInsecureConnection)
				ClientSettings.m_VerificationFlags = NNetwork::CSSLSettings::EVerificationFlag_IgnoreVerificationFailures;
			else
				ClientSettings.m_VerificationFlags = NNetwork::CSSLSettings::EVerificationFlag_UseOSStoreIfNoCASpecified;
		}
		else
		{
			ClientSettings.m_CACertificateData = _Settings.m_PublicServerCertificate;
			try
			{
				RealHostID = CActorDistributionManager::fs_GetCertificateHostID(_Settings.m_PublicServerCertificate);
			}
			catch (NException::CException const &_Exception)
			{
				return DMibErrorInstance(fg_Format("Error getting server host ID from certificate: {}", _Exception.f_GetErrorStr()));
			}

			if (RealHostID.f_IsEmpty())
			{
				// TODO: Make sure this does not compile
				return DMibErrorInstance("Server certifate has no host ID");
			}
		}

		if (!_Settings.m_PrivateClientKey.f_IsEmpty())
		{
			ClientSettings.m_PrivateKeyData = _Settings.m_PrivateClientKey;
			ClientSettings.m_PublicCertificateData = _Settings.m_PublicClientCertificate;
		}

		return {};
	}

	TCFuture<CActorDistributionManager::CConnectionResult> CActorDistributionManager::f_Connect(CActorDistributionConnectionSettings _Settings, fp64 _Timeout)
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_bPreShutdown)
			co_return DMibErrorInstance("Distribution manager is shutting down");

		CActorDistributionManagerInternal::CDecodedClientConnectionSetting DecodedSettings;
		if (auto pError = Internal.fp_DecodeClientConnectionSettings(_Settings, DecodedSettings))
			co_return fg_Move(pError);

		if (!Internal.m_WebsocketClientConnector)
			Internal.m_WebsocketClientConnector = NConcurrency::fg_ConstructActor<NWeb::CWebSocketClientActor>(Internal.m_WebsocketSettings);

		NStr::CStr ConnectionID = NCryptography::fg_RandomID(Internal.m_ClientConnections);

		auto pConnection = Internal.m_ClientConnections[ConnectionID] = fg_Construct();

		pConnection->m_ConnectionID = ConnectionID;
		pConnection->m_ExpectedRealHostID = DecodedSettings.m_RealHostID;
		pConnection->m_bAnonymous = DecodedSettings.m_bAnonymous;

		pConnection->m_bRetryConnectOnFailure = _Settings.m_bRetryConnectOnFailure;
		pConnection->m_bRetryConnectOnFirstFailure = _Settings.m_bRetryConnectOnFirstFailure;

		try
		{
			pConnection->m_pSSLContext = fg_Construct(NNetwork::CSSLContext::EType_Client, DecodedSettings.m_ClientSettings);
		}
		catch (NException::CException const &_Exception)
		{
			co_return DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr()));
		}
		pConnection->m_ServerURL = _Settings.m_ServerURL;

		TCPromiseFuturePair<CActorDistributionManager::CConnectionResult> Promise;
		Internal.fp_Reconnect(pConnection, fg_Construct(fg_Move(Promise.m_Promise)), _Settings.m_bRetryConnectOnFailure, _Settings.m_bRetryConnectOnFirstFailure, _Timeout);

		co_return co_await fg_Move(Promise.m_Future);
	}

	TCFuture<CDistributedActorConnectionStatus> CActorDistributionManager::fp_GetConnectionStatus(NStr::CStr _ConnectionID)
	{
		auto &Internal = *mp_pInternal;
		auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
		if (!pConnection)
			co_return DMibErrorInstance("No such client connection");

		auto &Connection = **pConnection;
		CDistributedActorConnectionStatus Return;
		if (Connection.m_pHost)
		{
			Return.m_HostInfo.m_HostID = Connection.m_pHost->m_HostInfo.m_RealHostID;
			Return.m_HostInfo.m_FriendlyName = Connection.m_pHost->m_FriendlyName;
		}

		Return.m_bConnected = Connection.m_bConnected;
		Return.m_Error = Connection.m_LastConnectionError;
		Return.m_ErrorTime = Connection.m_LastConnectionErrorTime;

		co_return fg_Move(Return);
	}

	TCFuture<void> CActorDistributionManager::fp_Debug_BreakConnection(NStr::CStr _ConnectionID, fp64 _Timeout, NNetwork::ESocketDebugFlag _DebugFlags)
	{
		auto &Internal = *mp_pInternal;

		auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
		if (!pConnection)
			co_return {};

		auto &Connection = **pConnection;
		if (Connection.m_Connection)
			co_await Connection.m_Connection(&NWeb::CWebSocketActor::f_DebugSetFlags, _Timeout, _DebugFlags);

		co_return {};
	}

	TCFuture<void> CActorDistributionManager::fp_RemoveConnection(NStr::CStr _ConnectionID, bool _bPreserveHost)
	{
		auto &Internal = *mp_pInternal;

		auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
		if (!pConnection)
			co_return {};

		auto &Connection = **pConnection;

		TCPromiseFuturePair<void> Promise;

		Internal.fp_DestroyClientConnection
			(
				Connection
				, false
				, _bPreserveHost ? "Remove connection (preserve host)" : "Remove connection"
				, !_bPreserveHost && Connection.m_Link.f_IsAloneInList()
				, &Promise.m_Promise
			)
		;

		co_return co_await fg_Move(Promise.m_Future);
	}

	TCFuture<void> CActorDistributionManager::fp_Reconnect(NStr::CStr _ConnectionID)
	{
		using namespace NStr;

		auto &Internal = *mp_pInternal;

		auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);

		if (!pConnection)
			co_return DMibErrorInstance("No such client connection");

		if (Internal.m_bPreShutdown)
			co_return DMibErrorInstance("Distribution manager is shutting down");

		auto &Connection = **pConnection;

		Connection.f_Reset(false, Internal, "Reconnect");
		Connection.f_SetLastError("Manual reconnect");

		Internal.fp_Reconnect(*pConnection, nullptr, Connection.m_bRetryConnectOnFailure, Connection.m_bRetryConnectOnFirstFailure, 3600.0);

		co_return {};
	}

	TCFuture<void> CActorDistributionManager::fp_UpdateConnectionSettings(NStr::CStr _ConnectionID, CActorDistributionConnectionSettings _Settings)
	{
		auto &Internal = *mp_pInternal;
		auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
		if (!pConnection)
			co_return DMibErrorInstance("No such client connection");

		auto &Connection = **pConnection;
		if (!Connection.m_pHost)
			co_return DMibErrorInstance("Client has no host");

		auto &Host = *Connection.m_pHost;

		CActorDistributionManagerInternal::CDecodedClientConnectionSetting DecodedSettings;
		if (auto pError = Internal.fp_DecodeClientConnectionSettings(_Settings, DecodedSettings))
			co_return fg_Move(pError);

		if (DecodedSettings.m_bAnonymous != Host.m_HostInfo.m_bAnonymous)
			co_return DMibErrorInstance("You cannot change the anonymous status of the connection");

		if (DecodedSettings.m_RealHostID != Host.m_HostInfo.m_RealHostID)
			co_return DMibErrorInstance("You cannot change the host ID of the connection");

		NStorage::TCSharedPointer<NNetwork::CSSLContext> pNewSSLContext;
		try
		{
			pNewSSLContext = fg_Construct(NNetwork::CSSLContext::EType_Client, DecodedSettings.m_ClientSettings);
		}
		catch (NException::CException const &_Exception)
		{
			co_return DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr()));
		}

		Connection.m_pSSLContext = fg_Move(pNewSSLContext);
		Connection.m_ServerURL = _Settings.m_ServerURL;

		co_return {};
	}
}
