// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

	struct CDistributedAppSensorStoreLocal::CInternal : public CActorInternal
	{
		struct CSensor;
		struct CRemoteSensorReporterInterface;

		struct CSensorReporter
		{
			auto f_GetID() const -> TCWeakDistributedActor<CActor> const &
			{
				return TCMap<TCWeakDistributedActor<CActor>, CSensorReporter>::fs_GetKey(*this);
			}

			CSensor *m_pSensor = nullptr;
			CRemoteSensorReporterInterface *m_pInterface = nullptr;
			CDistributedAppSensorReporter::CSensorReporter m_Reporter;
			CSequencer m_WriteSequencer{"DistributedAppSensorStoreLocal SensorReporter WriteSequencer {}"_f << f_GetID()};
			DMibListLinkDS_Link(CSensorReporter, m_LinkInterface);
			DMibListLinkDS_Link(CSensorReporter, m_LinkFailed);
		};

		struct CSensor
		{
			CSensor(CSensor &&) = default;
			CSensor(CDistributedAppSensorReporter::CSensorInfo &&_Info)
				: m_Info(fg_Move(_Info))
			{
			}

			CDistributedAppSensorReporter::CSensorInfoKey const &f_GetKey() const
			{
				return TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CSensor>::fs_GetKey(*this);
			}

			CDistributedAppSensorReporter::CSensorInfo m_Info;
			uint64 m_LastSeenUniqueSequence = 0;
			umint m_ActiveRefCount = 0;

			CSequencer m_SensorSequencer{"DistributedAppSensorStoreLocal Sensor SensorSequencer {}"_f << m_Info.f_Key()};
			TCMap<TCWeakDistributedActor<CActor>, CSensorReporter> m_SensorReporters;

			TCSharedPointer<CCanDestroyTracker> m_pCanDestroy;
		};

		struct CRemoteSensorReporterInterface
		{
			TCDistributedActorInterface<CDistributedAppSensorReporter> m_Actor;
			DMibListLinkDS_List(CSensorReporter, m_LinkInterface) m_Reporters;
		};

		struct CSensorSubscription
		{
			TCVector<CDistributedAppSensorReader_SensorFilter> m_Filters;
			TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader::CSensorChange _Change)> m_fOnChange;

			TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CDistributedAppSensorReader::CSensorChange> m_QueuedChanges;
		};

		struct CReadingSubscriptionShared
		{
			TCMap<CDistributedAppSensorReporter::CSensorInfoKey, zuint64> m_LastSeenReading;
			TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading _Reading)> m_fOnReading;
			TCVector<CDistributedAppSensorReader_SensorKeyAndReading> m_QueuedReadings;
		};

		struct CReadingSubscription : public CReadingSubscriptionShared
		{
			TCVector<CDistributedAppSensorReader_SensorReadingSubscriptionFilter> m_Filters;
		};

		struct CStatusSubscription : public CReadingSubscriptionShared
		{
			TCVector<CDistributedAppSensorReader_SensorStatusFilter> m_Filters;
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

		TCFuture<void> f_UpdateSensorForReporter(CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey, TCWeakDistributedActor<CActor> _Actor);

		TCFuture<void> f_SensorReporterInterfaceAdded(TCDistributedActorInterface<CDistributedAppSensorReporter> _Actor, CTrustedActorInfo _TrustInfo);
		TCFuture<void> f_SensorReporterInterfaceRemoved(TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _TrustInfo);
		TCFuture<void> f_SensorInfoChanged(CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey, bool _bWasAdded);
		TCFuture<void> f_NewSensorReadings
			(
				CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey
				, TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const> _pReadings
			)
		;
		TCFuture<void> f_StoreSensorReadings
			(
				CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey
				, NSensorStoreLocalDatabase::CSensorReadingKey _DatabaseKey
				, TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const> _pReadings
			)
		;
		TCFuture<void> f_CleanupSensorReporter(CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey);

		auto f_GetReportReadingsFunctor(CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey, NSensorStoreLocalDatabase::CSensorReadingKey _DatabaseKey)
			-> TCActorFunctorWithID<TCFuture<void> (NContainer::TCVector<CDistributedAppSensorReporter::CSensorReading> _Readings)>
		;

		static TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fs_Cleanup(CInternal *_pThis, NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction);
		TCFuture<void> f_PerformLocalCleanup();

		void f_ScheduleFailedRetry();
		TCFuture<void> f_RetryFailedReporters();

		CStr f_GetSensorUpdateFailedMessage(CSensorReporter const &_Reporter);

		void f_Subscription_Readings(CSensor const &_Sensor, TCVector<CDistributedAppSensorReporter::CSensorReading> const &_Readings, NDatabase::CDatabaseSubReadTransaction &_Transaction);
		void f_Subscription_SensorInfoChanged
			(
				CSensor const &_Sensor
				, NSensorStoreLocalDatabase::CKnownHostValue const *_pKnownHostValue
			)
		;

		template <typename tf_CKey, typename tf_CInfoOrKey>
		static tf_CKey fs_GetDatabaseKey(CStr const &_Prefix, tf_CInfoOrKey const &_SensorInfo);

		template <typename tf_CKey, typename tf_CInfoOrKey>
		tf_CKey f_GetDatabaseKey(tf_CInfoOrKey const &_SensorInfo) const;

		TCMap<TCWeakDistributedActor<CActor>, CRemoteSensorReporterInterface> m_SensorInterfaces;

		DMibListLinkDS_List(CSensorReporter, m_LinkFailed) m_FailedReporters;

		CDistributedAppSensorStoreLocal *m_pThis;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CDatabaseActor> m_Database;

		TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CSensor> m_Sensors;
		TCMap<CStr, NSensorStoreLocalDatabase::CKnownHostValue> m_KnownHosts;

		TCSharedPointer<CCanDestroyTracker> m_pCanDestroyStoringLocal = fg_Construct();

		TCTrustedActorSubscription<CDistributedAppSensorReporter> m_SensorsInterfaceSubscription;
		TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)> m_fCleanup;
		CActorSubscription m_CleanupTimerSubscription;
		CStr m_Prefix;
		CStr m_ThisHostID;

		TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CDistributedAppSensorReporter::CSensorReading> m_LastReadings;

		TCMap<CStr, CSensorSubscription> m_SensorSubscriptions;
		TCMap<CStr, CReadingSubscription> m_ReadingSubscriptions;
		TCMap<CStr, CStatusSubscription> m_StatusSubscriptions;

		uint64 m_RetentionDays = 0;

		bool m_bStarted = false;
		bool m_bStarting = false;
		bool m_bOwnDatabase = false;
		bool m_bScheduledFailedReportersRetry = false;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.hpp"
