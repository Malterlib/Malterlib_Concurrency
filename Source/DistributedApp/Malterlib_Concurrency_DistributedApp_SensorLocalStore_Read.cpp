// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency
{
	namespace
	{
		template <typename tf_CKey>
		bool fg_FilterSensorKey(tf_CKey const &_Key, CDistributedAppSensorReader_SensorFilter const &_Filter)
		{
			if (_Filter.m_HostID && _Key.m_HostID != *_Filter.m_HostID)
				return false;

			if (_Filter.m_Scope)
			{
				switch (_Filter.m_Scope->f_GetTypeID())
				{
				case CDistributedAppSensorReporter::ESensorScope_Unsupported:
					{
						return false;
					}
					break;
				case CDistributedAppSensorReporter::ESensorScope_Host:
					{
						if (!_Key.m_ApplicationName.f_IsEmpty())
							return false;
					}
					break;
				case CDistributedAppSensorReporter::ESensorScope_Application:
					{
						auto &Application = _Filter.m_Scope->f_Get<CDistributedAppSensorReporter::ESensorScope_Application>();
						if (_Key.m_ApplicationName != Application.m_ApplicationName)
							return false;
					}
					break;
				}
			}

			if (_Filter.m_Identifier && _Key.m_Identifier != *_Filter.m_Identifier)
				return false;

			if (_Filter.m_IdentifierScope && _Key.m_IdentifierScope != *_Filter.m_IdentifierScope)
				return false;

			return true;
		}
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensors(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReporter::CSensorInfo>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		try
		{
			auto ReadTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead);

			TCVector<CDistributedAppSensorReporter::CSensorInfo> Batch;

			for (auto iSensor = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, NSensorStoreLocalDatabase::CSensorKey::mc_Prefix); iSensor; ++iSensor)
			{
				auto Key = iSensor.f_Key<NSensorStoreLocalDatabase::CSensorKey>();
				auto Value = iSensor.f_Value<NSensorStoreLocalDatabase::CSensorValue>();

				if (!fg_FilterSensorKey(Key, _Filter))
					continue;

				Batch.f_Insert(fg_Move(Value.m_Info));

				if (Batch.f_GetLen() >= _BatchSize)
					co_yield fg_Move(Batch);
			}

			if (!Batch.f_IsEmpty())
				co_yield fg_Move(Batch);
		}
		catch ([[maybe_unused]] CException const &_Exception)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error reading sensor data from database (f_GetSensors): {}", _Exception);
			co_return DMibErrorInstance("Internal error reading from database, see log");
		}

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensorReadings
		(
			CDistributedAppSensorReader_SensorReadingFilter &&_Filter
			, uint32 _BatchSize
		)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto ReadTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead);

		auto Prefix = Internal.m_Prefix;

		co_await fg_ContinueRunningOnActor(fg_ConcurrentActor());

		// From here on you can't access Internal

		try
		{
			TCVector<CDistributedAppSensorReader_SensorKeyAndReading> Batch;

			auto iReadingByTime = ReadTransaction.m_Transaction.f_ReadCursor(Prefix, NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix);

			bool bReportNewestFirst = _Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst;

			if (bReportNewestFirst)
			{
				if (_Filter.m_MaxTimestamp)
				{
					if (_Filter.m_MaxSequence)
						iReadingByTime.f_FindLowerBound(Prefix, NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix, *_Filter.m_MaxTimestamp, *_Filter.m_MaxSequence);
					else
						iReadingByTime.f_FindLowerBound(Prefix, NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix, *_Filter.m_MaxTimestamp);

					if (!iReadingByTime)
						iReadingByTime.f_Last();
				}
				else
					iReadingByTime.f_Last();
			}
			else
			{
				if (_Filter.m_MinTimestamp)
				{
					if (_Filter.m_MinSequence)
						iReadingByTime.f_FindLowerBound(Prefix, NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix, *_Filter.m_MinTimestamp, *_Filter.m_MinSequence);
					else
						iReadingByTime.f_FindLowerBound(Prefix, NSensorStoreLocalDatabase::CSensorReadingByTime::mc_Prefix, *_Filter.m_MinTimestamp);
				}
			}

			NTime::CCyclesClock YieldClock(true);

			auto OnResume = co_await fg_OnResume
				(
					[&]() -> CExceptionPointer
					{
						YieldClock.f_Start();
						return {};
					}
				)
			;

			for (; iReadingByTime; bReportNewestFirst ? --iReadingByTime : ++iReadingByTime)
			{
				NSensorStoreLocalDatabase::CSensorReadingValue Value;
				auto KeyByTime = iReadingByTime.f_Key<NSensorStoreLocalDatabase::CSensorReadingByTime>();

				if (bReportNewestFirst)
				{
					if (_Filter.m_MinTimestamp)
					{
						auto &MinTimestamp = *_Filter.m_MinTimestamp;

						if (KeyByTime.m_Timestamp < MinTimestamp)
							break;
						else if (KeyByTime.m_Timestamp == MinTimestamp && _Filter.m_MinSequence)
						{
							if (KeyByTime.m_UniqueSequence < *_Filter.m_MinSequence)
								break;
						}
					}
				}
				else
				{
					if (_Filter.m_MaxTimestamp)
					{
						auto &MaxTimestamp = *_Filter.m_MaxTimestamp;

						if (KeyByTime.m_Timestamp > MaxTimestamp)
							break;
						else if (KeyByTime.m_Timestamp == MaxTimestamp && _Filter.m_MaxSequence)
						{
							if (KeyByTime.m_UniqueSequence > *_Filter.m_MaxSequence)
								break;
						}
					}
				}

				if (YieldClock.f_GetTime() > 0.1)
				{
					if (co_await g_bShouldAbort)
						co_return DMibErrorInstance("Aborted");

					if (Batch.f_GetLen() > 0)
						co_yield fg_Move(Batch);

					co_await g_Yield;
				}

				if (!fg_FilterSensorKey(KeyByTime, _Filter.m_SensorFilter))
					continue;

				if (_Filter.m_MinSequence && KeyByTime.m_UniqueSequence < *_Filter.m_MinSequence)
					continue;
				if (_Filter.m_MaxSequence && KeyByTime.m_UniqueSequence > *_Filter.m_MaxSequence)
					continue;

				if (_Filter.m_MinTimestamp && KeyByTime.m_Timestamp < *_Filter.m_MinTimestamp)
					continue;
				if (_Filter.m_MaxTimestamp && KeyByTime.m_Timestamp > *_Filter.m_MaxTimestamp)
					continue;

				auto Key = KeyByTime.f_Key();
				if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
				{
					DMibLogWithCategory(SensorLocalStore, Error, "Missing sensor reading: {}", Key);
					continue;
				}

				Batch.f_Insert(CDistributedAppSensorReader_SensorKeyAndReading{fg_Move(Key).f_SensorInfoKey(), fg_Move(Value).f_Reading(Key)});

				if (Batch.f_GetLen() >= _BatchSize)
					co_yield fg_Move(Batch);
			}

			if (!Batch.f_IsEmpty())
				co_yield fg_Move(Batch);
		}
		catch ([[maybe_unused]] CException const &_Exception)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error reading sensor data from database (f_GetSensorReadings): {}", _Exception);
			co_return DMibErrorInstance("Internal error reading from database, see log");
		}

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensorStatus(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		try
		{
			auto ReadTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead);

			TCVector<CDistributedAppSensorReader_SensorKeyAndReading> Batch;

			for (auto iSensor = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, NSensorStoreLocalDatabase::CSensorKey::mc_Prefix); iSensor; ++iSensor)
			{
				auto Key = iSensor.f_Key<NSensorStoreLocalDatabase::CSensorKey>();
				auto Value = iSensor.f_Value<NSensorStoreLocalDatabase::CSensorValue>();

				if (!fg_FilterSensorKey(Key, _Filter))
					continue;

				NSensorStoreLocalDatabase::CSensorKey ReadingKey = Key;
				ReadingKey.m_Prefix = NSensorStoreLocalDatabase::CSensorReadingKey::mc_Prefix;

				auto iReading = ReadTransaction.m_Transaction.f_ReadCursor(ReadingKey);

				if (!iReading.f_Last())
					continue;

				Batch.f_Insert
					(
						CDistributedAppSensorReader_SensorKeyAndReading
						{
							fg_Move(Key).f_SensorInfoKey()
							, fg_Move(iReading.f_Value<NSensorStoreLocalDatabase::CSensorReadingValue>().f_Reading(iReading.f_Key<NSensorStoreLocalDatabase::CSensorReadingKey>()))
						}
					)
				;

				if (Batch.f_GetLen() >= _BatchSize)
					co_yield fg_Move(Batch);
			}

			if (!Batch.f_IsEmpty())
				co_yield fg_Move(Batch);
		}
		catch ([[maybe_unused]] CException const &_Exception)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error reading sensor data from database (f_GetSensorStatus): {}", _Exception);
			co_return DMibErrorInstance("Internal error reading from database, see log");
		}

		co_return {};
	}
}
