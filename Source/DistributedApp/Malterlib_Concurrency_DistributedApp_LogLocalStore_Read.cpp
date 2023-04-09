// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	auto CDistributedAppLogStoreLocal::f_GetLogs(CDistributedAppLogReader::CGetLogs &&_Params) -> TCAsyncGenerator<TCVector<CDistributedAppLogReporter::CLogInfo>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto CaptureScope = co_await
			(
				g_CaptureExceptions %
				(
					fg_LogCritical("LogLocalStore", "Error reading log data from database (f_GetLogs)")
					% "Internal error reading from database, see log"
				)
			)
		;

		auto ReadTransactionWrapped = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead).f_Wrap();
		auto &ReadTransaction = *ReadTransactionWrapped; // To force exception to be thrown here

		TCVector<CDistributedAppLogReporter::CLogInfo> Batch;

		NLogStore::CFilterLogKeyContext FilterContext{.m_Transaction = ReadTransaction.m_Transaction, .m_ThisHostID = Internal.m_ThisHostID, .m_Prefix = Internal.m_Prefix};

		for (auto iLog = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, CLogKey::mc_Prefix); iLog; ++iLog)
		{
			auto Key = iLog.f_Key<CLogKey>();
			auto Value = iLog.f_Value<CLogValue>();

			if (!NLogStore::fg_FilterLogKey(Key, _Params.m_Filters, FilterContext, &Value.m_Info))
				continue;

			Batch.f_Insert(fg_Move(Value.m_Info));

			if (Batch.f_GetLen() >= _Params.m_BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}

	auto CDistributedAppLogStoreLocal::f_GetLogEntries(CDistributedAppLogReader::CGetLogEntries &&_Params) -> TCAsyncGenerator<TCVector<CDistributedAppLogReader_LogKeyAndEntry>>
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
					fg_LogCritical("LogLocalStore", "Error reading log data from database (f_GetLogEntries)")
					% "Internal error reading from database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWrapped; // To force exception to be thrown here
		TCVector<CDistributedAppLogReader_LogKeyAndEntry> Batch;

		auto iEntryByTime = ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CLogEntryByTime::mc_Prefix);

		TCVector<CInternal::CEntryFilter> Filters;
		for (auto &Filter : _Params.m_Filters)
		{
			auto &OutFilter = Filters.f_Insert();
			OutFilter.m_Filter = fg_Move(Filter);
			OutFilter.m_bIsDataFiltered = OutFilter.m_Filter.m_LogDataFilter.f_IsFiltered();
		}

		NStorage::TCOptional<uint64> MinSequence;
		NStorage::TCOptional<uint64> MaxSequence;

		NStorage::TCOptional<NTime::CTime> MinTimestamp;
		NStorage::TCOptional<NTime::CTime> MaxTimestamp;

		bool bReportNewestFirst = true;
		{
			bool bFirst = true;
			for (auto &Filter : Filters)
			{
				bool bReportNewestFirstValue = !!(Filter.m_Filter.m_Flags & CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_ReportNewestFirst);
				if (bFirst)
				{
					bFirst = false;
					bReportNewestFirst = bReportNewestFirstValue;

					MinSequence = Filter.m_Filter.m_MinSequence;
					MaxSequence = Filter.m_Filter.m_MaxSequence;

					MinTimestamp = Filter.m_Filter.m_MinTimestamp;
					MaxTimestamp = Filter.m_Filter.m_MaxTimestamp;
					continue;
				}

				if (bReportNewestFirstValue != bReportNewestFirst)
					co_return DMibErrorInstance("ESensorReadingsFlag_ReportNewestFirst cannot be varied per filter");
				if (MinSequence != Filter.m_Filter.m_MinSequence)
					co_return DMibErrorInstance("MinSequence cannot be varied per filter");
				if (MaxSequence != Filter.m_Filter.m_MaxSequence)
					co_return DMibErrorInstance("MaxSequence cannot be varied per filter");
				if (MinTimestamp != Filter.m_Filter.m_MinTimestamp)
					co_return DMibErrorInstance("MinTimestamp cannot be varied per filter");
				if (MaxTimestamp != Filter.m_Filter.m_MaxTimestamp)
					co_return DMibErrorInstance("MaxTimestamp cannot be varied per filter");
			}
		}

		if (bReportNewestFirst)
		{
			if (MaxTimestamp)
			{
				if (MaxSequence)
					iEntryByTime.f_FindLowerBound(Prefix, CLogEntryByTime::mc_Prefix, *MaxTimestamp, *MaxSequence);
				else
					iEntryByTime.f_FindLowerBound(Prefix, CLogEntryByTime::mc_Prefix, *MaxTimestamp);

				if (!iEntryByTime)
					iEntryByTime.f_Last();
			}
			else
				iEntryByTime.f_Last();
		}
		else
		{
			if (MinTimestamp)
			{
				if (MinSequence)
					iEntryByTime.f_FindLowerBound(Prefix, CLogEntryByTime::mc_Prefix, *MinTimestamp, *MinSequence);
				else
					iEntryByTime.f_FindLowerBound(Prefix, CLogEntryByTime::mc_Prefix, *MinTimestamp);
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

		mint nBytesInBatch = 0;

		NLogStore::CFilterLogKeyContext FilterContext{.m_Transaction = ReadTransaction.m_Transaction, .m_ThisHostID = Internal.m_ThisHostID, .m_Prefix = Internal.m_Prefix};

		for (; iEntryByTime; bReportNewestFirst ? --iEntryByTime : ++iEntryByTime)
		{
			auto KeyByTime = iEntryByTime.f_Key<CLogEntryByTime>();

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

				if (!Batch.f_IsEmpty())
				{
					co_yield fg_Move(Batch);
					nBytesInBatch = 0;
				}

				co_await g_Yield;
			}

			auto Key = KeyByTime.f_Key();

			auto *pLog = Internal.m_Logs.f_FindEqual(Key.f_LogInfoKey());

			CLogEntryValue Value;
			if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
			{
				DMibLogWithCategory(LogLocalStore, Error, "Missing log reading: {}", Key);
				continue;
			}

			{
				bool bPassFilter = Filters.f_IsEmpty();

				for (auto &Filter : Filters)
				{
					if (!NLogStore::fg_FilterLogKey(KeyByTime, Filter.m_Filter.m_LogFilter, FilterContext, pLog ? &pLog->m_Info : nullptr))
						continue;

					if (Filter.m_bIsDataFiltered && !NLogStore::fg_FilterLogValue(Value.m_Data, Filter.m_Filter.m_LogDataFilter))
						continue;

					bPassFilter = true;
					break;
				}

				if (!bPassFilter)
					continue;
			}

			CDistributedAppLogReader_LogKeyAndEntry LogKeyAndEntry{fg_Move(Key).f_LogInfoKey(), fg_Move(Value).f_Entry(Key)};

			mint ThisTime = NStream::fg_GetBinaryStreamSize(LogKeyAndEntry);

			if ((nBytesInBatch + ThisTime > CActorDistributionManager::mc_HalfMaxMessageSize) && !Batch.f_IsEmpty())
			{
				co_yield fg_Move(Batch);
				nBytesInBatch = 0;
			}

			Batch.f_Insert(fg_Move(LogKeyAndEntry));
			nBytesInBatch += ThisTime;

			if (Batch.f_GetLen() >= _Params.m_BatchSize)
			{
				co_yield fg_Move(Batch);
				nBytesInBatch = 0;
			}
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}
}
