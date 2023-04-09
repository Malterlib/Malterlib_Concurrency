// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Filter.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	auto CDistributedAppSensorStoreLocal::f_GetSensors(CDistributedAppSensorReader::CGetSensors &&_Params)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReporter::CSensorInfo>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto ReadTransactionWapped = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead).f_Wrap();

		auto CaptureScope = co_await
			(
				g_CaptureExceptions %
				(
					fg_LogCritical("SensorLocalStore", "Error reading sensor data from database (f_GetSensors)")
					% "Internal error reading from database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWapped; // To force exception to be thrown here

		TCVector<CDistributedAppSensorReporter::CSensorInfo> Batch;

		for (auto iSensor = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, CSensorKey::mc_Prefix); iSensor; ++iSensor)
		{
			auto Key = iSensor.f_Key<CSensorKey>();
			auto Value = iSensor.f_Value<CSensorValue>();

			if (!NSensorStore::fg_FilterSensorKey(Key, _Params.m_Filters))
				continue;

			Batch.f_Insert(fg_Move(Value.m_Info));

			if (Batch.f_GetLen() >= _Params.m_BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensorReadings(CDistributedAppSensorReader::CGetSensorReadings &&_Params)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		NStorage::TCOptional<uint64> MinSequence;
		NStorage::TCOptional<uint64> MaxSequence;

		NStorage::TCOptional<NTime::CTime> MinTimestamp;
		NStorage::TCOptional<NTime::CTime> MaxTimestamp;

		bool bReportNewestFirst = true;
		{
			bool bFirst = true;
			for (auto &Filter : _Params.m_Filters)
			{
				bool bReportNewestFirstValue = !!(Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst);
				if (bFirst)
				{
					bFirst = false;
					bReportNewestFirst = bReportNewestFirstValue;

					MinSequence = Filter.m_MinSequence;
					MaxSequence = Filter.m_MaxSequence;

					MinTimestamp = Filter.m_MinTimestamp;
					MaxTimestamp = Filter.m_MaxTimestamp;
					continue;
				}

				if (bReportNewestFirstValue != bReportNewestFirst)
					co_return DMibErrorInstance("ESensorReadingsFlag_ReportNewestFirst cannot be varied per filter");
				if (MinSequence != Filter.m_MinSequence)
					co_return DMibErrorInstance("MinSequence cannot be varied per filter");
				if (MaxSequence != Filter.m_MaxSequence)
					co_return DMibErrorInstance("MaxSequence cannot be varied per filter");
				if (MinTimestamp != Filter.m_MinTimestamp)
					co_return DMibErrorInstance("MinTimestamp cannot be varied per filter");
				if (MaxTimestamp != Filter.m_MaxTimestamp)
					co_return DMibErrorInstance("MaxTimestamp cannot be varied per filter");
			}
		}

		auto ReadTransactionWrapped = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead).f_Wrap();

		auto Prefix = Internal.m_Prefix;

		co_await fg_ContinueRunningOnActor(fg_ConcurrentActor());

		// From here on you can't access Internal

		auto CaptureScope = co_await
			(
				g_CaptureExceptions %
				(
					fg_LogCritical("SensorLocalStore", "Error reading sensor data from database (f_GetSensorReadings)")
					% "Internal error reading from database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWrapped; // To force exception to be thrown here

		TCVector<CDistributedAppSensorReader_SensorKeyAndReading> Batch;

		auto iReadingByTime = ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CSensorReadingByTime::mc_Prefix);

		if (bReportNewestFirst)
		{
			if (MaxTimestamp)
			{
				if (MaxSequence)
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *MaxTimestamp, *MaxSequence);
				else
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *MaxTimestamp);

				if (!iReadingByTime)
					iReadingByTime.f_Last();
			}
			else
				iReadingByTime.f_Last();
		}
		else
		{
			if (MinTimestamp)
			{
				if (MinSequence)
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *MinTimestamp, *MinSequence);
				else
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *MinTimestamp);
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

		NSensorStore::CFilterSensorKeyContext FilterContext{.m_Transaction = ReadTransaction.m_Transaction};

		for (; iReadingByTime; bReportNewestFirst ? --iReadingByTime : ++iReadingByTime)
		{
			auto KeyByTime = iReadingByTime.f_Key<CSensorReadingByTime>();

			if (bReportNewestFirst)
			{
				if (MinTimestamp)
				{
					auto &MinTimestampValue = *MinTimestamp;

					if (KeyByTime.m_Timestamp < MinTimestampValue)
						break;
					else if (KeyByTime.m_Timestamp == MinTimestampValue && MinSequence)
					{
						if (KeyByTime.m_UniqueSequence < *MinSequence)
							break;
					}
				}
			}
			else
			{
				if (MaxTimestamp)
				{
					auto &MaxTimestampValue = *MaxTimestamp;

					if (KeyByTime.m_Timestamp > MaxTimestampValue)
						break;
					else if (KeyByTime.m_Timestamp == MaxTimestampValue && MaxSequence)
					{
						if (KeyByTime.m_UniqueSequence > *MaxSequence)
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

			if (MinSequence && KeyByTime.m_UniqueSequence < *MinSequence)
				continue;
			if (MaxSequence && KeyByTime.m_UniqueSequence > *MaxSequence)
				continue;

			if (MinTimestamp && KeyByTime.m_Timestamp < *MinTimestamp)
				continue;
			if (MaxTimestamp && KeyByTime.m_Timestamp > *MaxTimestamp)
				continue;

			auto Key = KeyByTime.f_Key();
			CSensorReadingValue Value;
			if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
			{
				DMibLogWithCategory(SensorLocalStore, Error, "Missing sensor reading: {}", Key);
				continue;
			}

			auto *pSensor = Internal.m_Sensors.f_FindEqual(Key.f_SensorInfoKey());

			{
				bool bPassFilter = _Params.m_Filters.f_IsEmpty();

				for (auto &Filter : _Params.m_Filters)
				{
					if (!NSensorStore::fg_FilterSensorKey(KeyByTime, Filter.m_SensorFilter))
						continue;

					if (pSensor)
					{
						if (!NSensorStore::fg_FilterSensorReadingValueWithSensor(*pSensor, Value, Filter.m_Flags))
							continue;
					}
					else
					{
						if (!NSensorStore::fg_FilterSensorReadingValue(Value, Filter.m_Flags, Key, FilterContext))
							continue;
					}

					bPassFilter = true;
					break;
				}

				if (!bPassFilter)
					continue;
			}

			Batch.f_Insert(CDistributedAppSensorReader_SensorKeyAndReading{fg_Move(Key).f_SensorInfoKey(), fg_Move(Value).f_Reading(Key)});

			if (Batch.f_GetLen() >= _Params.m_BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensorStatus(CDistributedAppSensorReader::CGetSensorStatus &&_Params)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		NTime::CTime Now;
		for (auto &Filter : _Params.m_Filters)
		{
			if (Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems)
			{
				Now = NTime::CTime::fs_NowUTC();
				break;
			}
		}

		auto ReadTransactionWrapped = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead).f_Wrap();

		auto CaptureScope = co_await
			(
				g_CaptureExceptions %
				(
					fg_LogCritical("SensorLocalStore", "Error reading sensor data from database (f_GetSensorStatus)")
					% "Internal error reading from database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWrapped; // To force exception to be thrown here

		TCVector<CDistributedAppSensorReader_SensorKeyAndReading> Batch;

		for (auto iSensor = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, CSensorKey::mc_Prefix); iSensor; ++iSensor)
		{
			auto Key = iSensor.f_Key<CSensorKey>();
			auto Value = iSensor.f_Value<CSensorValue>();

			CSensorKey ReadingKey = Key;
			ReadingKey.m_Prefix = CSensorReadingKey::mc_Prefix;

			auto iReading = ReadTransaction.m_Transaction.f_ReadCursor(ReadingKey);

			if (!iReading.f_Last())
				continue;

			auto ReadingValue = iReading.f_Value<CSensorReadingValue>();

			{
				bool bPassFilter = _Params.m_Filters.f_IsEmpty();

				for (auto &Filter : _Params.m_Filters)
				{
					if (!NSensorStore::fg_FilterSensorKey(Key, Filter.m_SensorFilter))
						continue;

					if (!NSensorStore::fg_FilterSensorValueStatus(Value, ReadingValue, Filter.m_Flags, Now))
						continue;

					bPassFilter = true;
					break;
				}

				if (!bPassFilter)
					continue;
			}

			Batch.f_Insert
				(
					CDistributedAppSensorReader_SensorKeyAndReading
					{
						fg_Move(Key).f_SensorInfoKey()
						, fg_Move(ReadingValue).f_Reading(iReading.f_Key<CSensorReadingKey>())
					}
				)
			;

			if (Batch.f_GetLen() >= _Params.m_BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}
}
