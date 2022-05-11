// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"

namespace NMib::NConcurrency
{
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

	TCFuture<void> CDistributedAppSensorStoreLocal::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_CleanupTimerSubscription)
		{
			co_await Internal.m_CleanupTimerSubscription->f_Destroy();
			Internal.m_CleanupTimerSubscription.f_Clear();
		}

		auto CanDestroyFuture = Internal.m_pCanDestroyStoringLocal->f_Future();
		Internal.m_pCanDestroyStoringLocal.f_Clear();
		co_await fg_Move(CanDestroyFuture).f_Timeout(10.0, "Timeout").f_Wrap();

		TCActorResultVector<void> Destroys;
		for (auto &Sensor : Internal.m_Sensors)
		{
			Sensor.m_SensorSequencer.f_Abort() > Destroys.f_AddResult();
			for (auto &Reporter : Sensor.m_SensorReporters)
			{
				Reporter.m_WriteSequencer.f_Abort() > Destroys.f_AddResult();
				if (Reporter.m_Reporter.m_fReportReadings)
					fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Destroys.f_AddResult();
			}
		}

		co_await Destroys.f_GetResults();

		if (Internal.m_bOwnDatabase)
			co_await Internal.m_Database.f_Destroy();

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_StartWithDatabase
		(
			TCActor<NDatabase::CDatabaseActor> &&_Database
			, CStr const &_Prefix
			, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction)> &&_fCleanup
		)
	{
		auto OnResume = g_OnResume / [&]
			{
				if (f_IsDestroyed())
					DMibError("Shutting down");
			}
		;

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Move(_Database);
		Internal.m_fCleanup = fg_Move(_fCleanup);
		Internal.m_Prefix = _Prefix;

		co_await fg_CallSafe(&Internal, &CInternal::f_Start);
		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_StartWithDatabasePath(CStr const &_DatabasePath, uint64 _RetentionDays)
	{
		auto OnResume = g_OnResume / [&]
			{
				if (f_IsDestroyed())
					DMibError("Shutting down");
			}
		;

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Construct(fg_Construct(), "");
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
				, "Database at '{}' uses {fe2}% of allotted space ({ns } / {ns } bytes)"
				, _DatabasePath
				, fp64(TotalSizeUsed) / fp64(MaxDatabaseSize) * 100.0
				, TotalSizeUsed
				, MaxDatabaseSize
			)
		;

		co_await fg_CallSafe(&Internal, &CInternal::f_Start);
		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_PerformLocalCleanup()
	{
		auto WriteTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionWrite);
		WriteTransaction = co_await fg_CallSafe(*this, &CInternal::f_Cleanup, fg_Move(WriteTransaction));
		co_await m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_Start()
	{
		auto OnResume = g_OnResume / [&]
			{
				if (m_pThis->f_IsDestroyed())
					DMibError("Shutting down");
			}
		;

		try
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			for (auto iSensor = ReadTransaction.m_Transaction.f_ReadCursor(m_Prefix, NSensorStoreLocalDatabase::CSensorKey::mc_Prefix); iSensor; ++iSensor)
			{
				auto Key = iSensor.f_Key<NSensorStoreLocalDatabase::CSensorKey>();
				auto Value = iSensor.f_Value<NSensorStoreLocalDatabase::CSensorValue>();

				auto &Sensor = *m_Sensors(Key.f_SensorInfoKey(), CSensor{fg_Move(Value.m_Info)});

				Sensor.m_LastSeenUniqueSequence = Value.m_UniqueSequenceAtLastCleanup;

				{
					auto DatabaseKey = f_GetDatabaseKey<NSensorStoreLocalDatabase::CSensorReadingKey>(Sensor.m_SensorInfo);
					auto ReadCursor = ReadTransaction.m_Transaction.f_ReadCursor((NSensorStoreLocalDatabase::CSensorKey const &)DatabaseKey);
					if (ReadCursor.f_Last())
						Sensor.m_LastSeenUniqueSequence = fg_Max(ReadCursor.f_Key<NSensorStoreLocalDatabase::CSensorReadingKey>().m_UniqueSequence, Sensor.m_LastSeenUniqueSequence);
				}
			}
		}
		catch (CException const &_Exception)
		{
			co_return DMibErrorInstance("Error reading sensor data from database (f_Start): {}"_f << _Exception);
		}

		if (m_RetentionDays && !m_fCleanup)
		{
			co_await fg_CallSafe(this, &CInternal::f_PerformLocalCleanup);

			m_CleanupTimerSubscription = co_await fg_RegisterTimer
				(
					24.0 * 60.0 * 60.0
					, [this]() -> TCFuture<void>
					{
						auto Result = co_await fg_CallSafe(this, &CInternal::f_PerformLocalCleanup).f_Wrap();

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
				g_ActorFunctor / [this](TCDistributedActor<CDistributedAppSensorReporter> const &_Actor, CTrustedActorInfo const &_TrustInfo) -> TCFuture<void>
				{
					auto Result = co_await fg_CallSafe(*this, &CInternal::f_SensorReporterInterfaceAdded, fg_TempCopy(_Actor), _TrustInfo).f_Wrap();
					if (!Result)
						DMibLogWithCategory(LogLocalStore, Error, "Failures adding new sensor reporter: {}", Result.f_GetExceptionStr());

					co_return {};
				}
				, g_ActorFunctor / [this](TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo) -> TCFuture<void>
				{
					co_await fg_CallSafe(*this, &CInternal::f_SensorReporterInterfaceRemoved, _Actor, fg_Move(_TrustInfo));

					co_return {};
				}
				, "Malterlib/Concurrency/Sensor"
				, "Failed to {} sensor reporter"
			)
		;

		m_bStarted = true;

		co_return {};
	}

	TCFuture<CActorSubscription> CDistributedAppSensorStoreLocal::f_AddExtraSensorReporter
		(
			TCDistributedActorInterface<CDistributedAppSensorReporter> &&_SensorReporter
			, CTrustedActorInfo const &_TrustInfo
		)
	{
		auto OnResume = g_OnResume / [&]
			{
				if (f_IsDestroyed())
					DMibError("Shutting down");
			}
		;

		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Subscription = g_ActorSubscription / [this, _TrustInfo, SensorInterfaceWeak = _SensorReporter.f_Weak(), TrustInfo = _TrustInfo]() mutable
			{
				auto &Internal = *mp_pInternal;
				fg_CallSafe(Internal, &CInternal::f_SensorReporterInterfaceRemoved, SensorInterfaceWeak, fg_Move(TrustInfo))
					> fg_LogError("Malterlib/Concurrency/Sensor", "Error removing sensor reporter")
				;
			}
		;

		auto Result = co_await fg_CallSafe(Internal, &CInternal::f_SensorReporterInterfaceAdded, fg_Move(_SensorReporter), _TrustInfo).f_Wrap();
		if (!Result)
			DMibLogWithCategory(LogLocalStore, Error, "Failures adding extra sensor reporter: {}", Result.f_GetExceptionStr());

		co_return fg_Move(Subscription);
	}
}
