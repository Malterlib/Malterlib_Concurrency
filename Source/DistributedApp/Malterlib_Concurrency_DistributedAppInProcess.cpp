// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Concurrency/LogError>
#include <Mib/Encoding/JsonShortcuts>

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
			NStr::CStr _HomeDirectory
			, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> _fDistributedAppFactory
			, NContainer::TCVector<NStr::CStr> _Params
		)
	{
		{
			auto &ThreadLocal = fg_DistributedAppThreadLocal();
			auto OldSettings = ThreadLocal.m_DefaultSettings;
			auto OldAppType = ThreadLocal.m_DefaultAppType;
			auto Cleanup = g_OnScopeExit / [&]
				{
					ThreadLocal.m_DefaultSettings = OldSettings;
					ThreadLocal.m_DefaultAppType = OldAppType;
				}
			;

			ThreadLocal.m_DefaultAppType = EDistributedAppType_InProcess;
			ThreadLocal.m_DefaultSettings.m_bSeparateDistributionManager = true;
			ThreadLocal.m_DefaultSettings.m_RootDirectory = _HomeDirectory;

			auto &InterfaceSettings = ThreadLocal.m_DefaultSettings.m_InterfaceSettings;

			DMibFastCheck(!InterfaceSettings.m_pRequestTicket);

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

			{
				auto CaptureScope = co_await g_CaptureExceptions;
				mp_DistributedApp = _fDistributedAppFactory();
			}
		}

		NEncoding::CEJsonSorted AppParams;
		{
			using namespace NStr;
			CDistributedAppCommandLineClient CommandLineClient = co_await mp_DistributedApp(&CDistributedAppActor::f_GetCommandLineClient, nullptr);
			CommandLineClient.f_MutateCommandLineSpecification
				(
					[&](CDistributedAppCommandLineSpecification &_Spec, CDistributedAppActor_Settings const &_Settings)
					{
						_Spec.f_GetDefaultSection().f_RegisterDirectCommand
							(
								{
									"Names"_o= _o["--daemon-run-in-app"]
									, "Description"_o= "Run as in app daemon."
								}
								, [](NEncoding::CEJsonSorted const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
								{
									return 0;
								}
							)
						;
					}
				)
			;

			NContainer::TCVector<NStr::CStr> Params{"InApp", "--daemon-run-in-app"};

			Params.f_Insert(fg_Move(_Params));

			if (auto iRunStandalone = Params.f_Contains("--daemon-run-standalone"); iRunStandalone >= 0)
				Params.f_Remove(iRunStandalone);

			CDistributedAppCommandLineSpecification::CParsedCommandLine CommandLine;
			{
				auto CaptureScope = co_await g_CaptureExceptions;
				CommandLine = CommandLineClient.f_ParseCommandLine(fg_Move(Params));
			}

			AppParams = fg_Move(CommandLine.m_Params);
		}

		co_return co_await mp_DistributedApp(&CDistributedAppActor::f_StartApp, fg_Move(AppParams), nullptr, EDistributedAppType_InProcess);
	}

	TCFuture<void> CDistributedAppInProcessActor::fp_Destroy()
	{
		CLogError LogError("Malterlib/Concurrency/InProcessApp");

		if (mp_DistributedApp)
		{
			co_await mp_DistributedApp(&CDistributedAppActor::f_StopApp).f_Wrap() > LogError.f_Warning("Failed to stop distributed app");
			co_await fg_Move(mp_DistributedApp).f_Destroy().f_Wrap()  > LogError.f_Warning("Failed to destroy distributed app");;
		}

		co_return {};
	}

	TCFuture<CDistributedActorTrustManager::CTrustTicket> CDistributedAppInProcessActor::fp_HandleTicketRequest()
	{
		DMibLogWithCategory(Malterlib/Concurrency, Info, "Generating ticket for '{}'", mp_Description);

		NStr::CStr HandleRequestID = NCryptography::fg_RandomID(mp_HandleRequests);

		auto Ticket = co_await mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, g_ActorFunctor
				(
					g_ActorSubscription / [this, HandleRequestID]() -> TCFuture<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							co_return {};

						if (pHandleRequest->m_NotificationsSubscription)
							co_await pHandleRequest->m_NotificationsSubscription->f_Destroy();

						mp_HandleRequests.f_Remove(HandleRequestID);

						co_return {};
					}
				)
				/ [this, HandleRequestID](NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest) -> TCFuture<void>
				{
					if (mp_fOnUseTicket)
						co_await mp_fOnUseTicket(_HostID, _HostInfo, _CertificateRequest);

					mp_HandleRequests.f_Remove(HandleRequestID);

					co_return {};
				}
				, nullptr
			)
			.f_Wrap()
		;

		if (!Ticket)
		{
			DMibLogWithCategory
				(
					Malterlib/Concurrency
					, Error
					, "Failed to generate ticket for '{}': {}"
					, mp_Description
					, Ticket.f_GetExceptionStr()
				)
			;
			co_return Ticket.f_GetException();
		}

		auto &Request = mp_HandleRequests[HandleRequestID];
		Request.m_NotificationsSubscription = fg_Move(Ticket->m_NotificationsSubscription);

		DMibLogWithCategory(Malterlib/Concurrency, Info, "Sending ticket to '{}'", mp_Description);
		co_return fg_Move(Ticket->m_Ticket);
	}

#if DMibConfig_Tests_Enable
	TCFuture<NEncoding::CEJsonSorted> CDistributedAppInProcessActor::f_Test_Command(NStr::CStr _Command, NEncoding::CEJsonSorted const _Params)
	{
		if (!mp_DistributedApp)
			co_return DMibErrorInstance("No distributed app");

		co_return co_await mp_DistributedApp(&CDistributedAppActor::f_Test_Command, fg_Move(_Command), fg_Move(fg_RemoveQualifiers(_Params)));
	}

	TCFuture<uint32> CDistributedAppInProcessActor::f_RunCommandLine
		(
			CCallingHostInfo _CallingHost
			, NStr::CStr _Command
			, NEncoding::CEJsonSorted _Params
			, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
		)
	{
		if (!mp_DistributedApp)
			co_return DMibErrorInstance("No distributed app");

		CCallingHostInfoScope Scope(fg_Move(_CallingHost));
		co_return co_await mp_DistributedApp(&CDistributedAppActor::f_RunCommandLine, fg_Move(_Command), fg_Move(_Params), fg_Move(_pCommandLine));
	}
#endif
}
