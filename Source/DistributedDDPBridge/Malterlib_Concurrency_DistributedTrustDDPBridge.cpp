// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrustDDPBridge.h"
#include <Mib/Network/ResolveActor>
#include <Mib/Network/SSL>
#include <Mib/Network/Sockets/SSL>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Web/WebSocket>
#include <Mib/Web/DDPServer>

using namespace NMib::NStr;
using namespace NMib::NContainer;
using namespace NMib::NWeb;
using namespace NMib::NNetwork;
using namespace NMib::NFunction;
using namespace NMib::NStorage;
using namespace NMib::NEncoding;
using namespace NMib::NException;

namespace NMib::NConcurrency
{
	struct CDistributedTrustDDPBridge::CInternal : public CActorInternal
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
			
			TCActorFunctor<TCFuture<CEJsonSorted> (NContainer::TCVector<NEncoding::CEJsonSorted> _Params)> m_fHandlerFunction;
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

		void fp_NewValidatedWebsocketConnection(TCSharedPointer<CWebSocketNewServerConnection> const &_pNewServerConnection, CStr const &_HostID);
		CEJsonSorted fp_MethodError(CStr _Error, CStr _Reason, CStr _Details = "");
		void fp_StartupFailed(CException const &_Exception);
		
		CDistributedTrustDDPBridge *m_pThis;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		CActorSubscription m_WebsocketHandlerSubscription;
		
		TCMap<CStr, CMethodHandlers> m_MethodHandlers;
		
		mint m_LastConnectionID = 0;
		bool m_bStartedUp = false;
		TCUniquePointer<CException> m_pStartupFailedException;
		TCVector<TCPromise<void>> m_OnStartup;
		
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
	
	TCFuture<void> CDistributedTrustDDPBridge::f_Startup()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_bStartedUp)
		{
			if (Internal.m_pStartupFailedException)
				co_return Internal.m_pStartupFailedException->f_ExceptionPointer();
			co_return {};
		}
		
		auto Promise = Internal.m_OnStartup.f_Insert();
		Internal.m_bStartedUp = true;
		Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager) > [this](TCAsyncResult<TCActor<CActorDistributionManager>> &&_DistributionManager)
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
						, [this](NStorage::TCSharedPointer<NWeb::CWebSocketNewServerConnection> _pServerConnection, NStr::CStr _HostID) -> TCFuture<void>
						{
							auto &Internal = *mp_pInternal;
							Internal.fp_NewValidatedWebsocketConnection(_pServerConnection, _HostID);
							co_return {};
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

		co_return co_await Promise.f_MoveFuture();
	}
	
	TCFuture<CActorSubscription> CDistributedTrustDDPBridge::f_RegisterMethods(NContainer::TCVector<CMethod> _Methods)
	{
		auto &Internal = *mp_pInternal;
		TCSet<CStr> MethodNames;
		for (auto &Method : _Methods)
		{
			auto pOldHandler = Internal.m_MethodHandlers.f_FindEqual(Method.m_Name);
			if (pOldHandler)
				co_return DMibErrorInstance(fg_Format("Handler already registered for this method name: {}", Method.m_Name));
			if (!MethodNames(Method.m_Name).f_WasCreated())
				co_return DMibErrorInstance(fg_Format("Duplicate method name: {}", Method.m_Name));
		}
		
		for (auto &Method : _Methods)
		{
			auto &NewHandler = Internal.m_MethodHandlers[Method.m_Name];
			NewHandler.m_fHandlerFunction = fg_Move(Method.m_fHandler);
		}
		
		auto Subscription = g_ActorSubscription / [this, MethodNames]
			{
				auto &Internal = *mp_pInternal;
				for (auto &MethodName : MethodNames)
					Internal.m_MethodHandlers.f_Remove(MethodName);
			}
		;
		
		co_return fg_Move(Subscription);
	}
	
	CEJsonSorted CDistributedTrustDDPBridge::CInternal::fp_MethodError(CStr _Error, CStr _Reason, CStr _Details)
	{
		CEJsonSorted Ret;
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
		
		NewConnection.m_AddressDesc = NewServerConnection.m_Info.m_PeerAddress.f_GetString(ENetAddressStringFlag_IncludePort);

		NewConnection.m_DDPConnection = fg_ConstructActor<CDDPServerConnection>(fg_Move(NewServerConnection), CDDPServerConnection::EConnectionType_WebSocket);
		NewConnection.m_DDPConnection
			(
				&CDDPServerConnection::f_Register
				, g_ActorFunctorWeak / [this, ConnectionID](CDDPServerConnection::CConnectionInfo _ConnectionInfo) -> TCFuture<void>
				// On connection
				{
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
					{
						_ConnectionInfo.f_Reject();
						co_return {};
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

					co_return {};
				}
				, g_ActorFunctorWeak / [this, ConnectionID](CDDPServerConnection::CMethodInfo _MethodInfo) -> TCFuture<void>
				// On method call
				{
					auto *pMethodHandler = m_MethodHandlers.f_FindEqual(_MethodInfo.m_Name);
					if (!pMethodHandler)
					{
						_MethodInfo.f_Error(fp_MethodError("method-not-found", "Method not found"));
						DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "Unknown method call: {}", _MethodInfo.m_Name);
						co_return {};
					}
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
					{
						_MethodInfo.f_Error(fp_MethodError("connection-not-found", "Connection not found"));
						co_return {};
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
							, ""
							, nullptr
						}
					;
					
					pMethodHandler->m_fHandlerFunction.f_CallWrapped
						(
							[ThisCallingHostInfo = fg_Move(ThisCallingHostInfo)] mark_no_coroutine_debug (auto &&_fToDispatch) mutable -> TCFuture<CEJsonSorted>
							{
								CCallingHostInfoScope Scope(fg_Move(ThisCallingHostInfo));
								return _fToDispatch();
							}
							, _MethodInfo.m_Parameters
						)
						> [this, _MethodInfo](TCAsyncResult<CEJsonSorted> &&_Result)
						{
							if (!_Result)
							{
								_MethodInfo.f_Error(fp_MethodError("exception", _Result.f_GetExceptionStr()));
								return;
							}
							_MethodInfo.f_Result(fg_Move(*_Result));
						}
					;
					co_return {};
				}
				, g_ActorFunctorWeak / [](CDDPServerConnection::CSubscribeInfo _SubscribeInfo) -> TCFuture<void>
				// On subscribe
				{
					_SubscribeInfo.f_Error(CEJsonSorted()); // Sub not found
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Info, "DDP subscription {}", _SubscribeInfo.m_Name);
					co_return {};
				}
				, g_ActorFunctorWeak / [](CStr _SubscriptionID) -> TCFuture<void>
				// On unsubscribe
				{
					co_return {};
				}
				, g_ActorFunctorWeak / [](CStr _Error) -> TCFuture<void>
				// On error
				{
					DMibLogWithCategory(Mib/Concurrency/DistributedActorDDPBridge, Error, "DDP connection error: {}", _Error);
					co_return {};
				}
				, g_ActorFunctorWeak / [this, ConnectionID](EWebSocketStatus _Reason, CStr _Message, EWebSocketCloseOrigin _Origin) -> TCFuture<void>
				{
					auto *pConnection = m_Connections.f_FindEqual(ConnectionID);
					if (!pConnection)
						co_return {};

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
					co_return {};
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
