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
					DistributionManager(&CActorDistributionManager::fp_RemoveConnection, mp_ConnectionID) > fg_DiscardResult();
				
				mp_DistributionManager.f_Clear();
				mp_ConnectionID.f_Clear();
			}
		}

		TCDispatchedActorCall<CDistributedActorConnectionStatus> CDistributedActorConnectionReference::f_GetStatus() 
		{
			bool bAlreadyRemoved = mp_DistributionManager.f_IsEmpty();
			auto DistributionManager = mp_DistributionManager.f_Lock();
			return fg_ConcurrentDispatch
				(
					[bAlreadyRemoved, DistributionManager = fg_Move(DistributionManager), ConnectionID = mp_ConnectionID]
					{
						TCContinuation<CDistributedActorConnectionStatus> Continuation;
						if (DistributionManager)
						{
							DistributionManager(&CActorDistributionManager::fp_GetConnectionStatus, ConnectionID) 
								> [Continuation](TCAsyncResult<CDistributedActorConnectionStatus> &&_ConnectionStatus)
								{
									Continuation.f_SetResult(fg_Move(_ConnectionStatus));
								}
							;
							return Continuation;
						}
						if (bAlreadyRemoved)
							Continuation.f_SetException(DMibErrorInstance("Connection has been disconnected"));
						else
							Continuation.f_SetException(DMibErrorInstance("Distribution manager has been deleted"));

						return Continuation;
					}
				)
			;
		}
		
		TCDispatchedActorCall<void> CDistributedActorConnectionReference::f_Disconnect() 
		{
			bool bAlreadyRemoved = mp_DistributionManager.f_IsEmpty();
			auto DistributionManager = mp_DistributionManager.f_Lock();
			mp_DistributionManager.f_Clear();
			return fg_ConcurrentDispatch
				(
					[bAlreadyRemoved, DistributionManager = fg_Move(DistributionManager), ConnectionID = mp_ConnectionID]
					{
						TCContinuation<void> Continuation;
						if (DistributionManager)
						{
							DistributionManager(&CActorDistributionManager::fp_RemoveConnection, ConnectionID) > [Continuation](TCAsyncResult<void> &&_Result)
								{
									Continuation.f_SetResult(fg_Move(_Result));
								}
							;
							return Continuation;
						}
						if (bAlreadyRemoved)
							Continuation.f_SetException(DMibErrorInstance("Connection has already been disconnected"));
						else
							Continuation.f_SetException(DMibErrorInstance("Distribution manager has been deleted"));
						
						return Continuation;
					}
				)
			;
		}
		
		TCDispatchedActorCall<void> CDistributedActorConnectionReference::f_UpdateConnectionSettings(CActorDistributionConnectionSettings const &_Settings)
		{
			auto DistributionManager = mp_DistributionManager.f_Lock();
			return fg_ConcurrentDispatch
				(
					[DistributionManager = fg_Move(DistributionManager), ConnectionID = mp_ConnectionID, _Settings]
					{
						TCContinuation<void> Continuation;
						
						if (!DistributionManager)
						{
							Continuation.f_SetException(DMibErrorInstance("Conection disconnected"));
							return Continuation;
						}
						DistributionManager(&CActorDistributionManager::fp_UpdateConnectionSettings, ConnectionID, _Settings) > Continuation;
						return Continuation;
					}
				)
			;
		}
		
		void CActorDistributionManagerInternal::fp_ScheduleReconnect
			(
				NPtr::TCSharedPointer<CClientConnection> const &_pConnection
				, NPtr::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
				, bool _bRetry
				, mint _Sequence
				, NStr::CStr const &_ConnectionError 
			)
		{
			_pConnection->f_Reset(false);
			_pConnection->f_SetLastError(_ConnectionError);
			
			fg_TimerActor()
				(
					&CTimerActor::f_OneshotTimer
					, 0.5
					, fg_ThisActor(m_pThis)
					, [this, _pConnection, _Sequence, _pContinuation, _bRetry]() mutable
					{
						if (!m_ClientConnections.f_FindEqual(_pConnection->m_ConnectionID))
							return;
						if (_Sequence != _pConnection->m_ConnectionSequence)
							return;
						fp_Reconnect(_pConnection, _pContinuation, _bRetry);
					}
				)
				> fg_DiscardResult()
			;
		}
		
		void CActorDistributionManagerInternal::fp_DestroyClientConnection(CClientConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error)
		{
			_Connection.f_Destroy(_Error);
			auto &pHost = _Connection.m_pHost; 
			if (!_bSaveHost && pHost && pHost->m_bAnonymous)
			{
				fp_DestroyHost(*pHost, &_Connection);
				pHost = nullptr;
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
		NHTTP::CURL CActorDistributionManagerInternal::fp_TranslateURL(NHTTP::CURL const &_URL) const
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
				NPtr::TCSharedPointer<CClientConnection> const &_pConnection
				, NPtr::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
				, bool _bRetry
			)
		{
			if (!_pConnection->m_pSSLContext)
				return;
			
			mint Sequence = ++_pConnection->m_ConnectionSequence;
			NHTTP::CRequest Request;
			auto &EntityFields = Request.f_GetEntityFields();
			EntityFields.f_SetUnknownField("MalterlibDistributedActors_Enclave", m_Enclave);
			
			auto ToConnectTo = fp_TranslateURL(_pConnection->m_ServerURL);
			
			m_WebsocketClientConnector
				(
					&NWeb::CWebSocketClientActor::f_Connect
					, ToConnectTo.f_GetHost()
					, ""
					, NNet::ENetAddressType_None
					, ToConnectTo.f_GetPort()
					, ToConnectTo.f_GetFullPath()
					, ToConnectTo.f_Encode()
					, NContainer::fg_CreateVector<NStr::CStr>("MalterlibDistributedActors")
					, fg_Move(Request)
					, NNet::CSocket_SSL::fs_GetFactory(_pConnection->m_pSSLContext)
				)
				> [this, _pConnection, _pContinuation, Sequence, _bRetry](NConcurrency::TCAsyncResult<NWeb::CWebSocketNewClientConnection> &&_Result) mutable
				{
					auto fReportError = [this, _bRetry, _pContinuation, _pConnection, Sequence](NStr::CStr const &_Error, CExceptionPointer const &_Exception)
						{
							if (!_bRetry)
							{
								if (_pContinuation)
									_pContinuation->f_SetException(_Exception);
								return;
							}
							if (_pContinuation)
								_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
							fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, _Error);
						}
					;
					
					auto &Connection = *_pConnection;
					
					if (!_Result)
					{
						auto Error = fg_Format("Error connecting to '{}': {}", Connection.m_ServerURL.f_Encode(), _Result.f_GetExceptionStr());
						if (Connection.m_LastLoggedError != Error)
						{
							Connection.m_LastLoggedError = Error;
							DMibLogWithCategory(Mib/Concurrency/Actors, Error, "{}", Error);
						}
						fReportError(Error, _Result.f_GetException());
						return;
					}
					
					Connection.m_LastLoggedError.f_Clear();
					
					if (Sequence != Connection.m_ConnectionSequence)
					{
						if (_pContinuation)
							_pContinuation->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
						return;
					}
					
					auto &Result = *_Result;
					
					auto pSocketInfo = static_cast<NNet::CSocketConnectionInfo_SSL const *>(Result.m_pSocketInfo.f_Get());

					if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
					{
						NStr::CStr Error = "Missing peer certificate";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
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
							fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
							return;
						}
					}
					catch (NException::CException const &_Exception)
					{
						NStr::CStr Error = "Incorrect peer certificate";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
						return;
					}

					if (!Connection.m_ExpectedRealHostID.f_IsEmpty() && Connection.m_ExpectedRealHostID != RealHostID)
					{
						NStr::CStr Error = "Host ID mismatch";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
						return;
					}
					
					NStr::CStr Enclave;
					if (auto *pValue = _Result->m_Response.f_GetEntityFields().f_GetUnknownField("MalterlibDistributedActors_Enclave"))
						Enclave = *pValue;
					
					if (!Enclave.f_IsEmpty() && !CActorDistributionManager::fs_IsValidEnclave(Enclave))
					{
						NStr::CStr Error = "Invalid enclave";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
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
						pHost = fg_Construct(*m_pThis);
						auto &Host = *pHost;
						Host.m_RealHostID = RealHostID;
						Host.m_UniqueHostID = UniqueHostID;
						Host.m_bAnonymous = Connection.m_bAnonymous;
						m_HostsByRealHostID[RealHostID].f_Insert(Host);
					}
					
					auto &Host = *pHost;
					
					if (Host.m_RealHostID != RealHostID || Host.m_UniqueHostID != UniqueHostID || Host.m_bAnonymous != Connection.m_bAnonymous)
					{
						NStr::CStr Error = "Host IDs mismatch";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
						return;
					}
					
					Connection.m_pHost = pHost;
					Host.m_ClientConnections.f_Insert(Connection);
					
					Host.m_bOutgoing = true;
					
					Result.m_fOnClose = [this, _pConnection, Sequence](NWeb::EWebSocketStatus _Reason, NStr::CStr const &_Message, NWeb::EWebSocketCloseOrigin _Origin)
						{
							if (Sequence != _pConnection->m_ConnectionSequence)
								return;
							if (!_pConnection->m_pHost)
								return;
							
							NStr::CStr Error = fg_Format
								(
									"Lost outgoing connection to '{}' <{}>: {} {} {}"
									, _pConnection->m_ServerURL.f_Encode()
									, _pConnection->f_GetHostInfo().f_GetDesc()
									, _Origin == NWeb::EWebSocketCloseOrigin_Local ? "Local" : "Remote"
									, _Reason	
									, _Message
								)
							;
							
							DMibLogWithCategory
								(
									Mib/Concurrency/Actors
									, Error
									, "{}"
									, Error
								)
							;
							auto &pHost = _pConnection->m_pHost; 
							if (pHost && pHost->m_bAnonymous)
								fp_DestroyClientConnection(*_pConnection, false, Error);
							else
								fp_ScheduleReconnect(_pConnection, nullptr, true, Sequence, Error);
						}
					;
					
					Result.m_fOnReceiveBinaryMessage = [this, _pConnection, Sequence](NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage)
						{
							if (Sequence != _pConnection->m_ConnectionSequence)
							{
								DMibLog(DebugVerbose2, " ---- {} {} Invalid connection secuence", _pConnection->m_pHost->m_bIncoming, _pConnection->f_GetConnectionID());
								return;
							}
							if (!_pConnection->m_pHost)
							{
								DMibLog(DebugVerbose2, " ---- {} No host", _pConnection->f_GetConnectionID());
								return;
							}
							if (!fp_HandleProtocolIncoming(_pConnection.f_Get(), _pMessage))
							{
								// TODO: Assume malicious client
							}
						}
					;

					_pConnection->m_IdentifyContinuation = TCContinuation<void>();
					_pConnection->m_Connection = Result.f_Accept
						(
							fg_ThisActor(m_pThis) 
							/ 
							[
								this
								, _pConnection
								, _pContinuation
								, Sequence
								, RealHostID
								, UniqueHostID
								, CertificateChain = pSocketInfo->m_CertificateChain
								, fReportError
							]
							(NConcurrency::TCAsyncResult<NConcurrency::CActorSubscription> &&_Callback) mutable
							{
								if (Sequence != _pConnection->m_ConnectionSequence)
								{
									if (_pContinuation)
										_pContinuation->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
									return;
								}
								
								if (!_pConnection->m_Connection)
								{
									NStr::CStr Error = fg_Format
										(
											"Disconnected: {}"
											, _pConnection->m_LastConnectionError
										)
									;
									fReportError(Error, _Callback.f_GetException());
									return;
								}
								
								if (!_Callback)
								{
									NStr::CStr Error = fg_Format
										(
											"Error accepting connection to '{}': {}"
											, _pConnection->m_ServerURL.f_Encode()
											, _Callback.f_GetExceptionStr()
										)
									;
									fReportError(Error, _Callback.f_GetException());
									return;
								}

								_pConnection->m_ConnectionSubscription = fg_Move(*_Callback);

								fg_Dispatch
									(
										[this, _pConnection]
										{
											return _pConnection->m_IdentifyContinuation;
										}
									)
									> [this, RealHostID, UniqueHostID, Sequence, _pConnection, _pContinuation, CertificateChain = fg_Move(CertificateChain)]
									(TCAsyncResult<void> &&_Result) mutable
									{
										if (Sequence != _pConnection->m_ConnectionSequence)
										{
											if (_pContinuation)
												_pContinuation->f_SetException(DMibErrorInstance("Connection sequence mismatch"));
											return;
										}
										if (!_Result)
										{
											if (_pContinuation)
												_pContinuation->f_SetException(fg_Move(_Result));
											return;
										}
										
										DMibLogWithCategory
											(
												Mib/Concurrency/Actors
												, Info
												, "Connection({}) to '{}' <{}> was successful. Host ID: {}   Unique host ID: {}"
												, _pConnection->f_GetConnectionID()
												, _pConnection->m_ServerURL.f_Encode()
												, _pConnection->f_GetHostInfo().f_GetDesc()
												, RealHostID
												, UniqueHostID
											)
										;

										if (_pContinuation)
										{
											CActorDistributionManager::CConnectionResult ConnectionResult{fg_ThisActor(m_pThis), _pConnection->m_ConnectionID};
											ConnectionResult.m_CertificateChain = fg_Move(CertificateChain);
											ConnectionResult.m_RealHostID = RealHostID;
											ConnectionResult.m_UniqueHostID = UniqueHostID;
											ConnectionResult.m_HostInfo.m_HostID = RealHostID;
											ConnectionResult.m_HostInfo.m_FriendlyName = _pConnection->m_pHost->m_FriendlyName;
											_pContinuation->f_SetResult(fg_Move(ConnectionResult));
										}
										_pConnection->m_bConnected = true;
									}
								;
								
								fp_Identify(_pConnection.f_Get());
							}
						)
					; 
				}
			;
		}

		template <typename tf_CReturnType>
		bool CActorDistributionManagerInternal::fp_DecodeClientConnectionSettings
			(
				CActorDistributionConnectionSettings const &_Settings
				, TCContinuation<tf_CReturnType> &_Continuation
				, CDecodedClientConnectionSetting &o_DecodedSettings
			)
		{
			bool &bAnonymous = o_DecodedSettings.m_bAnonymous; 
			NStr::CStr &RealHostID = o_DecodedSettings.m_RealHostID;
			NNet::CSSLSettings &ClientSettings = o_DecodedSettings.m_ClientSettings;
			
			bAnonymous = _Settings.m_PrivateClientKey.f_IsEmpty();
			
			if (_Settings.m_bRetryConnectOnFailure && bAnonymous)
			{
				_Continuation = DMibErrorInstance("Anonymous connections cannot reconnect on failure"); 
				return false;
			}
			
			if (_Settings.m_PublicServerCertificate.f_IsEmpty())
			{
				if (_Settings.m_bAllowInsecureConnection)
					ClientSettings.m_VerificationFlags = NNet::CSSLSettings::EVerificationFlag_IgnoreVerificationFailures;
				else
					ClientSettings.m_VerificationFlags = NNet::CSSLSettings::EVerificationFlag_UseOSStoreIfNoCASpecified;
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
					_Continuation = DMibErrorInstance(fg_Format("Error getting server host ID from certificate: {}", _Exception.f_GetErrorStr()));
					return false;
				}
				
				if (RealHostID.f_IsEmpty())
				{
					_Continuation = DMibErrorInstance("Server certifate has no host ID"); 
					return false;
				}
			}
			if (!_Settings.m_PrivateClientKey.f_IsEmpty())
			{
				ClientSettings.m_PrivateKeyData = _Settings.m_PrivateClientKey;
				ClientSettings.m_PublicCertificateData = _Settings.m_PublicClientCertificate; 
			}
			return true;
		}
		
		TCContinuation<CActorDistributionManager::CConnectionResult> CActorDistributionManager::f_Connect(CActorDistributionConnectionSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;
			
			TCContinuation<CActorDistributionManager::CConnectionResult> Continuation;
			
			CActorDistributionManagerInternal::CDecodedClientConnectionSetting DecodedSettings;
			if (!Internal.fp_DecodeClientConnectionSettings(_Settings, Continuation, DecodedSettings))
				return Continuation;

			if (!Internal.m_WebsocketClientConnector)
				Internal.m_WebsocketClientConnector = NConcurrency::fg_ConstructActor<NWeb::CWebSocketClientActor>();
			
			NStr::CStr ConnectionID = NCryptography::fg_RandomID();

			auto pConnection = Internal.m_ClientConnections[ConnectionID] = fg_Construct();
			
			pConnection->m_ConnectionID = ConnectionID;
			pConnection->m_ExpectedRealHostID = DecodedSettings.m_RealHostID;
			pConnection->m_bAnonymous = DecodedSettings.m_bAnonymous;
			
			try
			{
				pConnection->m_pSSLContext = fg_Construct(NNet::CSSLContext::EType_Client, DecodedSettings.m_ClientSettings);
			}
			catch (NException::CException const &_Exception)
			{
				return DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr()));
			}
			pConnection->m_ServerURL = _Settings.m_ServerURL;
			
			Internal.fp_Reconnect(pConnection, fg_Construct(Continuation), _Settings.m_bRetryConnectOnFailure);
			
			return Continuation;
		}

		TCContinuation<CDistributedActorConnectionStatus> CActorDistributionManager::fp_GetConnectionStatus(NStr::CStr const &_ConnectionID)
		{
			auto &Internal = *mp_pInternal;
			auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
			if (!pConnection)
				return DMibErrorInstance("No such client connection");
			
			auto &Connection = **pConnection;
			CDistributedActorConnectionStatus Return;
			if (Connection.m_pHost)
			{
				Return.m_HostInfo.m_HostID = Connection.m_pHost->m_RealHostID;
				Return.m_HostInfo.m_FriendlyName = Connection.m_pHost->m_FriendlyName;
			}
			Return.m_bConnected = Connection.m_bConnected;
			Return.m_Error = Connection.m_LastConnectionError;
			Return.m_ErrorTime = Connection.m_LastConnectionErrorTime;
			return fg_Explicit(Return);
		}

		void CActorDistributionManager::fp_RemoveConnection(NStr::CStr const &_ConnectionID)
		{
			auto &Internal = *mp_pInternal;
			auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
			if (pConnection)
				Internal.fp_DestroyClientConnection(**pConnection, false, "Remove connection");
		}
		
		TCContinuation<void> CActorDistributionManager::fp_UpdateConnectionSettings(NStr::CStr const &_ConnectionID, CActorDistributionConnectionSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;
			auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
			if (!pConnection)
				return DMibErrorInstance("No such client connection");
			
			auto &Connection = **pConnection;
			if (!Connection.m_pHost)
				return DMibErrorInstance("Client has no host");
			
			auto &Host = *Connection.m_pHost;
		
			TCContinuation<void> Continuation;
			CActorDistributionManagerInternal::CDecodedClientConnectionSetting DecodedSettings;
			if (!Internal.fp_DecodeClientConnectionSettings(_Settings, Continuation, DecodedSettings))
				return Continuation;
			
			if (DecodedSettings.m_bAnonymous != Host.m_bAnonymous)
				return DMibErrorInstance("You cannot change the anonymous status of the connection");
			if (DecodedSettings.m_RealHostID != Host.m_RealHostID)
				return DMibErrorInstance("You cannot change the host ID of the connection");
				
			NPtr::TCSharedPointer<NNet::CSSLContext> pNewSSLContext;
			try
			{
				pNewSSLContext = fg_Construct(NNet::CSSLContext::EType_Client, DecodedSettings.m_ClientSettings);
			}
			catch (NException::CException const &_Exception)
			{
				return DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr()));
			}
			
			Connection.m_pSSLContext = fg_Move(pNewSSLContext);
			Connection.m_ServerURL = _Settings.m_ServerURL;
			
			return Continuation;
		}
	}
}
