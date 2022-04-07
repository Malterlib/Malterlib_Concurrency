// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency
{
	using namespace NDatabase;
	using namespace NStr;
	using namespace NIntrusive;
	using namespace NContainer;
	using namespace NException;
	using namespace NStorage;

	struct CDistributedAppSensorStoreLocal::CInternal
	{
		struct CSensor;
		struct CRemoteSensorReporterInterface;

		struct CSensorReporter
		{
			auto &f_GetID() const
			{
				return TCMap<TCWeakDistributedActor<CActor>, CSensorReporter>::fs_GetKey(*this);
			}

			CSensor *m_pSensor = nullptr;
			CRemoteSensorReporterInterface *m_pInterface = nullptr;
			CDistributedAppSensorReporter::CSensorReporter m_Reporter;
			TCActorSequencerAsync<void> m_WriteSequencer;
			DMibListLinkDS_Link(CSensorReporter, m_LinkInterface);
			DMibListLinkDS_Link(CSensorReporter, m_LinkFailed);
		};

		struct CSensor
		{
			CDistributedAppSensorReporter::CSensorInfoKey const &f_GetKey() const
			{
				return TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CSensor>::fs_GetKey(*this);
			}

			CDistributedAppSensorReporter::CSensorInfo m_SensorInfo;
			uint64 m_LastSeenUniqueSequence = 0;
			mint m_ActiveRefCount = 0;

			TCActorSequencerAsync<void> m_InitSensorSequencer;
			TCMap<TCWeakDistributedActor<CActor>, CSensorReporter> m_SensorReporters;

			TCSharedPointer<CCanDestroyTracker> m_pCanDestroy;
		};

		struct CRemoteSensorReporterInterface
		{
			TCDistributedActorInterface<CDistributedAppSensorReporter> m_Actor;
			DMibListLinkDS_List(CSensorReporter, m_LinkInterface) m_Reporters;
		};

		CInternal
			(
				CDistributedAppSensorStoreLocal *_pThis
				, TCActor<CActorDistributionManager> const &_DistributionManager
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
			)
			: m_pThis(_pThis)
			, m_DistributionManager(_DistributionManager)
			, m_TrustManager(_TrustManager)
		{
		}

		TCFuture<void> f_Start();

		TCFuture<void> f_UpdateSensorForReporter(CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey, TCWeakDistributedActor<CActor> const &_Actor);

		TCFuture<void> f_SensorReporterInterfaceAdded(TCDistributedActorInterface<CDistributedAppSensorReporter> &&_Actor, CTrustedActorInfo const &_TrustInfo);
		TCFuture<void> f_SensorReporterInterfaceRemoved(TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo);
		TCFuture<void> f_SensorInfoChanged(CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey, bool _bWasCreated);
		TCFuture<void> f_NewSensorReadings(CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey, TCVector<CDistributedAppSensorReporter::CSensorReading> const &_Readings);
		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> f_Cleanup(NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction);
		TCFuture<void> f_PerformLocalCleanup();

		void f_ScheduleFailedRetry();
		TCFuture<void> f_RetryFailedReporters();

		CStr f_GetSensorUpdateFailedMessage(CSensorReporter const &_Reporter);

		template <typename tf_CKey>
		tf_CKey f_GetDatabaseKey(CDistributedAppSensorReporter::CSensorInfo const &_SensorInfo) const;

		TCMap<TCWeakDistributedActor<CActor>, CRemoteSensorReporterInterface> m_SensorInterfaces;

		DMibListLinkDS_List(CSensorReporter, m_LinkFailed) m_FailedReporters;

		CDistributedAppSensorStoreLocal *m_pThis;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CDatabaseActor> m_Database;

		TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CSensor> m_Sensors;

		TCTrustedActorSubscription<CDistributedAppSensorReporter> m_SensorsInterfaceSubscription;
		TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction)> m_fCleanup;
		CActorSubscription m_CleanupTimerSubscription;
		CStr m_Prefix;

		uint64 m_RetentionDays = 0;
		
		bool m_bStarted = false;
		bool m_bStarting = false;
		bool m_bOwnDatabase = false;
		bool m_bScheduledFailedReportersRetry = false;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.hpp"
