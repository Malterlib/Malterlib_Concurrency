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
		
		void CActorDistributionManager::CInternal::fp_ScheduleReconnect
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
		
		void CActorDistributionManager::CInternal::fp_DestroyClientConnection(CClientConnection &_Connection, bool _bSaveHost, NStr::CStr const &_Error)
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

		void CActorDistributionManager::CInternal::fp_Reconnect
			(
				NPtr::TCSharedPointer<CClientConnection> const &_pConnection
				, NPtr::TCSharedPointer<TCContinuation<CActorDistributionManager::CConnectionResult>> const &_pContinuation
				, bool _bRetry
			)
		{
			if (!_pConnection->m_pSSLContext)
				return;
			
			mint Sequence = ++_pConnection->m_ConnectionSequence;
			m_WebsocketClientConnector
				(
					&NWeb::CWebSocketClientActor::f_Connect
					, _pConnection->m_ServerURL.f_GetHost()
					, ""
					, NNet::ENetAddressType_None
					, _pConnection->m_ServerURL.f_GetPort()
					, _pConnection->m_ServerURL.f_GetFullPath()
					, _pConnection->m_ServerURL.f_Encode()
					, NContainer::fg_CreateVector<NStr::CStr>("MalterlibDistributedActors")
					, NHTTP::CRequest()
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
					
					if (!_Result)
					{
						auto Error = fg_Format("Error connecting to '{}': {}", _pConnection->m_ServerURL.f_Encode(), _Result.f_GetExceptionStr());
						if (_pConnection->m_LastLoggedError != Error)
						{
							_pConnection->m_LastLoggedError = Error;
							DMibLogWithCategory(Mib/Concurrency/Actors, Error, "{}", Error);
						}
						fReportError(Error, _Result.f_GetException());
						return;
					}
					
					_pConnection->m_LastLoggedError.f_Clear();
					
					if (Sequence != _pConnection->m_ConnectionSequence)
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
					
					NStr::CStr HostID;
					try
					{
						NException::CDisableExceptionTraceScope DisableTrace;
						HostID = fs_GetCertificateHostID(pSocketInfo->m_PeerCertificate);
						if (HostID.f_IsEmpty())
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
					
					auto &pHost = _pConnection->m_pHost;
					
					DMibFastCheck(pHost);
					
					if (pHost->m_RealHostID.f_IsEmpty())
						pHost->m_RealHostID = HostID;
					else if (pHost->m_RealHostID != HostID)
					{
						NStr::CStr Error = "Host ID mismatch";
						fReportError(Error, fg_ExceptionPointer(DMibErrorInstance(Error)));						
						return;
					}

					auto &Host = *pHost;
					
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
								, HostID
								, UniqueHostID = pHost->m_UniqueHostID
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
									> [this, HostID, UniqueHostID, Sequence, _pConnection, _pContinuation, CertificateChain = fg_Move(CertificateChain)]
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
												, HostID
												, UniqueHostID
											)
										;

										if (_pContinuation)
										{
											CActorDistributionManager::CConnectionResult ConnectionResult{fg_ThisActor(m_pThis), _pConnection->m_ConnectionID};
											ConnectionResult.m_CertificateChain = fg_Move(CertificateChain);
											ConnectionResult.m_RealHostID = HostID;
											ConnectionResult.m_UniqueHostID = UniqueHostID;
											ConnectionResult.m_HostInfo.m_HostID = HostID;
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
		bool CActorDistributionManager::CInternal::fp_DecodeClientConnectionSettings
			(
				CActorDistributionConnectionSettings const &_Settings
				, TCContinuation<tf_CReturnType> &_Continuation
				, CDecodedClientConnectionSetting &o_DecodedSettings
			)
		{
			bool &bAnonymous = o_DecodedSettings.m_bAnonymous; 
			NStr::CStr &UniqueHostID = o_DecodedSettings.m_UniqueHostID;
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
				UniqueHostID = fg_Format("Anonymous_{}", NCryptography::fg_RandomID());
			}
			else
			{
				ClientSettings.m_CACertificateData = _Settings.m_PublicServerCertificate;
				try
				{
					RealHostID = UniqueHostID = fs_GetCertificateHostID(_Settings.m_PublicServerCertificate);
				}
				catch (NException::CException const &_Exception)
				{
					_Continuation = DMibErrorInstance(fg_Format("Error getting server host ID from certificate: {}", _Exception.f_GetErrorStr()));
					return false;
				}
				if (bAnonymous)
					UniqueHostID = fg_Format("Anonymous_{}_{}", RealHostID, NCryptography::fg_RandomID());
				
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
			
			CInternal::CDecodedClientConnectionSetting DecodedSettings;
			if (!Internal.fp_DecodeClientConnectionSettings(_Settings, Continuation, DecodedSettings))
				return Continuation;

			auto &pHost = Internal.m_Hosts[DecodedSettings.m_UniqueHostID];
			if (!pHost)
			{
				pHost = fg_Construct(*this);
				pHost->m_RealHostID = DecodedSettings.m_RealHostID;
				pHost->m_UniqueHostID = DecodedSettings.m_UniqueHostID;
				pHost->m_bAnonymous = DecodedSettings.m_bAnonymous;
			}
			else
				DMibFastCheck(!DecodedSettings.m_bAnonymous);
			
			if (!Internal.m_WebsocketClientConnector)
				Internal.m_WebsocketClientConnector = NConcurrency::fg_ConstructActor<NWeb::CWebSocketClientActor>();
			
			NStr::CStr ConnectionID = NCryptography::fg_RandomID();

			auto pConnection = Internal.m_ClientConnections[ConnectionID] = fg_Construct();
			
			pConnection->m_ConnectionID = ConnectionID;
			pConnection->m_pHost = pHost;
			pHost->m_ClientConnections.f_Insert(*pConnection);
			
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
			Return.m_HostInfo.m_HostID = Connection.m_pHost->m_RealHostID;
			Return.m_HostInfo.m_FriendlyName = Connection.m_pHost->m_FriendlyName; 
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
			CInternal::CDecodedClientConnectionSetting DecodedSettings;
			if (!Internal.fp_DecodeClientConnectionSettings(_Settings, Continuation, DecodedSettings))
				return Continuation;
			
			if (DecodedSettings.m_bAnonymous != Host.m_bAnonymous)
				return DMibErrorInstance("You cannot change the anonymous status of the connection");
			if (DecodedSettings.m_UniqueHostID != Host.m_UniqueHostID || DecodedSettings.m_RealHostID != Host.m_RealHostID)
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
