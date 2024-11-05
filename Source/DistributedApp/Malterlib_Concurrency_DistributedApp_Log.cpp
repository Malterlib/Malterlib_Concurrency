// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;

	TCFuture<CActorSubscription> CDistiributedAppLogActor::f_OnStopDeferring(TCActorFunctorWeak<TCFuture<void> ()> _fOnStopDefer)
	{
		CStr SubscriptionID = NCryptography::fg_RandomID(mp_StopDeferSubscriptions);

		mp_StopDeferSubscriptions[SubscriptionID] = fg_Move(_fOnStopDefer);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto *pSubscription = mp_StopDeferSubscriptions.f_FindEqual(SubscriptionID);
				if (*pSubscription)
					co_await fg_Move(*pSubscription).f_Destroy();

				co_return {};
			}
		;
	}

	TCFuture<void> CDistiributedAppLogActor::f_StopDeferring()
	{
		TCFutureVector<void> Results;

		for (auto &fOnStopDefer : mp_StopDeferSubscriptions)
			fOnStopDefer() > Results;

		co_await fg_AllDoneWrapped(Results);

		co_return {};
	}

	TCFuture<void> CDistiributedAppLogActor::fp_Destroy()
	{
		CLogError LogError("DistiributedAppLogActor");

		co_await f_StopDeferring().f_Wrap() > LogError.f_Warning("Failed to stop deferring");;

		co_return {};
	}


	TCFuture<TCActor<CDistributedAppLogStoreLocal>> CDistributedAppActor::fp_OpenLogStoreLocal()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_AppLogStoreLocal)
			co_return Internal.m_AppLogStoreLocal;

		TCActor<CDistributedAppLogStoreLocal> AppLogStoreLocal;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> NException::CExceptionPointer
				{
					if (Internal.m_bAuditLogsDestroyed)
					{
						if (AppLogStoreLocal)
							fg_Move(AppLogStoreLocal).f_Destroy().f_DiscardResult();

						return DMibErrorInstance("Shutting down");
					}

					return {};
				}
			)
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

		Internal.m_AppLogStoreLocalAppServerChangeSubscription = co_await fp_RegisterForAppInterfaceServerChanges
			(
				g_ActorFunctor / [this](TCDistributedActor<CDistributedAppInterfaceServer> _AppInterfaceServer, CTrustedActorInfo _TrustInfo) -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;
					auto OnResume = co_await fg_OnResume
						(
							[&]() -> NException::CExceptionPointer
							{
								if (Internal.m_bAuditLogsDestroyed || !Internal.m_AppLogStoreLocal)
									return DMibErrorInstance("Shutting down");
								return {};
							}
						)
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

	auto CDistributedAppActor::fp_OpenLogReporter(CDistributedAppLogReporter::CLogInfo _LogInfo)
		-> TCFuture<CDistributedAppLogReporter::CLogReporter>
	{
		auto Store = co_await fp_OpenLogStoreLocal();

		for (auto &MetaData : mp_LogMetaData.f_Entries())
		{
			auto &Key = MetaData.f_Key();

			if (_LogInfo.m_MetaData.f_FindEqual(Key))
				continue;

			_LogInfo.m_MetaData[Key] = MetaData.f_Value();
		}

		co_return co_await Store(&CDistributedAppLogStoreLocal::f_OpenLogReporter, fg_Move(_LogInfo));
	}

	auto CDistributedAppActor::f_OpenLogReporter(CDistributedAppLogReporter::CLogInfo _LogInfo) -> TCFuture<CDistributedAppLogReporter::CLogReporter>
	{
		co_return co_await fp_OpenLogReporter(fg_Move(_LogInfo));
	}

	TCFuture<TCActor<CDistributedAppLogStoreLocal>> CDistributedAppActor::f_OpenLogStoreLocal()
	{
		co_return co_await fp_OpenLogStoreLocal();
	}
}
