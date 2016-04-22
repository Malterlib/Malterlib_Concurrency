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
		void CActorDistributionManager::CInternal::fp_ScheduleReconnect
			(
				NPtr::TCSharedPointer<CClientConnection> const &_pConnection
				, TCContinuation<void> &_Continuation
				, bool _bRetry
				, mint _Sequence
			)
		{
			_pConnection->m_Connection.f_Clear();
			_pConnection->m_ConnectionSubscription.f_Clear();
			fg_TimerActor()
				(
					&CTimerActor::f_OneshotTimer
					, 0.5
					, fg_ThisActor(m_pThis)
					, [this, _pConnection, _Sequence, _Continuation, _bRetry]() mutable
					{
						if (_Sequence != _pConnection->m_ConnectionSequence)
							return;
						fp_Reconnect(_pConnection, _Continuation, _bRetry);
					}
				)
				> fg_DiscardResult()
			;
		}

		void CActorDistributionManager::CInternal::fp_Reconnect(NPtr::TCSharedPointer<CClientConnection> const &_pConnection, TCContinuation<void> &_Continuation, bool _bRetry)
		{
			if (!_pConnection->m_pSSLContext)
				return;
			
			mint Sequence = ++_pConnection->m_ConnectionSequence;
			m_WebsocketClientConnector
				(
					&NWeb::CWebSocketClientActor::f_Connect
					, _pConnection->m_ServerURL.f_GetHost()
					, ""
					, _pConnection->m_ServerURL.f_GetPort()
					, _pConnection->m_ServerURL.f_GetFullPath()
					, _pConnection->m_ServerURL.f_Encode()
					, NContainer::fg_CreateVector<NStr::CStr>("MalterlibDistributedActors")
					, NHTTP::CRequest()
					, NNet::CSocket_SSL::fs_GetFactory(_pConnection->m_pSSLContext)
				)
				> [this, _pConnection, _Continuation, Sequence, _bRetry](NConcurrency::TCAsyncResult<NWeb::CWebSocketNewClientConnection> &&_Result) mutable
				{
					if (!_Result)
					{
						DMibLogWithCategory(Mib/Concurrency/Actors, Error, "Error connecting to '{}':", _pConnection->m_ServerURL.f_Encode(), _Result.f_GetExceptionStr());
						if (!_bRetry)
						{
							_Continuation.f_SetException(_Result);
							return;
						}
						fp_ScheduleReconnect(_pConnection, _Continuation, _bRetry, Sequence);
						return;
					}
					if (Sequence != _pConnection->m_ConnectionSequence)
					{
						_Continuation.f_SetException(DMibErrorInstance("Connection sequence missmatch"));
						return;
					}
					
					auto &Result = *_Result;
					
					auto pSocketInfo = static_cast<NNet::CSocketConnectionInfo_SSL const *>(Result.m_pSocketInfo.f_Get());

					if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
					{
						if (!_bRetry)
						{
							_Continuation.f_SetException(DMibErrorInstance("Missing peer certificate"));
							return;
						}
						fp_ScheduleReconnect(_pConnection, _Continuation, _bRetry, Sequence);
						return;
					}
					
					NStr::CStr HostID;
					try
					{
						auto Extensions = NNet::CSSLContext::fs_GetCertificateExtensions(pSocketInfo->m_PeerCertificate);
						
						auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
						if (!pHostIDExtension || pHostIDExtension->f_GetLen() != 1 || (*pHostIDExtension)[0].m_Value.f_IsEmpty())
						{
							if (!_bRetry)
							{
								_Continuation.f_SetException(DMibErrorInstance("Missing or incorrect Host ID in server certificate"));
								return;
							}
							fp_ScheduleReconnect(_pConnection, _Continuation, _bRetry, Sequence);
							return;
						}
						
						HostID = (*pHostIDExtension)[0].m_Value;
					}
					catch (NException::CException const &_Exception)
					{
						if (!_bRetry)
						{
							_Continuation.f_SetException(DMibErrorInstance("Incorrect peer certificate"));
							return;
						}
						fp_ScheduleReconnect(_pConnection, _Continuation, _bRetry, Sequence);
						return;
					}

					auto &Host = this->m_Hosts[HostID];
					_pConnection->m_pHost = &Host;
					
					Host.m_bOutgoing = true;
					
					Result.m_fOnClose = [this, _pConnection, Sequence](NWeb::EWebSocketStatus _Reason, NStr::CStr const &_Message, NWeb::EWebSocketCloseOrigin _Origin)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/Actors
									, Error
									, "Lost connection to '{}': {} {} {}"
									, _Origin == NWeb::EWebSocketCloseOrigin_Local ? "Local" : "Remote"
									, _Reason	
									, _Message
								)
							;
							TCContinuation<void> Continuation;
							fp_ScheduleReconnect(_pConnection, Continuation, true, Sequence);
						}
					;
					
					Result.m_fOnReceiveBinaryMessage = [this, _pConnection, Sequence](NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage)
						{
							if (Sequence != _pConnection->m_ConnectionSequence)
								return;
							if (!fp_HandleProtocolIncoming(_pConnection.f_Get(), _pMessage))
							{
								// TODO: Assume malicious client
							}
						}
					;
					
					
					_pConnection->m_Connection = Result.f_Accept
						(
							fg_ThisActor(m_pThis) / [this, _pConnection, _Continuation, Sequence, _bRetry, HostID](NConcurrency::TCAsyncResult<NConcurrency::CActorCallback> &&_Callback) mutable
							{
								if (Sequence != _pConnection->m_ConnectionSequence)
								{
									_Continuation.f_SetException(DMibErrorInstance("Connection sequence missmatch"));
									return;
								}
								
								if (!_Callback)
								{
									DMibLogWithCategory
										(
											Mib/Concurrency/Actors
											, Error
											, "Error accepting connection to '{}': {}"
											, _pConnection->m_ServerURL.f_Encode()
											, _Callback.f_GetExceptionStr()
										)
									;
									if (_bRetry)
										fp_ScheduleReconnect(_pConnection, _Continuation, _bRetry, Sequence);
									else
										_Continuation.f_SetException(_Callback);
									return;
								}
								
								DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Connection to '{}' was successful", _pConnection->m_ServerURL.f_Encode());
								
								_Continuation.f_SetResult();
								_pConnection->m_ConnectionSubscription = fg_Move(*_Callback);
								
								fp_Identify(_pConnection.f_Get());
							}
						)
					; 
				}
			;
		}

		TCContinuation<void> CActorDistributionManager::f_Connect(CActorDistributionConnectionSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;

			TCContinuation<void> Continuation;
			
			if (!Internal.m_WebsocketClientConnector)
				Internal.m_WebsocketClientConnector = NConcurrency::fg_ConstructActor<NWeb::CWebSocketClientActor>();

			auto pConnection = Internal.m_ClientConnections.f_Insert(fg_Construct(fg_Construct()));
			
			NNet::CSSLSettings ClientSettings;
			ClientSettings.m_VerificationFlags |= NNet::CSSLSettings::EVerificationFlag_UseSpecificPeerCertificate;
			ClientSettings.m_CACertificateData = _Settings.m_PublicServerCertificate;
			ClientSettings.m_PrivateKeyData = _Settings.m_PrivateClientCertificate;
			ClientSettings.m_PublicCertificateData = _Settings.m_PublicClientCertificate; 
			
			pConnection->m_pSSLContext = fg_Construct(NNet::CSSLContext::EType_Client, ClientSettings);
			pConnection->m_ServerURL = _Settings.m_ServerURL;
			
			Internal.fp_Reconnect(pConnection, Continuation, _Settings.m_bRetryConnectOnFailure);
			
			return Continuation;
		}
	}
}
