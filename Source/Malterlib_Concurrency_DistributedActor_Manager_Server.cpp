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
		void CActorDistributionManager::CInternal::fp_ServerConnectionClosed(NPtr::TCSharedPointer<CActorDistributionManager::CInternal::CServerConnection> const &_pConnection)
		{
		}

		void CActorDistributionManager::CInternal::fp_Listen(CActorDistributionListenSettings const &_Settings, TCContinuation<void> &_Continuation)
		{
			NNet::CSSLSettings ServerSettings;

			ServerSettings.m_PublicCertificateData = _Settings.m_PublicCertificate;
			ServerSettings.m_PrivateKeyData = _Settings.m_PrivateCertificate;
			ServerSettings.m_CACertificateData = ServerSettings.m_PublicCertificateData;

			NPtr::TCSharedPointer<NNet::CSSLContext> pServerContext = fg_Construct(NNet::CSSLContext::EType_Server, ServerSettings);
			
			m_WebsocketServer = fg_ConstructActor<NWeb::CWebSocketServerActor>();
			m_WebsocketServer
				(
					&NWeb::CWebSocketServerActor::f_StartListenAddress
					, fg_TempCopy(_Settings.m_ListenAddresses)
					, fg_ThisActor(m_pThis)
					, [this](NWeb::CWebSocketNewServerConnection &&_NewServerConnection)
					{
						NWeb::CWebSocketNewServerConnection &NewServerConnection = _NewServerConnection;
						
						auto fReject = [&](NStr::CStr const &_Error)
							{
								DMibLogWithCategory(Mib/Concurrency/Actors, Error, "Rejected connection: {} - {}", NewServerConnection.m_Info.m_PeerAddress.f_GetString(), _Error);
								NewServerConnection.f_Reject(_Error);
							}
						;
						
						if (NewServerConnection.m_Protocols.f_Contains("MalterlibDistributedActors") < 0)
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

						if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
						{
							fReject("Missing peer certificate");
							return;
						}

						if (pSocketInfo->m_PeerCertificateFingerprint.f_IsEmpty())
						{
							fReject("Missing peer fingerprint");
							return;
						}
						
						NStr::CStr ConnectionUniqueName = pSocketInfo->m_PeerCertificateFingerprint;

						auto pConnection = m_ServerConnections.f_Insert(fg_Construct(fg_Construct()));
						
						auto HostID = pSocketInfo->m_PeerCertificateFingerprint;
						auto &Host = this->m_Hosts[HostID];
						pConnection->m_pHost = &Host;

						Host.m_bIncoming = true;
						
						NewServerConnection.m_fOnClose = [this, pConnection](NWeb::EWebSocketStatus _Reason, NStr::CStr const& _Message, NWeb::EWebSocketCloseOrigin _Origin)
							{
								fp_ServerConnectionClosed(pConnection);
							}
						;
						
						NewServerConnection.m_fOnReceiveBinaryMessage = [this, pConnection](NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage)
							{
								if (!fp_HandleProtocolIncoming(pConnection.f_Get(), _pMessage))
								{
									// TODO: Assume malicious client
								}
							}
						;

						pConnection->m_Connection = NewServerConnection.f_Accept
							(
								"MalterlibDistributedActors"
								, NMib::NConcurrency::fg_ConcurrentActor() / [this, pConnection, Address = NewServerConnection.m_Info.m_PeerAddress](NConcurrency::TCAsyncResult<NConcurrency::CActorCallback> &&_Subscription)
								{
									if (_Subscription)
									{
										DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Accepted connection from '{}'", Address.f_GetString());
										pConnection->m_ConnectionSubscription = fg_Move(*_Subscription);
										fp_Identify(pConnection.f_Get());
									}
									else
									{
										DMibLogWithCategory(Mib/Concurrency/Actors, Error, "Error accepting connection from '{}': {}", Address.f_GetString(), _Subscription.f_GetExceptionStr());
									}
								}
							)
						;
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
				> [this, _Continuation, _Settings](TCAsyncResult<CActorCallback> &&_Result)
				{
					if (!_Result)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Actors
								, Error
								, "Failed to start listen: {}"
								, _Result.f_GetExceptionStr()
							)
						;
						if (_Settings.m_bRetryOnListenFailure)
						{
							fg_TimerActor()
								(
									&CTimerActor::f_OneshotTimer
									, 0.5
									, fg_ThisActor(m_pThis)
									, [this, _Continuation, _Settings]() mutable
									{
										fp_Listen(_Settings, _Continuation);
									}
								)
								> fg_DiscardResult()
							;
						}
						else
						{
							_Continuation.f_SetException(fg_Move(_Result));
						}
						return;
					}
					
					m_ListenCallbackSubscription = fg_Move(*_Result);
					_Continuation.f_SetResult();
				}
			;
		}
		
		TCContinuation<void> CActorDistributionManager::f_Listen(CActorDistributionListenSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<void> Continuation;
			
			Internal.fp_Listen(_Settings, Continuation);
			
			return Continuation;
		}
	}
}
