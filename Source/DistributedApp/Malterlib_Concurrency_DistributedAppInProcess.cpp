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

	TCContinuation<NStr::CStr> CDistributedAppInProcessActor::f_Launch(NStr::CStr const &_HomeDirectory, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory)
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
		*(InterfaceSettings.m_pRequestTicket = fg_Construct()) = g_ActorFunctor / [this]() -> TCContinuation<CDistributedActorTrustManager::CTrustTicket>
			{
				return fp_HandleTicketRequest();
			}
		;

		mp_DistributedApp = _fDistributedAppFactory();

		TCContinuation<NStr::CStr> Continuation;

		mp_DistributedApp(&CDistributedAppActor::f_StartApp, NEncoding::CEJSON{}, nullptr, EDistributedAppType_InProcess) > Continuation;

		return Continuation;
	}

	TCContinuation<void> CDistributedAppInProcessActor::fp_Destroy()
	{
		TCContinuation<void> Continuation;
		if (mp_DistributedApp)
		{
			mp_DistributedApp(&CDistributedAppActor::f_StopApp) > [Continuation, this](auto &&)
				{
					mp_DistributedApp->f_Destroy() > Continuation;
					mp_DistributedApp.f_Clear();
				}
			;
		}
		else
			Continuation.f_SetResult();
		return Continuation;
	}

	TCContinuation<CDistributedActorTrustManager::CTrustTicket> CDistributedAppInProcessActor::fp_HandleTicketRequest()
	{
		DMibLogWithCategory(Malterlib/Concurrency, Info, "Generating ticket for '{}'", mp_Description);
		
		NStr::CStr HandleRequestID = NCryptography::fg_RandomID();

		TCContinuation<CDistributedActorTrustManager::CTrustTicket> Continuation;
		
		mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, g_ActorFunctor
				(
					g_ActorSubscription / [this, HandleRequestID]() -> TCContinuation<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							return fg_Explicit();
						
						TCContinuation<void> Continuation;
						if (pHandleRequest->m_NotificationsSubscription)
							Continuation = pHandleRequest->m_NotificationsSubscription->f_Destroy();
						else
							Continuation.f_SetResult();
						
						mp_HandleRequests.f_Remove(HandleRequestID);
						
						return Continuation;
					}
				)
				/ [this, HandleRequestID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCContinuation<void>
				{
					TCContinuation<void> Continuation;
					mp_fOnUseTicket(_HostID, _HostInfo, _CertificateRequest) > [this, HandleRequestID, Continuation](TCAsyncResult<void> &&_Result)
						{
							mp_HandleRequests.f_Remove(HandleRequestID);
							Continuation.f_SetResult(fg_Move(_Result));
						}
					;
					return Continuation;
				}
			 	, nullptr
			)
			> [this, HandleRequestID, Continuation](TCAsyncResult<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> &&_Ticket)
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
					Continuation.f_SetException(_Ticket);
					return;
				}

				auto &Request = mp_HandleRequests[HandleRequestID];
				Request.m_NotificationsSubscription = fg_Move(_Ticket->m_NotificationsSubscription);
				DMibLogWithCategory(Malterlib/Concurrency, Info, "Sending ticket to '{}'", mp_Description);
				Continuation.f_SetResult(fg_Move(_Ticket->m_Ticket));
			}
		;

		return Continuation;
	}

#if DMibConfig_Tests_Enable
	TCContinuation<NEncoding::CEJSON> CDistributedAppInProcessActor::f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		if (!mp_DistributedApp)
			DMibError("No distributed app");
		
		return mp_DistributedApp(&CDistributedAppActor::f_Test_Command, _Command, _Params);
	}

	TCContinuation<uint32> CDistributedAppInProcessActor::f_RunCommandLine
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
