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

		auto CDistributedActorConnectionReference::f_GetStatus() -> 
			TCActorCall
			<
				TCActor<CConcurrentActor>
				, TCContinuation<CDistributedActorConnectionStatus> 
				(CActor::*)
				(NFunction::TCFunction<TCContinuation<CDistributedActorConnectionStatus> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&)
				, NContainer::TCTuple<NFunction::TCFunction<TCContinuation<CDistributedActorConnectionStatus> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>>
				, NMeta::TCTypeList<NFunction::TCFunction<TCContinuation<CDistributedActorConnectionStatus> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>> 
			>
		{
			bool bAlreadyRemoved = mp_DistributionManager.f_IsEmpty();
			auto DistributionManager = mp_DistributionManager.f_Lock();
			mp_DistributionManager.f_Clear();
			return fg_ConcurrentActor().f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<CDistributedActorConnectionStatus>>
					, NFunction::TCFunction<TCContinuation<CDistributedActorConnectionStatus> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>
					(
						[bAlreadyRemoved, DistributionManager = fg_Move(DistributionManager), ConnectionID = fg_Move(mp_ConnectionID)]
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
				)
			;
		}
		
		auto CDistributedActorConnectionReference::f_Disconnect() 
			-> TCActorCall
			<
				TCActor<CConcurrentActor>
				, TCContinuation<void> (CActor::*)(NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&)
				, NContainer::TCTuple<NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>>
				, NMeta::TCTypeList<NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>> 
			>
		{
			bool bAlreadyRemoved = mp_DistributionManager.f_IsEmpty();
			auto DistributionManager = mp_DistributionManager.f_Lock();
			mp_DistributionManager.f_Clear();
			return fg_ConcurrentActor().f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<void>>
					, NFunction::TCFunction<TCContinuation<void> (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag>
					(
						[bAlreadyRemoved, DistributionManager = fg_Move(DistributionManager), ConnectionID = fg_Move(mp_ConnectionID)]
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
			_pConnection->m_bConnected = false;
			_pConnection->m_LastConnectionError = _ConnectionError; 
			_pConnection->m_ConnectionSubscription.f_Clear();
			if (_pConnection->m_Connection)
				_pConnection->m_Connection->f_Destroy();
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
					, _pConnection->m_PreferAddress 
					, _pConnection->m_ServerURL.f_GetPort()
					, _pConnection->m_ServerURL.f_GetFullPath()
					, _pConnection->m_ServerURL.f_Encode()
					, NContainer::fg_CreateVector<NStr::CStr>("MalterlibDistributedActors")
					, NHTTP::CRequest()
					, NNet::CSocket_SSL::fs_GetFactory(_pConnection->m_pSSLContext)
				)
				> [this, _pConnection, _pContinuation, Sequence, _bRetry](NConcurrency::TCAsyncResult<NWeb::CWebSocketNewClientConnection> &&_Result) mutable
				{
					if (!_Result)
					{
						auto Error = fg_Format("Error connecting to '{}':", _pConnection->m_ServerURL.f_Encode(), _Result.f_GetExceptionStr());
						DMibLogWithCategory(Mib/Concurrency/Actors, Error, "{}", Error);
						if (!_bRetry)
						{
							if (_pContinuation)
								_pContinuation->f_SetException(_Result);
							return;
						}
						if (_pContinuation)
							_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
						fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
						return;
					}
					if (Sequence != _pConnection->m_ConnectionSequence)
					{
						if (_pContinuation)
							_pContinuation->f_SetException(DMibErrorInstance("Connection sequence missmatch"));
						return;
					}
					
					auto &Result = *_Result;
					
					auto pSocketInfo = static_cast<NNet::CSocketConnectionInfo_SSL const *>(Result.m_pSocketInfo.f_Get());

					if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
					{
						NStr::CStr Error = "Missing peer certificate";
						if (!_bRetry)
						{
							if (_pContinuation)
								_pContinuation->f_SetException(DMibErrorInstance(Error));
							return;
						}
						if (_pContinuation)
							_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
						fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
						return;
					}
					
					NStr::CStr HostID;
					try
					{
						HostID = fs_GetCertificateHostID(pSocketInfo->m_PeerCertificate);
						if (HostID.f_IsEmpty())
						{
							NStr::CStr Error = "Missing or incorrect Host ID in server certificate";
							if (!_bRetry)
							{
								if (_pContinuation)
									_pContinuation->f_SetException(DMibErrorInstance(Error));
								return;
							}
							if (_pContinuation)
								_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
							fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
							return;
						}
					}
					catch (NException::CException const &_Exception)
					{
						NStr::CStr Error = "Incorrect peer certificate";
						if (!_bRetry)
						{
							if (_pContinuation)
								_pContinuation->f_SetException(DMibErrorInstance(Error));
							return;
						}
						if (_pContinuation)
							_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
						fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
						return;
					}
					
					auto &pHost = _pConnection->m_pHost;
					
					DMibCheck(pHost);
					
					if (pHost->m_RealHostID.f_IsEmpty())
						pHost->m_RealHostID = HostID;
					else if (pHost->m_RealHostID != HostID)
					{
						NStr::CStr Error = "Host ID mismatch";
						if (!_bRetry)
						{
							if (_pContinuation)
								_pContinuation->f_SetException(DMibErrorInstance(Error));
							return;
						}
						if (_pContinuation)
							_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));
						fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
						return;
					}

					auto &Host = *pHost;
					
					Host.m_bOutgoing = true;
					
					Result.m_fOnClose = [this, _pConnection, Sequence](NWeb::EWebSocketStatus _Reason, NStr::CStr const &_Message, NWeb::EWebSocketCloseOrigin _Origin)
						{
							NStr::CStr Error = fg_Format
								(
									"Lost connection to '{}': {} {} {}"
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
							{
								if (pHost)
								{
									pHost->m_ActiveConnections.f_Remove(*_pConnection);
								 	pHost->f_Clear(this);
									m_Hosts.f_Remove(pHost->m_UniqueHostID);
									pHost = nullptr;
								}
								
								_pConnection->m_ConnectionSubscription.f_Clear();
								if (_pConnection->m_Connection)
									_pConnection->m_Connection->f_Destroy();
								
								m_ClientConnections.f_Remove(_pConnection->m_ConnectionID);
							}
							else
								fp_ScheduleReconnect(_pConnection, nullptr, true, Sequence, Error);
						}
					;
					
					Result.m_fOnReceiveBinaryMessage = [this, _pConnection, Sequence](NPtr::TCSharedPointer<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> const &_pMessage)
						{
							if (Sequence != _pConnection->m_ConnectionSequence)
								return;
							if (!_pConnection->m_pHost)
								return;
							if (!fp_HandleProtocolIncoming(_pConnection.f_Get(), _pMessage))
							{
								// TODO: Assume malicious client
							}
						}
					;
					
					_pConnection->m_Connection = Result.f_Accept
						(
							fg_ThisActor(m_pThis) 
							/ 
							[
								this
								, _pConnection
								, _pContinuation
								, Sequence
								, _bRetry
								, HostID
								, CertificateChain = pSocketInfo->m_CertificateChain
							]
							(NConcurrency::TCAsyncResult<NConcurrency::CActorCallback> &&_Callback) mutable
							{
								if (Sequence != _pConnection->m_ConnectionSequence)
								{
									if (_pContinuation)
										_pContinuation->f_SetException(DMibErrorInstance("Connection sequence missmatch"));
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
									DMibLogWithCategory
										(
											Mib/Concurrency/Actors
											, Error
											, "{}"
											, Error
										)
									;
									if (_bRetry)
									{
										if (_pContinuation)
											_pContinuation->f_SetResult(CActorDistributionManager::CConnectionResult(fg_ThisActor(m_pThis), _pConnection->m_ConnectionID));

										fp_ScheduleReconnect(_pConnection, nullptr, _bRetry, Sequence, Error);
									}
									else if (_pContinuation)
										_pContinuation->f_SetException(_Callback);
									return;
								}

								DMibLogWithCategory(Mib/Concurrency/Actors, Info, "Connection to '{}' was successful", _pConnection->m_ServerURL.f_Encode());

								if (_pContinuation)
								{
									CActorDistributionManager::CConnectionResult ConnectionResult{fg_ThisActor(m_pThis), _pConnection->m_ConnectionID};
									ConnectionResult.m_CertificateChain = fg_Move(CertificateChain);
									ConnectionResult.m_HostID = HostID;
									_pContinuation->f_SetResult(fg_Move(ConnectionResult));
								}
								_pConnection->m_bConnected = true;
								_pConnection->m_ConnectionSubscription = fg_Move(*_Callback);

								fp_Identify(_pConnection.f_Get());
							}
						)
					; 
				}
			;
		}

		TCContinuation<CActorDistributionManager::CConnectionResult> CActorDistributionManager::f_Connect(CActorDistributionConnectionSettings const &_Settings)
		{
			auto &Internal = *mp_pInternal;
			
			bool bAnonymous = _Settings.m_PrivateClientKey.f_IsEmpty();
			
			if (_Settings.m_bRetryConnectOnFailure && bAnonymous)
				return DMibErrorInstance("Anonymous connections cannot reconnect on failure");
			
			NStr::CStr UniqueHostID;
			NStr::CStr HostID;
			NNet::CSSLSettings ClientSettings;
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
					HostID = UniqueHostID = fs_GetCertificateHostID(_Settings.m_PublicServerCertificate);
				}
				catch (NException::CException const &_Exception)
				{
					return DMibErrorInstance(fg_Format("Error getting server host ID from certificate: {}", _Exception.f_GetErrorStr()));
				}
				if (bAnonymous)
					UniqueHostID = fg_Format("Anonymous_{}_{}", HostID, NCryptography::fg_RandomID());
				
				if (HostID.f_IsEmpty())
					return DMibErrorInstance("Server certifate has no host ID");
			}
			
			auto &pHost = Internal.m_Hosts[UniqueHostID];
			if (!pHost)
			{
				pHost = fg_Construct();
				pHost->m_RealHostID = HostID;
				pHost->m_UniqueHostID = UniqueHostID;
				pHost->m_bAnonymous = bAnonymous;
			}
			else
				DMibCheck(!bAnonymous);
			
			TCContinuation<CActorDistributionManager::CConnectionResult> Continuation;
			
			if (!Internal.m_WebsocketClientConnector)
				Internal.m_WebsocketClientConnector = NConcurrency::fg_ConstructActor<NWeb::CWebSocketClientActor>();
			
			NStr::CStr ConnectionID = NCryptography::fg_RandomID();

			auto pConnection = Internal.m_ClientConnections[ConnectionID] = fg_Construct();
			
			pConnection->m_ConnectionID = ConnectionID;
			pConnection->m_pHost = pHost;
			
			if (!_Settings.m_PrivateClientKey.f_IsEmpty())
			{
				ClientSettings.m_PrivateKeyData = _Settings.m_PrivateClientKey;
				ClientSettings.m_PublicCertificateData = _Settings.m_PublicClientCertificate; 
			}
			
			try
			{
				pConnection->m_pSSLContext = fg_Construct(NNet::CSSLContext::EType_Client, ClientSettings);
			}
			catch (NException::CException const &_Exception)
			{
				return DMibErrorInstance(fg_Format("Error creating SSL context: {}", _Exception.f_GetErrorStr()));
			}
			pConnection->m_ServerURL = _Settings.m_ServerURL;
			pConnection->m_PreferAddress = _Settings.m_ServerURLPreferAddress;
			
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
			Return.m_HostID = Connection.m_pHost->m_RealHostID;
			Return.m_bConnected = Connection.m_bConnected;
			Return.m_Error = Connection.m_LastConnectionError;
			return fg_Explicit(Return);
		}

		void CActorDistributionManager::fp_RemoveConnection(NStr::CStr const &_ConnectionID)
		{
			auto &Internal = *mp_pInternal;
			auto *pConnection = Internal.m_ClientConnections.f_FindEqual(_ConnectionID);
			if (pConnection)
			{
				(*pConnection)->m_ConnectionSubscription.f_Clear();
				if ((*pConnection)->m_Connection)
					(*pConnection)->m_Connection->f_Destroy();
				Internal.m_ClientConnections.f_Remove(pConnection);
			}
		}
	}
}
