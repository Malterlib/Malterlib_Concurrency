// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Encoding/JSONShortcuts>

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

			try
			{
				mp_DistributedApp = _fDistributedAppFactory();
			}
			catch (NException::CException const &)
			{
				co_return NException::fg_CurrentException();
			}
		}

		NEncoding::CEJSON AppParams;
		{
			using namespace NStr;
			CDistributedAppCommandLineClient CommandLineClient = co_await mp_DistributedApp(&CDistributedAppActor::f_GetCommandLineClient);
			CommandLineClient.f_MutateCommandLineSpecification
				(
					[&](CDistributedAppCommandLineSpecification &_Spec, CDistributedAppActor_Settings const &_Settings)
					{
						_Spec.f_GetDefaultSection().f_RegisterDirectCommand
							(
								{
									"Names"_= {"--daemon-run-in-app"}
									, "Description"_= "Run as in app daemon"
								}
								, [](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
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
			try
			{
				CommandLine = CommandLineClient.f_ParseCommandLine(Params);
			}
			catch (NException::CException const &)
			{
				co_return NException::fg_CurrentException();
			}
			AppParams = fg_Move(CommandLine.m_Params);
		}

		co_return co_await mp_DistributedApp(&CDistributedAppActor::f_StartApp, fg_Move(AppParams), nullptr, EDistributedAppType_InProcess);
	}

	TCFuture<void> CDistributedAppInProcessActor::fp_Destroy()
	{
		if (mp_DistributedApp)
		{
			co_await mp_DistributedApp(&CDistributedAppActor::f_StopApp);
			co_await fg_Move(mp_DistributedApp).f_Destroy();
		}

		co_return {};
	}

	TCFuture<CDistributedActorTrustManager::CTrustTicket> CDistributedAppInProcessActor::fp_HandleTicketRequest()
	{
		TCPromise<CDistributedActorTrustManager::CTrustTicket> Promise;

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
				/ [this, HandleRequestID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCFuture<void>
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
	TCFuture<NEncoding::CEJSON> CDistributedAppInProcessActor::f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		TCPromise<NEncoding::CEJSON> Promise;
		if (!mp_DistributedApp)
			return Promise <<= DMibErrorInstance("No distributed app");

		return Promise <<= mp_DistributedApp(&CDistributedAppActor::f_Test_Command, _Command, _Params);
	}

	TCFuture<uint32> CDistributedAppInProcessActor::f_RunCommandLine
		(
			CCallingHostInfo const &_CallingHost
			, NStr::CStr const &_Command
			, NEncoding::CEJSON const &_Params
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		TCPromise<uint32> Promise;
		if (!mp_DistributedApp)
			return Promise <<= DMibErrorInstance("No distributed app");

		CCallingHostInfoScope Scope(fg_TempCopy(_CallingHost));
		return Promise <<= mp_DistributedApp(&CDistributedAppActor::f_RunCommandLine, _Command, _Params, _pCommandLine);
	}
#endif
}
