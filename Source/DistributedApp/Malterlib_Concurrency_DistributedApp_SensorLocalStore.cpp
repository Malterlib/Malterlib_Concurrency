// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	CDistributedAppSensorStoreLocal::CDistributedAppSensorStoreLocal
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
		: mp_pInternal(fg_Construct(this, _DistributionManager, _TrustManager))
	{
	}

	CDistributedAppSensorStoreLocal::~CDistributedAppSensorStoreLocal()
	{
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_DestroyRemote()
	{
		auto &Internal = *mp_pInternal;

		CLogError LogError("SensorLocalStore");

		co_await Internal.m_SensorsInterfaceSubscription.f_Destroy().f_Wrap() > LogError.f_Warning("Failed destory local sensors reporters remote interface subscription");

		{
			TCFutureVector<void> Destroys;
			for (auto &Sensor : Internal.m_Sensors)
			{
				for (auto &Reporter : Sensor.m_SensorReporters)
				{
					fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Destroys;
					if (Reporter.m_Reporter.m_fReportReadings)
						fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Destroys;
				}
				Sensor.m_SensorReporters.f_Clear();
			}

			co_await fg_AllDone(Destroys).f_Wrap() > LogError.f_Warning("Failed destroy local sensor reporters remote");;
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		CLogError LogError("SensorLocalStore");

		if (Internal.m_CleanupTimerSubscription)
		{
			co_await Internal.m_CleanupTimerSubscription->f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy cleanup timer subscription");
			Internal.m_CleanupTimerSubscription.f_Clear();
		}

		auto CanDestroyFuture = Internal.m_pCanDestroyStoringLocal->f_Future();
		Internal.m_pCanDestroyStoringLocal.f_Clear();
		co_await fg_Move(CanDestroyFuture).f_Timeout(10.0, "Timeout").f_Wrap() > LogError.f_Warning("Failed to wait for can destroy");

		{
			TCFutureVector<void> Destroys;
			for (auto &Sensor : Internal.m_Sensors)
			{
				fg_Move(Sensor.m_SensorSequencer).f_Destroy() > Destroys;
				for (auto &Reporter : Sensor.m_SensorReporters)
				{
					fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Destroys;
					if (Reporter.m_Reporter.m_fReportReadings)
						fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Destroys;
				}
			}

			Internal.m_SensorsInterfaceSubscription.f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError.f_Warning("Failed destroy local sensor store");;
		}

		if (Internal.m_bOwnDatabase)
			co_await fg_Move(Internal.m_Database).f_Destroy().f_Wrap() > LogError.f_Warning("Failed destroy database");;

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_StartWithDatabase
		(
			TCActor<NDatabase::CDatabaseActor> _Database
			, CStr _Prefix
			, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)> _fCleanup
		)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Move(_Database);
		Internal.m_fCleanup = fg_Move(_fCleanup);
		Internal.m_Prefix = _Prefix;

		co_await Internal.f_Start();
		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_StartWithDatabasePath(CStr _DatabasePath, uint64 _RetentionDays)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Construct();
		Internal.m_bOwnDatabase = true;
		Internal.m_Prefix = "mib.lsensor"; // lsensor = local sensor
		Internal.m_RetentionDays = _RetentionDays;

		uint64 MaxDatabaseSize = constant_int64(1) * 1024 * 1024 * 1024;

		co_await Internal.m_Database(&CDatabaseActor::f_OpenDatabase, _DatabasePath, MaxDatabaseSize);

		[[maybe_unused]] auto Stats = co_await (Internal.m_Database(&CDatabaseActor::f_GetAggregateStatistics));
		[[maybe_unused]] auto TotalSizeUsed = Stats.f_GetTotalUsedSize();

		DMibLogWithCategory
			(
				SensorLocalStore
				, Debug
				, "Database at '{}' uses {fe2}% of allotted space ({ns } / {ns } bytes). {ns } records."
				, _DatabasePath
				, fp64(TotalSizeUsed) / fp64(MaxDatabaseSize) * 100.0
				, TotalSizeUsed
				, MaxDatabaseSize
				, Stats.m_nDataItems
			)
		;

		co_await Internal.f_Start();
		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_PerformLocalCleanup()
	{
		auto CheckDestroy = co_await m_pThis->f_CheckDestroyedOnResume();

		co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, this](CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_return co_await fs_Cleanup(this, fg_Move(_Transaction));
				}
			)
		;

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_Start()
	{
		auto OnResume = co_await m_pThis->f_CheckDestroyedOnResume();

		m_ThisHostID = co_await m_TrustManager(&CDistributedActorTrustManager::f_GetHostID);

		CSensorGlobalStateKey GlobalStateKey{.m_Prefix = m_Prefix};
		CSensorGlobalStateValue GlobalStateValue;
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			auto [StateValue, Sensors] = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[GlobalStateKey, Prefix = m_Prefix](CDatabaseActor::CTransactionRead &&_ReadTransaction)
					{
						CSensorGlobalStateValue GlobalStateValue;
						_ReadTransaction.m_Transaction.f_Get(GlobalStateKey, GlobalStateValue);

						TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CSensor> Sensors;

						for (auto iSensor = _ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CSensorKey::mc_Prefix); iSensor; ++iSensor)
						{
							auto Key = iSensor.f_Key<CSensorKey>();
							auto Value = iSensor.f_Value<CSensorValue>();

							auto &Sensor = *Sensors(Key.f_SensorInfoKey(), CSensor{fg_Move(Value.m_Info)});
							Sensor.m_LastSeenUniqueSequence = Value.m_UniqueSequenceAtLastCleanup;

							{
								auto DatabaseKey = fs_GetDatabaseKey<CSensorReadingKey>(Prefix, Sensor.m_Info);
								auto ReadCursor = _ReadTransaction.m_Transaction.f_ReadCursor((CSensorKey const &)DatabaseKey);
								if (ReadCursor.f_Last())
									Sensor.m_LastSeenUniqueSequence = fg_Max(ReadCursor.f_Key<CSensorReadingKey>().m_UniqueSequence, Sensor.m_LastSeenUniqueSequence);
							}
						}

						return fg_Tuple(fg_Move(GlobalStateValue), fg_Move(Sensors));
					}
					, "Error reading sensor data from database (f_Start)"
				)
			;

			GlobalStateValue = fg_Move(StateValue);
			m_Sensors = fg_Move(Sensors);
		}

		if (!GlobalStateValue.m_bConvertedKnownHosts)
		{
			co_await m_Database
				(
					&CDatabaseActor::f_WriteWithCompaction
					, g_ActorFunctorWeak / [=, ThisActor = fg_ThisActor(m_pThis), Prefix = m_Prefix]
					(CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;

						auto WriteTransaction = fg_Move(_Transaction);
						if (_bCompacting)
							WriteTransaction = co_await ThisActor(&CDistributedAppSensorStoreLocal::fp_Cleanup, fg_Move(WriteTransaction));

						CSensorGlobalStateValue GlobalStateValue;
						WriteTransaction.m_Transaction.f_Get(GlobalStateKey, GlobalStateValue);

						if (GlobalStateValue.m_bConvertedKnownHosts)
							co_return fg_Move(WriteTransaction);

						for (auto iSensor = WriteTransaction.m_Transaction.f_ReadCursor(Prefix, CSensorKey::mc_Prefix); iSensor; ++iSensor)
						{
							auto Key = iSensor.f_Key<CSensorKey>();
							auto Value = iSensor.f_Value<CSensorValue>();

							CSensorKey ReadingKey = Key;
							ReadingKey.m_Prefix = CSensorReadingKey::mc_Prefix;

							auto iReading = WriteTransaction.m_Transaction.f_ReadCursor(ReadingKey);

							if (!iReading.f_Last())
								continue;

							auto ReadingValue = iReading.f_Value<CSensorReadingValue>();

							if (!Key.m_HostID)
								continue;

							CKnownHostKey KnownHostKey{.m_DbPrefix = Prefix, .m_HostID = Key.m_HostID};

							CKnownHostValue KnownHostValue;
							_Transaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

							KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, ReadingValue.m_Timestamp);
							_Transaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);
						}

						GlobalStateValue.m_bConvertedKnownHosts = true;
						WriteTransaction.m_Transaction.f_Upsert(GlobalStateKey, GlobalStateValue);

						co_return fg_Move(WriteTransaction);
					}
				)
			;
		}

		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			m_KnownHosts = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[Prefix = m_Prefix](CDatabaseActor::CTransactionRead &&_ReadTransaction)
					{
						TCMap<CStr, NSensorStoreLocalDatabase::CKnownHostValue> KnownHosts;

						for (auto iKnownHost = _ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CKnownHostKey::mc_Prefix); iKnownHost; ++iKnownHost)
							KnownHosts(fg_Move(iKnownHost.f_Key<CKnownHostKey>()).m_HostID, iKnownHost.f_Value<CKnownHostValue>());

						return KnownHosts;
					}
					, "Error reading known hosts data from database (f_Start)"
				)
			;
		}

		if (m_RetentionDays && !m_fCleanup)
		{
			co_await f_PerformLocalCleanup().f_Wrap() > fg_LogError("SensorLocalStore", "Failed to perform initial database cleanup");

			m_CleanupTimerSubscription = co_await fg_RegisterTimer
				(
					24_hours
					, [this]() -> TCFuture<void>
					{
						auto Result = co_await f_PerformLocalCleanup().f_Wrap();

						if (!Result)
							DMibLogWithCategory(SensorLocalStore, Error, "Failed to do nightly cleanup: {}", Result.f_GetExceptionStr());

						co_return {};
					}
				)
			;
		}

		m_SensorsInterfaceSubscription = co_await m_TrustManager->f_SubscribeTrustedActors<CDistributedAppSensorReporter>();

		co_await m_SensorsInterfaceSubscription.f_OnActor
			(
				g_ActorFunctor / [this](TCDistributedActor<CDistributedAppSensorReporter> _Actor, CTrustedActorInfo _TrustInfo) -> TCFuture<void>
				{
					auto Result = co_await f_SensorReporterInterfaceAdded(fg_TempCopy(_Actor), _TrustInfo).f_Wrap();
					if (!Result)
						DMibLogWithCategory(SensorLocalStore, Error, "Failures adding new sensor reporter: {}", Result.f_GetExceptionStr());

					co_return {};
				}
				, g_ActorFunctor / [this](TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _TrustInfo) -> TCFuture<void>
				{
					co_await f_SensorReporterInterfaceRemoved(_Actor, fg_Move(_TrustInfo));

					co_return {};
				}
				, "Malterlib/Concurrency/Sensor"
				, "Failed to handle '{}' for sensor reporter"
			)
		;

		m_bStarted = true;

		co_return {};
	}

	TCFuture<CActorSubscription> CDistributedAppSensorStoreLocal::f_AddExtraSensorReporter
		(
			TCDistributedActorInterface<CDistributedAppSensorReporter> _SensorReporter
			, CTrustedActorInfo _TrustInfo
		)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Subscription = g_ActorSubscription / [this, _TrustInfo, SensorInterfaceWeak = _SensorReporter.f_Weak(), TrustInfo = _TrustInfo]() mutable -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;
				co_await Internal.f_SensorReporterInterfaceRemoved(SensorInterfaceWeak, fg_Move(TrustInfo)).f_Wrap()
					> fg_LogError("Malterlib/Concurrency/Sensor", "Error removing sensor reporter")
				;

				co_return {};
			}
		;

		auto Result = co_await Internal.f_SensorReporterInterfaceAdded(fg_Move(_SensorReporter), _TrustInfo).f_Wrap();
		if (!Result)
			DMibLogWithCategory(SensorLocalStore, Error, "Failures adding extra sensor reporter: {}", Result.f_GetExceptionStr());

		co_return fg_Move(Subscription);
	}
}
