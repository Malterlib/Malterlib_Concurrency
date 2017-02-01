// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Process/StdInActor>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/DistributedActorTrustManagerProxy>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NStr;
	using namespace NHTTP;
	using namespace NProcess;
	using namespace NContainer;
	
	struct CDistributedAppActor::CDistributedAppInterfaceClientImplementation : public CDistributedAppInterfaceClient 
	{
		CDistributedAppInterfaceClientImplementation(TCActor<CDistributedAppActor> const &_AppActor)
			: mp_AppActor(_AppActor)
		{
		}
		
		NConcurrency::TCContinuation<void> f_PreUpdate() override
		{
			return mp_AppActor(&CDistributedAppActor::fp_PreUpdate);
		}
		
	private:
		TCActor<CDistributedAppActor> mp_AppActor;
	};
	
	TCContinuation<void> CDistributedAppActor::fp_PreUpdate()
	{
		return fg_Explicit();
	}
	
	TCContinuation<void> CDistributedAppActor::fp_SubscribeAppServerInterface()
	{
		mp_AppInterfaceClientImplementation = mp_State.m_DistributionManager->f_ConstructActor<CDistributedAppInterfaceClientImplementation>(fg_ThisActor(this));
		
		if (mp_bDelegateTrustToAppInterface)
		{
			CDistributedActorTrustManagerProxy::CPermissions Permissions;
			Permissions.m_Permissions = CDistributedActorTrustManagerProxy::EPermission_All;
			mp_AppInterfaceClientTrustProxy = mp_State.m_DistributionManager->f_ConstructActor<CDistributedActorTrustManagerProxy>(mp_State.m_TrustManager, Permissions);
		}
		
		TCContinuation<void> Continuation;
		mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_SubscribeTrustedActors<CDistributedAppInterfaceServer>
				, CDistributedAppInterfaceServer::mc_pDefaultNamespace
				, fg_ThisActor(this)
			)
			> Continuation / [=](TCTrustedActorSubscription<CDistributedAppInterfaceServer> &&_Subscription)
			{
				mp_AppInteraceServerSubscription = fg_Move(_Subscription);
				mp_AppInteraceServerSubscription.f_OnActor
					(
						[=](TCDistributedActor<CDistributedAppInterfaceServer> const &_NewActor, CTrustedActorInfo const &_ActorInfo)
						{
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Registering with app interface server {}", _ActorInfo.m_HostInfo.f_GetDesc());
							
							TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> ClientInterface
								{
									mp_AppInterfaceClientImplementation->f_ShareInterface<CDistributedAppInterfaceClient>()
								}
							;
							
							TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> TrustInterface;
							
							if (mp_AppInterfaceClientTrustProxy)
								TrustInterface = mp_AppInterfaceClientTrustProxy->f_ShareInterface<CDistributedActorTrustManagerInterface>();
							
							DMibCallActor(_NewActor, CDistributedAppInterfaceServer::f_RegisterDistributedApp, fg_Move(ClientInterface), fg_Move(TrustInterface), mp_Settings.m_UpdateType) 
								> [=, HostInfo = _ActorInfo.m_HostInfo](TCAsyncResult<NConcurrency::TCActorSubscriptionWithID<>> &&_Subscription)
								{
									if (!_Subscription)
									{
										DMibLogWithCategory
											(
												Mib/Concurrency/App
												, Error
												, "Register with app interface server {} failed: {}"
												, HostInfo.f_GetDesc()
												, _Subscription.f_GetExceptionStr()
											)
										;
										return;
									}
									DMibLogWithCategory(Mib/Concurrency/App, Info, "Successfully registered with {}", HostInfo.f_GetDesc());
									mp_AppInterfaceClientRegistrationSubscription = fg_Move(*_Subscription);
								}
							;
						}
					)
				;
				Continuation.f_SetResult();
			}
		;
		return Continuation;
	}
	
	TCContinuation<CDistributedActorTrustManager::CTrustTicket> CDistributedAppActor::fp_GetTicketThroughStdIn(NStr::CStr const &_RequestMagic)
	{
		if (mp_pStdInCleanup)
			return DMibErrorInstance("A ticket is already being requested");
		
		TCContinuation<CDistributedActorTrustManager::CTrustTicket> Continuation;
		
		struct CState
		{
			TCActor<CStdInActor> m_StdInActor;
			CActorSubscription m_StdInSubscription;
			
			void f_Clear()
			{
				m_StdInSubscription.f_Clear();
				m_StdInActor->f_Destroy2() > fg_DiscardResult();
				m_StdInActor.f_Clear();
			}
		};
		
		NPtr::TCSharedPointer<CState> pState = fg_Construct();
		
		mp_pStdInCleanup = g_OnScopeExitActor > [pState]
			{
				pState->f_Clear();
			}
		;
		
		pState->m_StdInActor = fg_ConstructActor<CStdInActor>();
		
		
		pState->m_StdInActor
			(
				&CStdInActor::f_RegisterForInput
				, g_ActorFunctor > 
				[
					=
					, Buffer = CStr{}
					, RequestMagicPrefix = _RequestMagic + ":"
				]
				(EStdInReaderOutputType _Type, const NStr::CStr &_Input) mutable -> NConcurrency::TCContinuation<void>
				{
					if (_Type != EStdInReaderOutputType_StdIn)
						return fg_Explicit();
					Buffer += _Input;
					while (Buffer.f_FindChar('\n') >= 0)
					{
						CStr Line = fg_GetStrLineSep(Buffer);
						if (Line.f_StartsWith(RequestMagicPrefix))
						{
							try
							{
								Continuation.f_SetResult(CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(Line.f_Extract(RequestMagicPrefix.f_GetLen())));
							}
							catch (NException::CException const &_Exception)
							{
								Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to parse trust ticket: {}", _Exception)));
							}
							pState->f_Clear();
							mp_pStdInCleanup->f_Clear();
							mp_pStdInCleanup.f_Clear();
						}
					}
					return fg_Explicit();
				}
				, EStdInReaderFlag_None
			)
			> Continuation / [=](CActorSubscription &&_Subscription)
			{
				DMibConErrOut("{}\n", _RequestMagic);
				pState->m_StdInSubscription = fg_Move(_Subscription);
			}
		;
		
		return Continuation;
	}
	
	TCContinuation<void> CDistributedAppActor::fp_SetupAppServerInterface()
	{
		CStr ServerAddress = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerAddress");
		CStr RequestTicketMagic = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerRequestTicket");
		CStr Options = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerOptions");
		
		while (!Options.f_IsEmpty())
		{
			CStr Option = fg_GetStrSep(Options, ";");
			if (Option == "DelegateTrust")
				mp_bDelegateTrustToAppInterface = true;
		}
		
		if (ServerAddress.f_IsEmpty() || RequestTicketMagic.f_IsEmpty())
			return fp_SubscribeAppServerInterface();

		TCContinuation<void> Continuation;
		
		CStr CurrentServerAddress;
		if (auto pValue = mp_State.m_StateDatabase.m_Data.f_GetMember("DistributedAppInterfaceServerAddress", EJSONType_String))
			CurrentServerAddress = pValue->f_String();
		
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClientConnection, CURL(CurrentServerAddress.f_IsEmpty() ? ServerAddress: CurrentServerAddress))
			+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClientConnection, CURL(ServerAddress))
			> Continuation / [=](bool _bHasCurrent, bool _bHasExpected)
			{
				TCActorResultVector<void> ClientConnectionChanges;
				if (_bHasCurrent && ServerAddress != CurrentServerAddress && !CurrentServerAddress.f_IsEmpty())
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "App interface address changed: {} != {}", ServerAddress, CurrentServerAddress);
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, CURL(CurrentServerAddress)) > ClientConnectionChanges.f_AddResult();
				}
				
				ClientConnectionChanges.f_GetResults() > Continuation / [=](TCVector<TCAsyncResult<void>> &&_Results)
					{
						for (auto &Result : _Results)
						{
							if (!Result)
							{
								Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to remove old app interface client connection: {}\n", Result.f_GetExceptionStr())));
								return;
							}
						}
						
						auto fSaveAndSubscribe = [=]
							{
								if (ServerAddress != CurrentServerAddress)
								{
									mp_State.m_StateDatabase.m_Data["DistributedAppInterfaceServerAddress"] = ServerAddress;
									mp_State.m_StateDatabase.f_Save() > Continuation / [=]
										{
											fp_SubscribeAppServerInterface() > Continuation;
										}
									;
								}
								else
									fp_SubscribeAppServerInterface() > Continuation;
							}
						;

						if (_bHasExpected)
							return fSaveAndSubscribe();
						
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Requesting trust ticket from parent process");
						self(&CDistributedAppActor::fp_GetTicketThroughStdIn, RequestTicketMagic)
							.f_Timeout(60.0, "Timed out getting trust ticket through stdin") 
							> Continuation % "Failed to get ticket through stdin" / [=](CDistributedActorTrustManager::CTrustTicket &&_TrustTicket)
							{
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Got trust ticket, adding client connection");
								mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, fg_Move(_TrustTicket), 60.0)
									> Continuation % "Failed to add client connection" / [=](CHostInfo &&_HostInfo)
									{
										DMibLogWithCategory(Mib/Concurrency/App, Info, "Added client connection, trusting host '{}' for app interface namespace", _HostInfo.f_GetDesc());
								
										mp_State.m_TrustManager
											(
												&CDistributedActorTrustManager::f_AllowHostsForNamespace
												, CDistributedAppInterfaceServer::mc_pDefaultNamespace
												, fg_CreateSet(_HostInfo.m_HostID)
											)
											> Continuation % "Failed to allow host for namespace" / fSaveAndSubscribe
										;
									}
								;
							}
						;
					}
				;
			}
		;
		
		return Continuation;
	}
}
