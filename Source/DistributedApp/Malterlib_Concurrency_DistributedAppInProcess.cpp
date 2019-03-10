// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/ActorFunctor>

#include "Malterlib_Concurrency_DistributedAppInProcess.h"

namespace NMib::NConcurrency
{
	CDistributedAppInProcessActor::CDistributedAppInProcessActor
		(
			NWeb::NHTTP::CURL const &_Address
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, FOnUseTicket &&_fOnUseTicket
			, NStr::CStr const &_Description
			, bool _bDelegateTrust
		)
		: mp_Address(_Address)
		, mp_TrustManager(_TrustManager)
		, mp_fOnUseTicket(fg_Move(_fOnUseTicket))
		, mp_Description(_Description)
		, mp_bDelegateTrust(_bDelegateTrust)
	{
	}
	
	CDistributedAppInProcessActor::~CDistributedAppInProcessActor()
	{
	}

	TCFuture<NStr::CStr> CDistributedAppInProcessActor::f_Launch
		(
			NStr::CStr const &_HomeDirectory
			, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory
			, NContainer::TCVector<NStr::CStr> &&_Params
		)
	{
		auto &ThreadLocal = fg_DistributedAppThreadLocal();
		auto OldSettings = ThreadLocal.m_DefaultSettings;
		auto OldAppType = ThreadLocal.m_DefaultAppType;
		auto Cleanup = g_OnScopeExit > [&]
			{
				ThreadLocal.m_DefaultSettings = OldSettings;
				ThreadLocal.m_DefaultAppType = OldAppType;
			}
		;

		ThreadLocal.m_DefaultAppType = EDistributedAppType_InProcess;
		ThreadLocal.m_DefaultSettings.m_bSeparateDistributionManager = true;
		ThreadLocal.m_DefaultSettings.m_RootDirectory = _HomeDirectory;

		auto &InterfaceSettings = ThreadLocal.m_DefaultSettings.m_InterfaceSettings;
		if (mp_bDelegateTrust)
			InterfaceSettings.m_Options = CDistributedAppActor_InterfaceSettings::EOption_DelegateTrustToAppInterface;
		else
			InterfaceSettings.m_Options = CDistributedAppActor_InterfaceSettings::EOption_None;
		InterfaceSettings.m_ServerAddress = mp_Address.f_Encode();
		*(InterfaceSettings.m_pRequestTicket = fg_Construct()) = g_ActorFunctor / [this]() -> TCFuture<CDistributedActorTrustManager::CTrustTicket>
			{
				return fp_HandleTicketRequest();
			}
		;

		mp_DistributedApp = _fDistributedAppFactory();

		TCPromise<NStr::CStr> Promise;

		CDistributedAppCommandLineClient CommandLineClient = co_await mp_DistributedApp(&CDistributedAppActor::f_GetCommandLineClient);

		NContainer::TCVector<NStr::CStr> Params{"--daemon-run-standalone"};

		Params.f_Insert(fg_Move(_Params));

		CDistributedAppCommandLineSpecification::CParsedCommandLine CommandLine;
		try
		{
			CommandLine = CommandLineClient.f_ParseCommandLine(Params);
		}
		catch (NException::CException const &_Exception)
		{
			co_return _Exception;
		}

		co_return co_await mp_DistributedApp(&CDistributedAppActor::f_StartApp, CommandLine.m_Params, nullptr, EDistributedAppType_InProcess);
	}

	TCFuture<void> CDistributedAppInProcessActor::fp_Destroy()
	{
		TCPromise<void> Promise;
		if (mp_DistributedApp)
		{
			mp_DistributedApp(&CDistributedAppActor::f_StopApp) > [Promise, this](auto &&)
				{
					mp_DistributedApp->f_Destroy() > Promise;
					mp_DistributedApp.f_Clear();
				}
			;
		}
		else
			Promise.f_SetResult();
		return Promise.f_MoveFuture();
	}

	TCFuture<CDistributedActorTrustManager::CTrustTicket> CDistributedAppInProcessActor::fp_HandleTicketRequest()
	{
		DMibLogWithCategory(Malterlib/Concurrency, Info, "Generating ticket for '{}'", mp_Description);
		
		NStr::CStr HandleRequestID = NCryptography::fg_RandomID();

		TCPromise<CDistributedActorTrustManager::CTrustTicket> Promise;
		
		mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, g_ActorFunctor
				(
					g_ActorSubscription / [this, HandleRequestID]() -> TCFuture<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							return fg_Explicit();
						
						TCFuture<void> DestroyFuture;
						if (pHandleRequest->m_NotificationsSubscription)
							DestroyFuture = pHandleRequest->m_NotificationsSubscription->f_Destroy();
						else
							DestroyFuture = fg_Explicit();
						
						mp_HandleRequests.f_Remove(HandleRequestID);
						
						return DestroyFuture;
					}
				)
				/ [this, HandleRequestID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCFuture<void>
				{
					TCPromise<void> Promise;
					mp_fOnUseTicket(_HostID, _HostInfo, _CertificateRequest) > [this, HandleRequestID, Promise](TCAsyncResult<void> &&_Result)
						{
							mp_HandleRequests.f_Remove(HandleRequestID);
							Promise.f_SetResult(fg_Move(_Result));
						}
					;
					return Promise.f_MoveFuture();
				}
			 	, nullptr
			)
			> [this, HandleRequestID, Promise](TCAsyncResult<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> &&_Ticket)
			{
				if (!_Ticket)
				{
					DMibLogWithCategory
						(
							Malterlib/Concurrency
							, Error
							, "Failed to generate ticket for '{}': {}"
							, mp_Description
							, _Ticket.f_GetExceptionStr()
						)
					;
					Promise.f_SetException(_Ticket);
					return;
				}

				auto &Request = mp_HandleRequests[HandleRequestID];
				Request.m_NotificationsSubscription = fg_Move(_Ticket->m_NotificationsSubscription);
				DMibLogWithCategory(Malterlib/Concurrency, Info, "Sending ticket to '{}'", mp_Description);
				Promise.f_SetResult(fg_Move(_Ticket->m_Ticket));
			}
		;

		return Promise.f_MoveFuture();
	}

#if DMibConfig_Tests_Enable
	TCFuture<NEncoding::CEJSON> CDistributedAppInProcessActor::f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		if (!mp_DistributedApp)
			DMibError("No distributed app");
		
		return mp_DistributedApp(&CDistributedAppActor::f_Test_Command, _Command, _Params);
	}

	TCFuture<uint32> CDistributedAppInProcessActor::f_RunCommandLine
		(
			CCallingHostInfo const &_CallingHost
			, NStr::CStr const &_Command
			, NEncoding::CEJSON const &_Params
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		if (!mp_DistributedApp)
			DMibError("No distributed app");

		return mp_DistributedApp(&CDistributedAppActor::f_RunCommandLine, _CallingHost, _Command, _Params, _pCommandLine);
	}
#endif
}
