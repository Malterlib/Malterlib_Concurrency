// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	auto CDistributedAppSensorStoreLocal::f_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo)
		-> TCFuture<CDistributedAppSensorReporter::CSensorReporter>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto SensorInfoKey = _SensorInfo.f_Key();

		auto SensorMap = Internal.m_Sensors(SensorInfoKey);

		auto *pSensor = &*SensorMap;

		auto OnResume = g_OnResume / [&]
			{
				auto &Internal = *mp_pInternal;

				if (f_IsDestroyed())
					DMibError("Shutting down");

				pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
				if (!pSensor)
					DMibError("Aborted");
			}
		;

		bool bWasCreated = SensorMap.f_WasCreated();
		bool bWasAdded = ++pSensor->m_ActiveRefCount == 0;

		if (!pSensor->m_pCanDestroy)
			pSensor->m_pCanDestroy = fg_Construct();

		{
			auto SequenceSubscription = co_await pSensor->m_InitSensorSequencer.f_Sequence();
			auto &Internal = *mp_pInternal;

			bool bWasChanged = bWasCreated || _SensorInfo != pSensor->m_SensorInfo;

			if (bWasChanged)
			{
				pSensor->m_SensorInfo = _SensorInfo;
				try
				{
					auto WriteTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionWrite);

					auto DatabaseKey = Internal.f_GetDatabaseKey<NSensorStoreLocalDatabase::CSensorKey>(_SensorInfo);

					NSensorStoreLocalDatabase::CSensorValue DatabaseSensorInfo;
					DatabaseSensorInfo.m_Info = _SensorInfo;

					WriteTransaction.m_Transaction.f_Upsert(DatabaseKey, DatabaseSensorInfo);

					co_await Internal.m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
				}
				catch (CException const &_Exception)
				{
					DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor data to database: {}", _Exception);
					co_return DMibErrorInstance("Failed to store sensor in database, see log for error.");
				}
			}

			if (bWasChanged || bWasAdded)
			{
				auto ChangeInfoResult = co_await fg_CallSafe(Internal, &CInternal::f_SensorInfoChanged, SensorInfoKey, bWasChanged || bWasAdded).f_Wrap();
				if (!ChangeInfoResult)
					DMibLogWithCategory(SensorLocalStore, Error, "Failed to add/change sensor info in upstream: {}", ChangeInfoResult.f_GetExceptionStr());
			}
		}

		CDistributedAppSensorReporter::CSensorReporter Reporter;
		Reporter.m_LastSeenUniqueSequence = pSensor->m_LastSeenUniqueSequence;
		Reporter.m_fReportReadings =
			(
				g_ActorFunctor
				(
					g_ActorSubscription / [this, SensorInfoKey]() -> TCFuture<void>
					{
						auto &Internal = *mp_pInternal;

						CInternal::CSensor *pSensor = nullptr;
						auto OnResume = g_OnResume / [&]
							{
								pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
								if (!pSensor)
									DMibError("Sensor removed");
							}
						;

						if (--pSensor->m_ActiveRefCount == 0)
						{
							auto CanDestroyFuture = pSensor->m_pCanDestroy->f_Future();
							pSensor->m_pCanDestroy.f_Clear();
							co_await fg_Move(CanDestroyFuture);
							
							if (pSensor->m_ActiveRefCount == 0) // Might have been recreated
							{
								TCActorResultVector<void> Destroys;

								for (auto &Reporter : pSensor->m_SensorReporters)
								{
									Reporter.m_WriteSequencer.f_Abort() > Destroys.f_AddResult();
									if (Reporter.m_Reporter.m_fReportReadings)
										fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Destroys.f_AddResult();
								}

								pSensor->m_SensorReporters.f_Clear();
								co_await Destroys.f_GetResults();
							}
						}

						co_return {};
					}
				)
				/ [this, SensorInfoKey, Key = Internal.f_GetDatabaseKey<NSensorStoreLocalDatabase::CSensorReadingKey>(_SensorInfo), AllowDestroy = g_AllowWrongThreadDestroy]
				(TCVector<CDistributedAppSensorReporter::CSensorReading> &&_Readings) mutable -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;

					auto *pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
					if (!pSensor)
						co_return {};

					TCOptional<CCanDestroyTracker::CCanDestroyResultFunctor> CanDestroy;
					if (pSensor->m_pCanDestroy)
						CanDestroy = pSensor->m_pCanDestroy->f_Track();

					auto [DatabaseResult, UpstreamResult] = co_await
						(
							(
								self / [=]() mutable -> TCFuture<void>
								{
									auto &Internal = *mp_pInternal;

									auto *pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
									if (!pSensor)
										co_return {};

									try
									{
										auto WriteTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionWrite);

										for (auto &Reading : _Readings)
										{
											if (Reading.m_UniqueSequence == TCLimitsInt<uint64>::mc_Max)
												Key.m_UniqueSequence = ++pSensor->m_LastSeenUniqueSequence;
											else
												Key.m_UniqueSequence = Reading.m_UniqueSequence;

											if (WriteTransaction.m_Transaction.f_Exists(Key))
												continue; // Already reported

											NSensorStoreLocalDatabase::CSensorReadingValue Value;
											Value.m_Timestamp = Reading.m_Timestamp;
											Value.m_Data = Reading.m_Data;

											WriteTransaction.m_Transaction.f_Insert(Key, Value);

											NSensorStoreLocalDatabase::CSensorReadingByTime ByTime;
											ByTime.m_DbPrefix = Key.m_DbPrefix;
											ByTime.m_Prefix = NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix;
											ByTime.m_Timestamp = Reading.m_Timestamp;
											ByTime.m_UniqueSequence = Key.m_UniqueSequence;

											ByTime.m_HostID = Key.m_HostID;
											ByTime.m_ApplicationName = Key.m_ApplicationName;
											ByTime.m_Identifier = Key.m_Identifier;
											ByTime.m_IdentifierScope = Key.m_IdentifierScope;

											WriteTransaction.m_Transaction.f_Insert(ByTime, NSensorStoreLocalDatabase::CSensorReadingByTimeValue());
										}

										co_await Internal.m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
									}
									catch (CException const &_Exception)
									{
										DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor data to database: {}", _Exception);
										co_return _Exception.f_ExceptionPointer();
									}

									co_return {};
								}
								+ fg_CallSafe(Internal, &CInternal::f_NewSensorReadings, SensorInfoKey, _Readings)
							)
							.f_Wrap()
						)
					;


					if (!UpstreamResult)
					{
						auto ExceptionStr = UpstreamResult.f_GetExceptionStr();
						DMibLogWithCategory(SensorLocalStore, Error, "Error sending sensor readings to upstream: {}", ExceptionStr);
					}

					if (!DatabaseResult)
					{
						auto ExceptionStr = DatabaseResult.f_GetExceptionStr();
						DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor readings to database: {}", ExceptionStr);
						co_return DMibErrorInstance("Failed to store sensor readings in database, see log for error.");
					}

					co_return {};
				}
			)
		;

		co_return fg_Move(Reporter);
	}
}
