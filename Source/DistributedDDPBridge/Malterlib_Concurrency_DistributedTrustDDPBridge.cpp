// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrustDDPBridge.h"
#include <Mib/Network/ResolveActor>
#include <Mib/Network/SSL>
#include <Mib/Network/SSLKeySetting>
#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/ActorSubscription>
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
		CInternal(CDistributedTrustDDPBridge *_pThis, TCActor<CDistributedActorTrustManager> const &_TrustManager)
			: m_pThis{_pThis}
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
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		CActorSubscription m_WebsocketHandlerSubscription;
		
		TCMap<CStr, CMethodHandlers> m_MethodHandlers;
		
		mint m_LastConnectionID = 0;
		bool m_bStartedUp = false;
		TCUniquePointer<CException> m_pStartupFailedException;
		TCVector<TCContinuation<void>> m_OnStartup;
		
		TCMap<mint, CConnection> m_Connections;
	};
	
	CDistributedTrustDDPBridge::CDistributedTrustDDPBridge(TCActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_pInternal{fg_Construct(this, _TrustManager)}
	{
	}
	
	CDistributedTrustDDPBridge::~CDistributedTrustDDPBridge()
	{
	}
	
	void CDistributedTrustDDPBridge::CInternal::fp_StartupFailed(CException const &_Exception)
	{
		m_pStartupFailedException = fg_Construct(_Exception);
		for (auto &OnStartup : m_OnStartup)
			OnStartup.f_SetException(_Exception);
		m_OnStartup.f_Clear();
	}
	
	TCContinuation<void> CDistributedTrustDDPBridge::f_Startup()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_bStartedUp)
		{
			if (Internal.m_pStartupFailedException)
				return *Internal.m_pStartupFailedException;
			return fg_Explicit();
		}
		
		auto Continuation = Internal.m_OnStartup.f_Insert();
		
		if (!Internal.m_bStartedUp)
		{
			Internal.m_bStartedUp = true;
			Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager) 
				> [this](TCAsyncResult<TCActor<CActorDistributionManager>> &&_DistributionManager)
				{
					auto &Internal = *mp_pInternal;
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
					Internal.m_DistributionManager = fg_Move(*_DistributionManager);
					Internal.m_DistributionManager
						(
							&CActorDistributionManager::f_RegisterWebsocketHandler
							, "/ActorDDPBridge"
							, fg_ThisActor(this)
							, [this](NPtr::TCSharedPointer<NWeb::CWebSocketNewServerConnection> const &_pServerConnection, NStr::CStr const &_HostID) -> TCContinuation<void>
							{
								auto &Internal = *mp_pInternal;
								Internal.fp_NewValidatedWebsocketConnection(_pServerConnection, _HostID);
								return fg_Explicit();
							}
						)
						> [this](TCAsyncResult<CActorSubscription> &&_Result)
						{
							auto &Internal = *mp_pInternal;
							if (!_Result)
							{
								CStr Error = fg_Format("Failed to register websocket handler: {}", _Result.f_GetExceptionStr()); 
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
							Internal.m_WebsocketHandlerSubscription = fg_Move(*_Result);
							for (auto &OnStartup : Internal.m_OnStartup)
								OnStartup.f_SetResult();
							Internal.m_OnStartup.f_Clear();
						}
					;
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
		
		auto Subscription = g_ActorSubscription > [this, MethodNames]
			{
				auto &Internal = *mp_pInternal;
				for (auto &MethodName : MethodNames)
					Internal.m_MethodHandlers.f_Remove(MethodName);
			}
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
		
		auto &NewServerConnection = *_pNewServerConnection;
		
		NewConnection.m_HostInfo.m_FriendlyName = "Unknown";
		if (NewServerConnection.m_Info.m_pRequest) {
			if (auto *pValue = NewServerConnection.m_Info.m_pRequest->f_GetEntityFields().f_GetUnknownField("MalterlibDDPBridge_FriendlyName"))
				NewConnection.m_HostInfo.m_FriendlyName = *pValue;
		}
		
		NewConnection.m_AddressDesc = NewServerConnection.m_Info.m_PeerAddress.f_GetString();

		NewConnection.m_DDPConnection = fg_ConstructActor<CDDPServerConnection>(fg_Move(NewServerConnection), CDDPServerConnection::EConnectionType_WebSocket);
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
				, [pThis = m_pThis, this, ConnectionID](CDDPServerConnection::CMethodInfo const &_MethodInfo) // On method call
				{
					auto *pMethodHandler = m_MethodHandlers.f_FindEqual(_MethodInfo.m_Name);
					if (!pMethodHandler)
					{
						_MethodInfo.f_Error(fp_MethodError("method-not-found", "Method not found"));
						DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "Unknown method call: {}", _MethodInfo.m_Name);
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
							, nullptr // TODO: Implement authentication
							, pConnection->m_RealHostID 
							, pConnection->m_HostInfo
							, pConnection->m_LastExecutionID
							, 0
							, ""
							, nullptr
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
								, ThisActor = fg_ThisActor(pThis)
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
				, [](CDDPServerConnection::CSubscribeInfo const &_SubscribeInfo) // On subscribe
				{
					_SubscribeInfo.f_Error(CEJSON()); // Sub not found
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Info, "DDP subscription {}", _SubscribeInfo.m_Name);
				}
				, [](CStr const &_SubscriptionID) // On unsubscribe
				{
				}
				, [](CStr const &_Error) // On error
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
}
