// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.h"

namespace NMib::NConcurrency
{
	auto CDistributedAppLogStoreLocal::f_GetLogs(CDistributedAppLogReader_LogFilter &&_Filter, uint32 _BatchSize)
		-> TCAsyncGenerator<TCVector<CDistributedAppLogReporter::CLogInfo>>
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

		for (auto iLog = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, NLogStoreLocalDatabase::CLogKey::mc_Prefix); iLog; ++iLog)
		{
			auto Key = iLog.f_Key<NLogStoreLocalDatabase::CLogKey>();
			auto Value = iLog.f_Value<NLogStoreLocalDatabase::CLogValue>();

			if (!NLogStore::fg_FilterLogKey(Key, _Filter))
				continue;

			Batch.f_Insert(fg_Move(Value.m_Info));

			if (Batch.f_GetLen() >= _BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}

	auto CDistributedAppLogStoreLocal::f_GetLogEntries
		(
			CDistributedAppLogReader_LogEntryFilter &&_Filter
			, uint32 _BatchSize
		)
		-> TCAsyncGenerator<TCVector<CDistributedAppLogReader_LogKeyAndEntry>>
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

		auto iEntryByTime = ReadTransaction.m_Transaction.f_ReadCursor(Prefix, NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix);

		bool bReportNewestFirst = _Filter.m_Flags & CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_ReportNewestFirst;
		bool bIsDataFiltered = _Filter.m_LogDataFilter.f_IsFiltered();

		if (bReportNewestFirst)
		{
			if (_Filter.m_MaxTimestamp)
			{
				if (_Filter.m_MaxSequence)
					iEntryByTime.f_FindLowerBound(Prefix, NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix, *_Filter.m_MaxTimestamp, *_Filter.m_MaxSequence);
				else
					iEntryByTime.f_FindLowerBound(Prefix, NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix, *_Filter.m_MaxTimestamp);

				if (!iEntryByTime)
					iEntryByTime.f_Last();
			}
			else
				iEntryByTime.f_Last();
		}
		else
		{
			if (_Filter.m_MinTimestamp)
			{
				if (_Filter.m_MinSequence)
					iEntryByTime.f_FindLowerBound(Prefix, NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix, *_Filter.m_MinTimestamp, *_Filter.m_MinSequence);
				else
					iEntryByTime.f_FindLowerBound(Prefix, NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix, *_Filter.m_MinTimestamp);
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

		for (; iEntryByTime; bReportNewestFirst ? --iEntryByTime : ++iEntryByTime)
		{
			auto KeyByTime = iEntryByTime.f_Key<NLogStoreLocalDatabase::CLogEntryByTime>();

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

				if (!Batch.f_IsEmpty())
				{
					co_yield fg_Move(Batch);
					nBytesInBatch = 0;
				}

				co_await g_Yield;
			}

			if (!NLogStore::fg_FilterLogKey(KeyByTime, _Filter.m_LogFilter))
				continue;

			auto Key = KeyByTime.f_Key();

			NLogStoreLocalDatabase::CLogEntryValue Value;
			if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
			{
				DMibLogWithCategory(LogLocalStore, Error, "Missing log reading: {}", Key);
				continue;
			}

			if (bIsDataFiltered && !NLogStore::	fg_FilterLogValue(Value.m_Data, _Filter.m_LogDataFilter))
				continue;

			CDistributedAppLogReader_LogKeyAndEntry LogKeyAndEntry{fg_Move(Key).f_LogInfoKey(), fg_Move(Value).f_Entry(Key)};

			mint ThisTime = NStream::fg_GetBinaryStreamSize(LogKeyAndEntry);

			if ((nBytesInBatch + ThisTime > CActorDistributionManager::mc_HalfMaxMessageSize) && !Batch.f_IsEmpty())
			{
				co_yield fg_Move(Batch);
				nBytesInBatch = 0;
			}

			Batch.f_Insert(fg_Move(LogKeyAndEntry));
			nBytesInBatch += ThisTime;

			if (Batch.f_GetLen() >= _BatchSize)
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
