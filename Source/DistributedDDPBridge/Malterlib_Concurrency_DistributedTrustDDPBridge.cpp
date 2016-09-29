// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrustDDPBridge.h"
#include <Mib/Network/ResolveActor>
#include <Mib/Network/SSL>
#include <Mib/Network/SSLKeySetting>
#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/ActorCallbackManager>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Web/WebSocket>
#include <Mib/Web/DDPServer>

using namespace NMib::NStr;
using namespace NMib::NContainer;
using namespace NMib::NWeb;
using namespace NMib::NNet;
using namespace NMib::NFunction;
using namespace NMib::NPtr;
using namespace NMib::NEncoding;
using namespace NMib::NException;

namespace NMib::NConcurrency
{
	struct CDistributedTrustDDPBridge::CInternal
	{
		CInternal(CDistributedTrustDDPBridge *_pThis, NContainer::TCVector<NHTTP::CURL> const &_ListenAddresses, TCActor<CDistributedActorTrustManager> const &_TrustManager)
			: m_pThis{_pThis}
			, m_ListenAddresses{_ListenAddresses}
			, m_TrustManager{_TrustManager}
		{
		}
		
		struct CMethodHandlers
		{
			CStr const &f_GetMethodName() const
			{
				return TCMap<CStr, CMethodHandlers>::fs_GetKey(*this);
			}
			
			TCWeakActor<> m_Actor;
			struct CHandlerFunction
			{
				TCFunctionMutable<TCContinuation<CEJSON> (NContainer::TCVector<NEncoding::CEJSON> const &_Params)> m_fHandler;
			};
			
			TCSharedPointer<CHandlerFunction> m_pHandlerFunction;
		};
		
		struct CConnection
		{
			TCActor<CDDPServerConnection> m_DDPConnection;
			CStr m_RealHostID;
			CStr m_UniqueHostID;
			CStr m_LastExecutionID;
			CHostInfo m_HostInfo;
			CActorSubscription m_DDPSubscription;
			CStr m_AddressDesc;
		};

		void fp_TryToListen(NHTTP::CURL const &_Address, bool _bFirstTry);
		void fp_NewValidatedWebsocketConnection(TCSharedPointer<CWebSocketNewServerConnection> const &_pNewServerConnection, CStr const &_HostID);
		CEJSON fp_MethodError(CStr const &_Error, CStr const &_Reason, CStr const &_Details = "");
		void fp_StartupFailed(CException const &_Exception);
		
		CDistributedTrustDDPBridge *m_pThis;
		NContainer::TCVector<NHTTP::CURL> m_ListenAddresses;
		TCActor<CResolveActor> m_ResolveActor;
		TCActor<CWebSocketServerActor> m_WebsocketServer;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<ICActorDistributionManagerAccessHandler> m_AccessHandler;
		CActorSubscription m_ListenCallbackSubscription;
		
		TCMap<CStr, CMethodHandlers> m_MethodHandlers;
		
		mint m_LastConnectionID = 0;
		mint m_nTryListen = 0;
		bool m_bStartedUp = false;
		TCUniquePointer<CException> m_pStartupFailedException;
		TCVector<TCContinuation<void>> m_OnStartup;
		
		TCMap<mint, CConnection> m_Connections;
	};
	
	CDistributedTrustDDPBridge::CDistributedTrustDDPBridge(NContainer::TCVector<NHTTP::CURL> const &_ListenAddresses, TCActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_pInternal{fg_Construct(this, _ListenAddresses, _TrustManager)}
	{
	}
	
	CDistributedTrustDDPBridge::~CDistributedTrustDDPBridge()
	{
	}
	
	void CDistributedTrustDDPBridge::f_Construct()
	{
	}

	void CDistributedTrustDDPBridge::CInternal::fp_StartupFailed(CException const &_Exception)
	{
		m_nTryListen = 0;
		m_pStartupFailedException = fg_Construct(_Exception);
		for (auto &OnStartup : m_OnStartup)
			OnStartup.f_SetException(_Exception);
		m_OnStartup.f_Clear();
	}
	
	TCContinuation<void> CDistributedTrustDDPBridge::f_Startup()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_bStartedUp && Internal.m_nTryListen == 0)
		{
			if (Internal.m_pStartupFailedException)
				return *Internal.m_pStartupFailedException;
			return fg_Explicit();
		}
		
		auto Continuation = Internal.m_OnStartup.f_Insert();
		
		if (!Internal.m_bStartedUp)
		{
			Internal.m_bStartedUp = true;
			Internal.m_nTryListen = Internal.m_ListenAddresses.f_GetLen(); 
			Internal.m_ResolveActor = fg_ConstructActor<CResolveActor>();
			Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetAccessHandler)
				+ Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager) 
				> [this](TCAsyncResult<TCActor<ICActorDistributionManagerAccessHandler>> &&_AccessHandler, TCAsyncResult<TCActor<CActorDistributionManager>> &&_DistributionManager)
				{
					auto &Internal = *mp_pInternal;
					if (!_AccessHandler)
					{
						CStr Error = fg_Format("Failed to get access handler: {}", _AccessHandler.f_GetExceptionStr()); 
						DMibLogWithCategory
							(
								Mib/Concurrency/DistributedActorDDPBridge
								, Error
								, "{}"
								, Error
							)
						;
						Internal.fp_StartupFailed(DMibErrorInstance(Error));
						return;
					}
					if (!_DistributionManager)
					{
						CStr Error = fg_Format("Failed to get distribution manager: {}", _DistributionManager.f_GetExceptionStr()); 
						DMibLogWithCategory
							(
								Mib/Concurrency/DistributedActorDDPBridge
								, Error
								, "{}"
								, Error
							)
						;
						Internal.fp_StartupFailed(DMibErrorInstance(Error));
						return;
					}
					if (!*_AccessHandler)
					{
						CStr Error = "No access handler"; 
						DMibLogWithCategory
							(
								Mib/Concurrency/DistributedActorDDPBridge
								, Error
								, "{}"
								, Error
							)
						;
						Internal.fp_StartupFailed(DMibErrorInstance(Error));
						return;
					}
					Internal.m_AccessHandler = fg_Move(*_AccessHandler);
					Internal.m_DistributionManager = fg_Move(*_DistributionManager);
					for (auto &ListenAddress : Internal.m_ListenAddresses)
						Internal.fp_TryToListen(ListenAddress, true);
				}
			;
		}
		
		return Continuation;
	}
	
	TCContinuation<CActorSubscription> CDistributedTrustDDPBridge::f_RegisterMethods
		(
			TCWeakActor<> const &_Actor
			, NContainer::TCVector<CMethod> &&_Methods
		)
	{
		auto &Internal = *mp_pInternal;
		TCSet<CStr> MethodNames;
		for (auto &Method : _Methods)
		{
			auto pOldHandler = Internal.m_MethodHandlers.f_FindEqual(Method.m_Name);
			if (pOldHandler)
				return DMibErrorInstance(fg_Format("Handler already registered for this method name: {}", Method.m_Name));
			if (!MethodNames(Method.m_Name).f_WasCreated())
				return DMibErrorInstance(fg_Format("Duplicate method name: {}", Method.m_Name));
		}
		
		for (auto &Method : _Methods)
		{
			auto &NewHandler = Internal.m_MethodHandlers[Method.m_Name];
			NewHandler.m_pHandlerFunction = fg_Construct();
			NewHandler.m_pHandlerFunction->m_fHandler = fg_Move(Method.m_fHandler);
			NewHandler.m_Actor = _Actor;
		}
		
		auto Subscription = fg_ActorSubscription
			(
				self
				, [this, MethodNames]
				{
					auto &Internal = *mp_pInternal;
					for (auto &MethodName : MethodNames)
						Internal.m_MethodHandlers.f_Remove(MethodName);
				}
			)
		;
		
		return fg_Explicit(fg_Move(Subscription));
	}
	
	CEJSON CDistributedTrustDDPBridge::CInternal::fp_MethodError(CStr const &_Error, CStr const &_Reason, CStr const &_Details)
	{
		CEJSON Ret;
		Ret["error"] = _Error;
		if (!_Reason.f_IsEmpty())
			Ret["reason"] = _Reason;
		if (!_Details.f_IsEmpty())
			Ret["details"] = _Details;
		return Ret;
	}

	void CDistributedTrustDDPBridge::CInternal::fp_NewValidatedWebsocketConnection
		(
			TCSharedPointer<CWebSocketNewServerConnection> const &_pNewServerConnection
			, CStr const &_HostID
		)
	{
		mint ConnectionID = ++m_LastConnectionID;
		auto &NewConnection = m_Connections[ConnectionID];
		NewConnection.m_RealHostID = _HostID;
		NewConnection.m_UniqueHostID = fg_Format("DDPBridge_{}_{}", _HostID, ConnectionID);
		NewConnection.m_LastExecutionID = fg_Format("DDPBridge_{}", _HostID, ConnectionID);
		NewConnection.m_HostInfo.m_HostID = _HostID;
		NewConnection.m_HostInfo.m_FriendlyName = "NotImplemented";
		NewConnection.m_AddressDesc = _pNewServerConnection->m_Info.m_PeerAddress.f_GetString();

		NewConnection.m_DDPConnection = fg_ConstructActor<CDDPServerConnection>(fg_Move(*_pNewServerConnection), CDDPServerConnection::EConnectionType_WebSocket);
		NewConnection.m_DDPConnection
			(
				&CDDPServerConnection::f_Register
				, fg_ThisActor(m_pThis)
				, [this, ConnectionID](CDDPServerConnection::CConnectionInfo const &_ConnectionInfo) // On connection
				{
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
					{
						_ConnectionInfo.f_Reject();
						return;
					}
					_ConnectionInfo.f_Accept(CStr()); // Empty sessions means use random ID
					CStr Message = fg_Format
						(
							"Accepted DDP connection ({}) from <{}> {}"
							, ConnectionID
							, pConnection->m_HostInfo.f_GetDesc()
							, pConnection->m_AddressDesc
						)
					;
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Info, "{}", Message);
				}
				, [this, ConnectionID](CDDPServerConnection::CMethodInfo const &_MethodInfo) // On method call
				{
					auto *pMethodHandler = m_MethodHandlers.f_FindEqual(_MethodInfo.m_Name);
					if (!pMethodHandler)
					{
						_MethodInfo.f_Error(fp_MethodError("method-not-found", "Method not found"));
						return;
					}
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
					{
						_MethodInfo.f_Error(fp_MethodError("connection-not-found", "Connection not found"));
						return;
					}

					CCallingHostInfo ThisCallingHostInfo = CCallingHostInfo			
						{
							m_DistributionManager
							, pConnection->m_RealHostID 
							, pConnection->m_HostInfo
							, pConnection->m_LastExecutionID
							, 0
						}
					;
					
					auto Actor = pMethodHandler->m_Actor.f_Lock();
					
					if (!Actor)
					{
						_MethodInfo.f_Error(fp_MethodError("actor-deleted", "Actor has been deleted"));
						return;
					}
					
					fg_Dispatch
						(
							Actor
							, 
							[
								this
								, pHandlerFunction = pMethodHandler->m_pHandlerFunction
								, _MethodInfo
								, ThisActor = fg_ThisActor(m_pThis)
								, ThisCallingHostInfo = fg_Move(ThisCallingHostInfo)
							]
							() -> TCContinuation<void>
							{
								auto &CallingHostInfo = NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CallingHostInfo;
								auto OldInfo = CallingHostInfo;
								CallingHostInfo = ThisCallingHostInfo;
								auto Cleanup = g_OnScopeExit > [&]
									{
										CallingHostInfo = OldInfo; 
									}
								;

								pHandlerFunction->m_fHandler(_MethodInfo.m_Parameters) > ThisActor / [this, _MethodInfo](TCAsyncResult<CEJSON> &&_Result)
									{
										if (!_Result)
										{
											_MethodInfo.f_Error(fp_MethodError("exception", _Result.f_GetExceptionStr()));
											return;
										}
										_MethodInfo.f_Result(fg_Move(*_Result));
									}
								;
								
								return fg_Explicit();
							}
						)
						> [this, _MethodInfo](TCAsyncResult<void> &&_Result)
						{
							if (!_Result)
								_MethodInfo.f_Error(fp_MethodError("exception", _Result.f_GetExceptionStr()));
						}
					;
				}
				, [this](CDDPServerConnection::CSubscribeInfo const &_SubscribeInfo) // On subscribe
				{
					_SubscribeInfo.f_Error(CEJSON()); // Sub not found
				}
				, [this](CStr const &_SubscriptionID) // On unsubscribe
				{
				}
				, [this](CStr const &_Error) // On error
				{
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "DDP connection error: {}", _Error);
				}
				, [this, ConnectionID](EWebSocketStatus _Reason, CStr const& _Message, EWebSocketCloseOrigin _Origin)
				{
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
						return;
					
					CStr CloseMessage = fg_Format
						(
							"Lost incoming connection ({}) from <{}> {}: {} {} {}"
							, ConnectionID
							, pConnection->m_HostInfo.f_GetDesc()
							, pConnection->m_AddressDesc
							, _Origin == EWebSocketCloseOrigin_Local ? "Local" : "Remote"
							, _Reason
							, _Message
						)
					;
					
					if (_Reason == EWebSocketStatus_NormalClosure)
						DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Info, "{}", CloseMessage);
					else
						DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "{}", CloseMessage);
					m_Connections.f_Remove(ConnectionID);
				}
			)
			> [this, ConnectionID](TCAsyncResult<CActorSubscription> &&_Callback)
			{
				if (!_Callback)
				{
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "Failed to register DDP connection: {}", _Callback.f_GetExceptionStr());
					m_Connections.f_Remove(ConnectionID);
					return;
				}
				auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
				if (!pConnection)
					return;
				pConnection->m_DDPSubscription = fg_Move(*_Callback);
			}
		;
	}
	
	void CDistributedTrustDDPBridge::CInternal::fp_TryToListen(NHTTP::CURL const &_Address, bool _bFirstTry)
	{
		auto fReportListenFailure = [this, _Address, _bFirstTry](CStr const &_ErrorString, bool _bRetry = true)
			{
				DMibLogWithCategory
					(
						Mib/Concurrency/DistributedActorDDPBridge
						, Error
						, "Failed to listen to address '{}': {}"
						, _Address.f_Encode()
						, _ErrorString
					)
				;
				if (_bFirstTry)
				{
					if (--m_nTryListen == 0)
					{
						for (auto &OnStartup : m_OnStartup)
							OnStartup.f_SetResult();
						m_OnStartup.f_Clear();
					}
				}
				if (!_bRetry)
					return;
				fg_TimerActor()
					(
						&CTimerActor::f_OneshotTimer
						, 0.5
						, fg_ThisActor(m_pThis)
						, [this, _Address]() mutable
						{
							fp_TryToListen(_Address, false);
						}
					)
					> fg_DiscardResult()
				;
			}
		;
		if (!_Address.f_IsValid())
		{
			fReportListenFailure("Address is invalid", false);
			return;
		}
		CDistributedActorTrustManager_Address Address{_Address};
		m_ResolveActor(&CResolveActor::f_Resolve, _Address.f_GetHost(), ENetAddressType_None)
			+ m_TrustManager(&CDistributedActorTrustManager::f_GetCertificateData, Address)
			> [this, _Address, _bFirstTry, fReportListenFailure](TCAsyncResult<CNetAddress> &&_ReslovedAddress, TCAsyncResult<CActorDistributionListenSettings> &&_ListenSettings)
			{
				
				if (!_ReslovedAddress)
				{
					fReportListenFailure(fg_Format("Failed to resolve address: {}", _ReslovedAddress.f_GetExceptionStr()));
					return;
				}
				
				if (!_ListenSettings)
				{
					fReportListenFailure(fg_Format("Failed to get certificate data: {}", _ListenSettings.f_GetExceptionStr()));
					return;
				}
				
				NContainer::TCVector<CNetAddress> Addresses;
				{
					auto Address = fg_Move(*_ReslovedAddress);
					auto Port = _Address.f_GetPortFromScheme();
					if (Port)
						Address.f_SetPort(Port); 
					Addresses.f_Insert(fg_Move(Address));
				}
				
				CSSLSettings ServerSettings;

				auto &ListenSettings = *_ListenSettings;

				ServerSettings.m_PublicCertificateData = ListenSettings.m_PublicCertificate;
				ServerSettings.m_PrivateKeyData = ListenSettings.m_PrivateKey;
				ServerSettings.m_CACertificateData = ListenSettings.m_CACertificate;
				
				TCSharedPointer<CSSLContext> pServerContext;
				
				try
				{
					 pServerContext = fg_Construct(CSSLContext::EType_Server, ServerSettings);
				}
				catch (NException::CException const &_Exception)
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/DistributedActorDDPBridge
							, Error
							, "Error creating SSL context: {}"
							, _Exception.f_GetErrorStr()
						)
					;
					return;
				}
				
				m_WebsocketServer = fg_ConstructActor<CWebSocketServerActor>();
				m_WebsocketServer
					(
						&CWebSocketServerActor::f_StartListenAddress
						, fg_Move(Addresses)
						, ListenSettings.m_ListenFlags
						, fg_ThisActor(m_pThis)
						, [this](CWebSocketNewServerConnection &&_NewServerConnection)
						{
							TCSharedPointer<CWebSocketNewServerConnection> pNewServerConnection = fg_Construct(fg_Move(_NewServerConnection));
							
							CWebSocketNewServerConnection &NewServerConnection = *pNewServerConnection;
							
							auto fReject = [pNewServerConnection](CStr const &_Error)
								{
									DMibLogWithCategory
										(
											Mib/Concurrency/DistributedActorDDPBridge
											, Error
											, "Rejected connection: {} - {}"
											, pNewServerConnection->m_Info.m_PeerAddress.f_GetString()
											, _Error
										)
									;
									pNewServerConnection->f_Reject(_Error);
								}
							;
							
							if (!NewServerConnection.m_Info.m_pSocketInfo.f_Get())
							{
								fReject("Missing socket info");
								return;
							}
							
							auto pSocketInfo = static_cast<CSocketConnectionInfo_SSL const *>(NewServerConnection.m_Info.m_pSocketInfo.f_Get());
							CStr RealHostID;
							
							if (!pSocketInfo || pSocketInfo->m_PeerCertificate.f_IsEmpty())
							{
								fReject("Anonymous connections not supported (you need a client certificate)");
								return ;
							}
							else
							{
								try
								{
									NException::CDisableExceptionTraceScope DisableTrace;
									RealHostID = CActorDistributionManager::fs_GetCertificateHostID(pSocketInfo->m_PeerCertificate);
									if (RealHostID.f_IsEmpty())
									{
										fReject("Missing or incorrect Host ID in peer certificate");
										return;
									}
								}
								catch (NException::CException const &_Exception)
								{
									fReject("Incorrect peer certificate");
									return;
								}
							}
							
							m_AccessHandler(&ICActorDistributionManagerAccessHandler::f_ValidateClientAccess, RealHostID, pSocketInfo->m_CertificateChain) 
								> [this, fReject, pNewServerConnection, RealHostID](TCAsyncResult<CStr> &&_Result) mutable
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
									fp_NewValidatedWebsocketConnection(pNewServerConnection, RealHostID);
								}
							;
						}
						, [](CWebSocketActor::CConnectionInfo &&_ConnectionInfo)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/DistributedActorDDPBridge
									, Error
									, "Websocket rejected connection: {} - {} {}"
									, _ConnectionInfo.m_PeerAddress.f_GetString()
									, _ConnectionInfo.m_ErrorStatus
									, _ConnectionInfo.m_Error
								)
							;
						}
						, CSocket_SSL::fs_GetFactory(pServerContext)
					)
					> [this, fReportListenFailure, _Address, _bFirstTry](TCAsyncResult<CActorSubscription> &&_Result) mutable
					{
						if (!_Result)
						{
							fReportListenFailure
								(
									fg_Format("Failed to start listen on '{}': {}", _Address, _Result.f_GetExceptionStr())
								)
							;
							return;
						}
						m_ListenCallbackSubscription = fg_Move(*_Result);
						if (_bFirstTry)
						{
							if (--m_nTryListen == 0)
							{
								for (auto &OnStartup : m_OnStartup)
									OnStartup.f_SetResult();
								m_OnStartup.f_Clear();
							}
						}
					}
				;
			}
		;
	}
}
