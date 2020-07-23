// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;

	TCFuture<TCActor<CDistributedAppSensorStoreLocal>> CDistributedAppActor::fp_OpenSensorStoreLocal()
	{
		auto &Internal = *mp_pInternal;
		if (Internal.m_AppSensorStoreLocal)
			co_return Internal.m_AppSensorStoreLocal;

		auto OnResume = g_OnResume / [this]
			{
				if (f_IsDestroyed())
					DMibError("Shutting down");
			}
		;

		auto InitSequenceSubscription = co_await Internal.m_AppSensorStoreLocalInitSequencer.f_Sequence();
		
		TCActor<CDistributedAppSensorStoreLocal> AppSensorStoreLocal = fg_Construct(mp_State.m_DistributionManager, mp_State.m_TrustManager);
		co_await AppSensorStoreLocal(&CDistributedAppSensorStoreLocal::f_StartWithDatabasePath, mp_Settings.m_RootDirectory / ("SensorStore.{}"_f << mp_Settings.m_AppName));

		Internal.m_AppSensorStoreLocal = fg_Move(AppSensorStoreLocal);

		Internal.m_AppSensorStoreLocalAppServerChangeSubscription = co_await self
			(
				&CDistributedAppActor::fp_RegisterForAppInterfaceServerChanges
				, g_ActorFunctor / [this](TCDistributedActor<CDistributedAppInterfaceServer> const &_AppInterfaceServer, CTrustedActorInfo const &_TrustInfo) -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;
					auto OnResume = g_OnResume / [&]
						{
							if (f_IsDestroyed() || !Internal.m_AppSensorStoreLocal)
								DMibError("Shutting down");
						}
					;

					auto SequenceSubscription = co_await Internal.m_AppSensorStoreLocalAppServerChangeSequencer.f_Sequence();

					if (Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription)
					{
						auto Result = co_await fg_Exchange(Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription, nullptr)->f_Destroy().f_Wrap();
						if (!Result)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/App
									, Error
									, "Exception destroying extra sensor interface subscription: {}"
									, Result.f_GetExceptionStr()
								)
							;
						}
					}

					auto SensorInterface = co_await mp_State.m_AppInterfaceServer.f_CallActor(&CDistributedAppInterfaceServer::f_GetSensorReporter)();

					if (SensorInterface)
					{
						Internal.m_AppSensorStoreLocalExtraSensorInterfaceSubscription = co_await Internal.m_AppSensorStoreLocal
							(
								&CDistributedAppSensorStoreLocal::f_AddExtraSensorReporter
								, fg_Move(SensorInterface)
								, _TrustInfo
							)
						;
					}
					
					co_return {};
				}
			)
		;

		co_return Internal.m_AppSensorStoreLocal;
	}

	auto CDistributedAppActor::fp_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo)
		-> TCFuture<CDistributedAppSensorReporter::CSensorReporter>
	{
		auto Store = co_await fp_OpenSensorStoreLocal();

		co_return co_await Store(&CDistributedAppSensorStoreLocal::f_OpenSensorReporter, fg_Move(_SensorInfo));
	}
}
