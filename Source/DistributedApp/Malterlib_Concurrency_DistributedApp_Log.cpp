// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;

	TCFuture<TCActor<CDistributedAppLogStoreLocal>> CDistributedAppActor::fp_OpenLogStoreLocal()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_AppLogStoreLocal)
			co_return Internal.m_AppLogStoreLocal;

		TCActor<CDistributedAppLogStoreLocal> AppLogStoreLocal;

		auto OnResume = g_OnResume / [&]
			{
				if (f_IsDestroyed())
				{
					if (AppLogStoreLocal)
						fg_Move(AppLogStoreLocal).f_Destroy() > fg_DiscardResult();

					DMibError("Shutting down");
				}
			}
		;

		auto InitSequenceSubscription = co_await Internal.m_AppLogStoreLocalInitSequencer.f_Sequence();

		if (Internal.m_AppLogStoreLocal)
			co_return Internal.m_AppLogStoreLocal;

		co_await fp_WaitForDistributedTrustInitialization();

		AppLogStoreLocal = fg_Construct(mp_State.m_DistributionManager, mp_State.m_TrustManager);
		co_await AppLogStoreLocal
			(
				&CDistributedAppLogStoreLocal::f_StartWithDatabasePath
				, mp_Settings.m_RootDirectory / ("LogStore.{}"_f << mp_Settings.m_AppName)
				, mp_State.m_ConfigDatabase.m_Data.f_GetMemberValue("LogRetentionDays", 6 * 30).f_Integer()
			)
		;

		Internal.m_AppLogStoreLocal = fg_Move(AppLogStoreLocal);

		Internal.m_AppLogStoreLocalAppServerChangeSubscription = co_await self
			(
				&CDistributedAppActor::fp_RegisterForAppInterfaceServerChanges
				, g_ActorFunctor / [this](TCDistributedActor<CDistributedAppInterfaceServer> const &_AppInterfaceServer, CTrustedActorInfo const &_TrustInfo) -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;
					auto OnResume = g_OnResume / [&]
						{
							if (f_IsDestroyed() || !Internal.m_AppLogStoreLocal)
								DMibError("Shutting down");
						}
					;

					auto SequenceSubscription = co_await Internal.m_AppLogStoreLocalAppServerChangeSequencer.f_Sequence();

					if (Internal.m_AppLogStoreLocalExtraLogInterfaceSubscription)
					{
						auto Result = co_await fg_Exchange(Internal.m_AppLogStoreLocalExtraLogInterfaceSubscription, nullptr)->f_Destroy().f_Wrap();
						if (!Result)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/App
									, Error
									, "Exception destroying extra Log interface subscription: {}"
									, Result.f_GetExceptionStr()
								)
							;
						}
					}

					if (mp_State.m_AppInterfaceServer->f_InterfaceVersion() >= CDistributedAppInterfaceServer::EProtocolVersion_SupportLogReporter)
					{
						auto LogInterface = co_await mp_State.m_AppInterfaceServer.f_CallActor(&CDistributedAppInterfaceServer::f_GetLogReporter)();

						if (LogInterface)
						{
							Internal.m_AppLogStoreLocalExtraLogInterfaceSubscription = co_await Internal.m_AppLogStoreLocal
								(
									&CDistributedAppLogStoreLocal::f_AddExtraLogReporter
									, fg_Move(LogInterface)
									, _TrustInfo
								)
							;
						}
					}
					
					co_return {};
				}
			)
		;

		co_return Internal.m_AppLogStoreLocal;
	}

	auto CDistributedAppActor::fp_OpenLogReporter(CDistributedAppLogReporter::CLogInfo &&_LogInfo)
		-> TCFuture<CDistributedAppLogReporter::CLogReporter>
	{
		auto Store = co_await fp_OpenLogStoreLocal();

		co_return co_await Store(&CDistributedAppLogStoreLocal::f_OpenLogReporter, fg_Move(_LogInfo));
	}
}
