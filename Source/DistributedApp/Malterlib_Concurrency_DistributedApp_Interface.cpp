// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedAppInterface>
#include <Mib/Process/StdInActor>
#include <Mib/Concurrency/DistributedActorTrustManagerProxy>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NStr;
	using namespace NWeb::NHTTP;
	using namespace NProcess;
	using namespace NContainer;
	using namespace NCryptography;

	TCFuture<void> CDistributedAppActor::fp_WaitForAppStartup()
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_AppStartupResult.f_IsSet())
			co_return Internal.m_AppStartupResult;

		co_return co_await Internal.m_DeferredAppStartupResults.f_Insert().f_Future();
	}

	TCFuture<void> CDistributedAppActor::CDistributedAppInterfaceClientImplementation::f_GetAppStartResult()
	{
		auto pThis = m_pThis;
		auto &Internal = *pThis->mp_pInternal;

		if (Internal.m_AppStartupResult.f_IsSet())
			co_return Internal.m_AppStartupResult;

		co_return co_await Internal.m_DeferredAppStartupResults.f_Insert().f_Future();
	}

	TCFuture<void> CDistributedAppActor::CDistributedAppInterfaceClientImplementation::f_PreUpdate()
	{
		return m_pThis->fp_PreUpdate();
	}

	TCFuture<void> CDistributedAppActor::CDistributedAppInterfaceClientImplementation::f_PreStop()
	{
		return m_pThis->fp_PreStop();
	}

	TCFuture<TCActorSubscriptionWithID<>> CDistributedAppActor::CDistributedAppInterfaceClientImplementation::f_StartBackup
		(
			TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> &&_BackupInterface
			, CActorSubscription &&_ManifestFinished
			, CStr const &_BackupRoot
		)
	{
		if (!_BackupInterface)
			co_return DMibErrorInstance("Invalid backup interface");

		co_return co_await m_pThis->self(&CDistributedAppActor::fp_StartBackup, fg_Move(_BackupInterface), fg_Move(_ManifestFinished), _BackupRoot);
	}

	TCFuture<void> CDistributedAppActor::fp_PreUpdate()
	{
		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_PreStop()
	{
		co_return {};
	}

	TCFuture<CActorSubscription> CDistributedAppActor::fp_StartBackup
		(
			TCDistributedActorInterface<CDistributedAppInterfaceBackup> &&_BackupInterface
			, CActorSubscription &&_ManifestFinished
			, CStr const &_BackupRoot
		)
	{
		co_return {};
	}

	CDistributedAppActor::CLocalAppState::CLocalAppState(CDistributedAppActor_Settings const &_Settings, CDistributedAppActor &_AppActor)
		: CDistributedAppState(_Settings)
		, mp_AppActor(_AppActor)
	{
	}

	TCFuture<void> CDistributedAppState::f_SaveStateDatabase()
	{
		co_return DMibErrorInstance("Copied app state does not support this");
	}

	TCFuture<void> CDistributedAppState::f_SaveConfigDatabase()
	{
		co_return DMibErrorInstance("Copied app state does not support this");
	}

	TCFuture<void> CDistributedAppActor::CLocalAppState::f_SaveStateDatabase()
	{
		return mp_AppActor.fp_SaveStateDatabase();
	}

	TCFuture<void> CDistributedAppActor::CLocalAppState::f_SaveConfigDatabase()
	{
		return mp_AppActor.fp_SaveConfigDatabase();
	}

	TCFuture<void> CDistributedAppActor::fp_SaveStateDatabase()
	{
		auto Result = co_await mp_State.m_StateDatabase.f_Save().f_Wrap();
		if (!Result)
		{
#if (DMibSysLogSeverities) != 0
			NLog::CSysLogCatScope Scope(fg_GetSys()->f_GetLogger(), mp_Settings.m_AuditCategory);
			DMibLog(Error, "Failed to save state database: {}", Result.f_GetExceptionStr());
#endif
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppActor::fp_SaveConfigDatabase()
	{
		auto Result = co_await mp_State.m_ConfigDatabase.f_Save().f_Wrap();
		if (!Result)
		{
#if (DMibSysLogSeverities) != 0
			NLog::CSysLogCatScope Scope(fg_GetSys()->f_GetLogger(), mp_Settings.m_AuditCategory);
			DMibLog(Error, "Failed to save config database: {}", Result.f_GetExceptionStr());
#endif
		}

		co_return {};
	}

	void CDistributedAppActor::fp_PopulateAppInterfaceInfo
		(
			CDistributedAppInterfaceServer::CRegisterInfo &o_RegisterInfo
			, CDistributedAppInterfaceServer::CConfigFiles &o_ConfigFiles
			, NEncoding::CEJSON const &_Params
		)
	{
		DMibCheck(!mp_State.m_ConfigDatabase.f_GetFileName().f_IsEmpty()); // Is currently guaranteed to be initialized in constructor, make sure this doesn't change

		o_ConfigFiles.m_Files[mp_State.m_ConfigDatabase.f_GetFileName()].m_Type = CDistributedAppInterfaceServer::EMonitorConfigType_Json;
	}

	TCFuture<CActorSubscription> CDistributedAppActor::fp_RegisterForAppInterfaceServerChanges
		(
			TCActorFunctor<TCFuture<void> (TCDistributedActor<CDistributedAppInterfaceServer> const &_AppInterfaceServer, CTrustedActorInfo const &_TrustInfo)> &&_fOnChangeInterfaceServer
		)
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_bAuditLogsDestroyed)
			co_return DMibErrorInstance("Shutting down");

		CStr SubscriptionID = fg_RandomID(Internal.m_AppInterfaceServerChangeSubscriptions);

		auto &fOnChangeInterfaceServer = Internal.m_AppInterfaceServerChangeSubscriptions[SubscriptionID];
		fOnChangeInterfaceServer = fg_Move(_fOnChangeInterfaceServer);

		if (mp_State.m_AppInterfaceServer)
		{
			auto Result = co_await fOnChangeInterfaceServer(mp_State.m_AppInterfaceServer, mp_State.m_AppInterfaceServerActorInfo).f_Wrap();
			if (!Result)
				DMibLogWithCategory(Mib/Concurrency/App, Error, "Exception when reporting app interface server change");
		}

		co_return g_ActorSubscription / [this, SubscriptionID]
			{
				auto &Internal = *mp_pInternal;

				Internal.m_AppInterfaceServerChangeSubscriptions.f_Remove(SubscriptionID);
			}
		;
	}

	TCFuture<void> CDistributedAppActor::fp_SubscribeAppServerInterface(NEncoding::CEJSON _Params)
	{
		auto &Internal = *mp_pInternal;

		Internal.m_AppInterfaceClientImplementation.f_Construct(mp_State.m_DistributionManager, this);

		if (Internal.m_bDelegateTrustToAppInterface)
		{
			CDistributedActorTrustManagerProxy::CPermissions Permissions;
			Permissions.m_Permissions = CDistributedActorTrustManagerProxy::EPermission_All;
			Internal.m_AppInterfaceClientTrustProxy = mp_State.m_DistributionManager->f_ConstructActor<CDistributedActorTrustManagerProxy>(mp_State.m_TrustManager, Permissions);
		}

		CDistributedAppInterfaceServer::CRegisterInfo RegisterInfo;
		RegisterInfo.m_UpdateType = mp_Settings.m_UpdateType;
		if (mp_Settings.m_InterfaceSettings.m_LaunchID)
			RegisterInfo.m_LaunchID = mp_Settings.m_InterfaceSettings.m_LaunchID;

		CDistributedAppInterfaceServer::CConfigFiles ConfigFiles;

		fp_PopulateAppInterfaceInfo(RegisterInfo, ConfigFiles, _Params);

		Internal.m_AppInteraceServerSubscription = co_await mp_State.m_TrustManager->f_SubscribeTrustedActors<CDistributedAppInterfaceServer>();

		co_await Internal.m_AppInteraceServerSubscription.f_OnActor
			(
				g_ActorFunctor / [=, RegisterInfo = fg_Move(RegisterInfo), ConfigFiles = fg_Move(ConfigFiles)]
				(TCDistributedActor<CDistributedAppInterfaceServer> const &_NewActor, CTrustedActorInfo const &_ActorInfo) -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;

					DMibLogWithCategory(Mib/Concurrency/App, Info, "Registering with app interface server {}", _ActorInfo.m_HostInfo.f_GetDesc());

					TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> ClientInterface
						{
							Internal.m_AppInterfaceClientImplementation.m_Actor->f_ShareInterface<CDistributedAppInterfaceClient>()
						}
					;

					TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> TrustInterface;

					if (Internal.m_AppInterfaceClientTrustProxy)
						TrustInterface = Internal.m_AppInterfaceClientTrustProxy->f_ShareInterface<CDistributedActorTrustManagerInterface>();

					TCFuture<TCActorSubscriptionWithID<>> RegisterConfigFilesFuture;
					if (_NewActor->f_InterfaceVersion() >= CDistributedAppInterfaceServer::EProtocolVersion_SupportConfigFiles)
						RegisterConfigFilesFuture = _NewActor.f_CallActor(&CDistributedAppInterfaceServer::f_RegisterConfigFiles)(ConfigFiles).f_Future();
					else
						RegisterConfigFilesFuture = TCPromise<TCActorSubscriptionWithID<>>() <<= (g_ActorSubscription / []{});

					auto [Subscription, RegisterConfigSubscription] = co_await
						(
							_NewActor.f_CallActor(&CDistributedAppInterfaceServer::f_RegisterDistributedApp)(fg_Move(ClientInterface), fg_Move(TrustInterface), RegisterInfo)
							+ fg_Move(RegisterConfigFilesFuture)
						)
						.f_Wrap()
					;
					if (!Subscription)
					{
						auto ErrorStr = Subscription.f_GetExceptionStr();
						DMibLogWithCategory
							(
								Mib/Concurrency/App
								, Error
								, "Register with app interface server {} failed: {}"
								, _ActorInfo.m_HostInfo.f_GetDesc()
								, ErrorStr
							)
						;
						co_return {};
					}


					DMibLogWithCategory(Mib/Concurrency/App, Info, "Successfully registered with {}", _ActorInfo.m_HostInfo.f_GetDesc());

					Internal.m_AppInterfaceClientRegistrationSubscription = fg_Move(*Subscription);
					mp_State.m_AppInterfaceServer = _NewActor;
					mp_State.m_AppInterfaceServerActorInfo = _ActorInfo;

					if (RegisterConfigSubscription)
						Internal.m_AppInterfaceClientRegistrationConfigSubscription = fg_Move(*RegisterConfigSubscription);
					else
					{
						auto ErrorStr = RegisterConfigSubscription.f_GetExceptionStr();
						DMibLogWithCategory
							(
								Mib/Concurrency/App
								, Error
								, "Register config files with app interface server {} failed: {}"
								, _ActorInfo.m_HostInfo.f_GetDesc()
								, ErrorStr
							)
						;
					}

					TCActorResultVector<void> OnChangeResults;

					for (auto &fOnChangeInterfaceServer : Internal.m_AppInterfaceServerChangeSubscriptions)
					{
						if (fOnChangeInterfaceServer)
							fOnChangeInterfaceServer(_NewActor, _ActorInfo) > OnChangeResults.f_AddResult();
					}

					for (auto &OnChangeResult : co_await OnChangeResults.f_GetResults())
					{
						if (!OnChangeResult)
							DMibLogWithCategory(Mib/Concurrency/App, Error, "Exception when reporting app interface server change: {}", OnChangeResult.f_GetExceptionStr());
					}

					co_return {};
				}
				, nullptr
			)
		;

		co_return {};
	}

	TCFuture<CDistributedActorTrustManager::CTrustTicket> CDistributedAppActor::fp_GetTicketThroughStdIn(NStr::CStr _RequestMagic)
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_pStdInCleanup)
			co_return DMibErrorInstance("A ticket is already being requested");

		struct CState
		{
			TCActor<CStdInActor> m_StdInActor;
			CActorSubscription m_StdInSubscription;

			void f_Clear()
			{
				m_StdInSubscription.f_Clear();
				fg_Move(m_StdInActor).f_Destroy() > fg_DiscardResult();
			}
		};

		NStorage::TCSharedPointer<CState> pState = fg_Construct();

		Internal.m_pStdInCleanup = g_OnScopeExitActor / [pState]
			{
				pState->f_Clear();
			}
		;

		pState->m_StdInActor = fg_ConstructActor<CStdInActor>();

		TCPromise<CDistributedActorTrustManager::CTrustTicket> Promise;
		pState->m_StdInActor
			(
				&CStdInActor::f_RegisterForInput
				, g_ActorFunctor /
				[
					=
					, Buffer = CStrSecure{}
					, RequestMagicPrefix = _RequestMagic + ":"
				]
				(EStdInReaderOutputType _Type, const NStr::CStrSecure &_Input) mutable -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;

					if (_Type != EStdInReaderOutputType_StdIn)
						co_return {};
					Buffer += _Input;
					while (Buffer.f_FindChar('\n') >= 0)
					{
						CStrSecure Line = fg_GetStrLineSep(Buffer);
						if (Line.f_StartsWith(RequestMagicPrefix))
						{
							try
							{
								Promise.f_SetResult(CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(Line.f_Extract(RequestMagicPrefix.f_GetLen())));
							}
							catch (NException::CException const &_Exception)
							{
								Promise.f_SetException(DMibErrorInstance(fg_Format("Failed to parse trust ticket: {}", _Exception)));
							}
							pState->f_Clear();
							Internal.m_pStdInCleanup->f_Clear();
							Internal.m_pStdInCleanup.f_Clear();
						}
					}
					co_return {};
				}
				, EStdInReaderFlag_None
				, TCLimitsInt<mint>::mc_Max
			)
			> Promise / [=](CActorSubscription &&_Subscription)
			{
				DMibConErrOut("{}\n", _RequestMagic);
				pState->m_StdInSubscription = fg_Move(_Subscription);
			}
		;

		co_return co_await Promise.f_MoveFuture();
	}

	TCFuture<void> CDistributedAppActor::fp_SetupAppServerInterface(NEncoding::CEJSON _Params)
	{
		auto &Internal = *mp_pInternal;

		CStr ServerAddress = mp_Settings.m_InterfaceSettings.m_ServerAddress;
		CStr RequestTicketMagic = mp_Settings.m_InterfaceSettings.m_RequestTicketMagic;
		auto pRequestTicket = mp_Settings.m_InterfaceSettings.m_pRequestTicket;
		Internal.m_bDelegateTrustToAppInterface = (mp_Settings.m_InterfaceSettings.m_Options & CDistributedAppActor_InterfaceSettings::EOption_DelegateTrustToAppInterface) != CDistributedAppActor_InterfaceSettings::EOption_None;

		if (ServerAddress.f_IsEmpty() || (RequestTicketMagic.f_IsEmpty() && pRequestTicket.f_IsEmpty()))
			co_return co_await fp_SubscribeAppServerInterface(_Params);

		CStr CurrentServerAddress;
		if (auto pValue = mp_State.m_StateDatabase.m_Data.f_GetMember("DistributedAppInterfaceServerAddress", EJSONType_String))
			CurrentServerAddress = pValue->f_String();

		auto [bHasCurrent, bHasExpected] = co_await
			(
				mp_State.m_TrustManager
				(
					&CDistributedActorTrustManager::f_HasClientConnection
					, CURL(CurrentServerAddress.f_IsEmpty() ? ServerAddress: CurrentServerAddress)
				)
				+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasClientConnection, CURL(ServerAddress))
			)
		;

		if (bHasCurrent && ServerAddress != CurrentServerAddress && !CurrentServerAddress.f_IsEmpty())
		{
			DMibLogWithCategory(Mib/Concurrency/App, Info, "App interface address changed: {} != {}", ServerAddress, CurrentServerAddress);
			co_await
				(
					mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, CURL(CurrentServerAddress), false)
					% "Failed to remove old app interface client connection"
				)
			;
		}

		if (!bHasExpected)
		{
			TCFuture<CDistributedActorTrustManager::CTrustTicket> RequestTicketFuture;
			if (pRequestTicket)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Requesting trust ticket with provided functor");
				RequestTicketFuture = g_Future <<= (*pRequestTicket)();
			}
			else
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Requesting trust ticket from parent process");
				RequestTicketFuture = g_Future <<= self(&CDistributedAppActor::fp_GetTicketThroughStdIn, RequestTicketMagic);
			}

			auto TrustTicket = co_await (fg_Move(RequestTicketFuture).f_Timeout(60.0, "Timed out getting trust ticket through stdin") % "Failed to get ticket through stdin");

			DMibLogWithCategory(Mib/Concurrency/App, Info, "Got trust ticket, adding client connection");

			auto HostInfo = co_await (mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, fg_Move(TrustTicket), 60.0, -1) % "Failed to add client connection");
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Added client connection, trusting host '{}' for app interface namespace", HostInfo.f_GetDesc());

			co_await
				(
					mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_AllowHostsForNamespace
						, CDistributedAppInterfaceServer::mc_pDefaultNamespace
						, fg_CreateSet(HostInfo.m_HostID)
						, EDistributedActorTrustManagerOrderingFlag_None
					)
					% "Failed to allow host for namespace"
				)
			;
		}

		if (ServerAddress != CurrentServerAddress)
		{
			mp_State.m_StateDatabase.m_Data["DistributedAppInterfaceServerAddress"] = ServerAddress;
			co_await fp_SaveStateDatabase();
		}

		co_await fp_SubscribeAppServerInterface(_Params);

		co_return {};
	}
}
