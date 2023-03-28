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

	auto CDistributedAppSensorStoreLocal::f_GetSensors(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
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

			if (!NSensorStore::fg_FilterSensorKey(Key, _Filter))
				continue;

			Batch.f_Insert(fg_Move(Value.m_Info));

			if (Batch.f_GetLen() >= _BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

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

		bool bReportNewestFirst = _Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst;

		if (bReportNewestFirst)
		{
			if (_Filter.m_MaxTimestamp)
			{
				if (_Filter.m_MaxSequence)
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *_Filter.m_MaxTimestamp, *_Filter.m_MaxSequence);
				else
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *_Filter.m_MaxTimestamp);

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
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *_Filter.m_MinTimestamp, *_Filter.m_MinSequence);
				else
					iReadingByTime.f_FindLowerBound(Prefix, CSensorReadingByTime::mc_Prefix, *_Filter.m_MinTimestamp);
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

			if (!NSensorStore::fg_FilterSensorKey(KeyByTime, _Filter.m_SensorFilter))
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
			CSensorReadingValue Value;
			if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
			{
				DMibLogWithCategory(SensorLocalStore, Error, "Missing sensor reading: {}", Key);
				continue;
			}

			auto *pSensor = Internal.m_Sensors.f_FindEqual(Key.f_SensorInfoKey());

			if (pSensor)
			{
				if (!NSensorStore::fg_FilterSensorReadingValueWithSensor(*pSensor, Value, _Filter.m_Flags))
					continue;
			}
			else
			{
				if (!NSensorStore::fg_FilterSensorReadingValue(Value, _Filter.m_Flags, Key, FilterContext))
					continue;
			}

			Batch.f_Insert(CDistributedAppSensorReader_SensorKeyAndReading{fg_Move(Key).f_SensorInfoKey(), fg_Move(Value).f_Reading(Key)});

			if (Batch.f_GetLen() >= _BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::f_GetSensorStatus(CDistributedAppSensorReader_SensorStatusFilter &&_Filter, uint32 _BatchSize)
		-> TCAsyncGenerator<TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		NTime::CTime Now;
		if (_Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems)
			Now = NTime::CTime::fs_NowUTC();

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

			if (!NSensorStore::fg_FilterSensorKey(Key, _Filter.m_SensorFilter))
				continue;

			CSensorKey ReadingKey = Key;
			ReadingKey.m_Prefix = CSensorReadingKey::mc_Prefix;

			auto iReading = ReadTransaction.m_Transaction.f_ReadCursor(ReadingKey);

			if (!iReading.f_Last())
				continue;

			auto ReadingValue = iReading.f_Value<CSensorReadingValue>();

			if (!NSensorStore::fg_FilterSensorValueStatus(Value, ReadingValue, _Filter.m_Flags, Now))
				continue;

			Batch.f_Insert
				(
					CDistributedAppSensorReader_SensorKeyAndReading
					{
						fg_Move(Key).f_SensorInfoKey()
						, fg_Move(ReadingValue).f_Reading(iReading.f_Key<CSensorReadingKey>())
					}
				)
			;

			if (Batch.f_GetLen() >= _BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}
}
