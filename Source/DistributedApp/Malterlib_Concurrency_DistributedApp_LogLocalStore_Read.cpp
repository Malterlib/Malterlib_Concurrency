// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency
{
	namespace
	{
		template <typename tf_CKey>
		bool fg_FilterLogKey(tf_CKey const &_Key, CDistributedAppLogReader_LogFilter const &_Filter)
		{
			if (_Filter.m_HostID && _Key.m_HostID != *_Filter.m_HostID)
				return false;

			if (_Filter.m_Scope)
			{
				switch (_Filter.m_Scope->f_GetTypeID())
				{
				case CDistributedAppLogReporter::ELogScope_Unsupported:
					{
						return false;
					}
					break;
				case CDistributedAppLogReporter::ELogScope_Host:
					{
						if (!_Key.m_ApplicationName.f_IsEmpty())
							return false;
					}
					break;
				case CDistributedAppLogReporter::ELogScope_Application:
					{
						auto &Application = _Filter.m_Scope->f_Get<CDistributedAppLogReporter::ELogScope_Application>();
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

		bool fg_MatchMultipleValuesWildcards(NContainer::TCVector<NStr::CStr> const &_Values, NContainer::TCSet<NStr::CStr> const &_Wildcards)
		{
			bool bMatched = false;
			for (auto &Category : _Values)
			{
				if (fg_StrMatchesAnyWildcardInContainer(Category, _Wildcards))
				{
					bMatched = true;
					break;
				}
			}

			return bMatched;
		}

		bool fg_FilterLogValue(CDistributedAppLogReporter::CLogData const &_Value, CDistributedAppLogReader_LogDataFilter const &_Filter)
		{
			if (_Filter.m_Flags)
			{
				if (!(_Value.m_Flags & *_Filter.m_Flags))
					return false;
			}

			if (_Filter.m_Severities)
			{
				if (!_Filter.m_Severities->f_FindEqual(_Value.m_Severity))
					return false;
			}

			if (_Filter.m_CategoriesWildcards)
			{
				if (!fg_MatchMultipleValuesWildcards(_Value.m_Categories, *_Filter.m_CategoriesWildcards))
					return false;
			}

			if (_Filter.m_OperationsWildcards)
			{
				if (!fg_MatchMultipleValuesWildcards(_Value.m_Operations, *_Filter.m_OperationsWildcards))
					return false;
			}

			if (_Filter.m_MessageWildcards)
			{
				if (!fg_StrMatchesAnyWildcardInContainer(_Value.m_Message, *_Filter.m_MessageWildcards))
					return false;
			}

			if (_Filter.m_SourceLocationFileWildcards)
			{
				if (!_Value.m_SourceLocation)
					return false;

				if (!fg_StrMatchesAnyWildcardInContainer(_Value.m_SourceLocation->m_File, *_Filter.m_SourceLocationFileWildcards))
					return false;
			}

			return true;
		}
	}

	auto CDistributedAppLogStoreLocal::f_GetLogs(CDistributedAppLogReader_LogFilter &&_Filter, uint32 _BatchSize)
		-> TCAsyncGenerator<TCVector<CDistributedAppLogReporter::CLogInfo>>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		try
		{
			auto ReadTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead);

			TCVector<CDistributedAppLogReporter::CLogInfo> Batch;

			for (auto iLog = ReadTransaction.m_Transaction.f_ReadCursor(Internal.m_Prefix, NLogStoreLocalDatabase::CLogKey::mc_Prefix); iLog; ++iLog)
			{
				auto Key = iLog.f_Key<NLogStoreLocalDatabase::CLogKey>();
				auto Value = iLog.f_Value<NLogStoreLocalDatabase::CLogValue>();

				if (!fg_FilterLogKey(Key, _Filter))
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
			DMibLogWithCategory(LogLocalStore, Critical, "Error reading log data from database (f_GetLogs): {}", _Exception);
			co_return DMibErrorInstance("Internal error reading from database, see log");
		}

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

		auto ReadTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionRead);

		auto Prefix = Internal.m_Prefix;

		co_await fg_ContinueRunningOnActor(fg_ConcurrentActor());

		// From here on you can't access Internal

		try
		{

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

				if (!fg_FilterLogKey(KeyByTime, _Filter.m_LogFilter))
					continue;

				auto Key = KeyByTime.f_Key();

				NLogStoreLocalDatabase::CLogEntryValue Value;
				if (!ReadTransaction.m_Transaction.f_Get(Key, Value))
				{
					DMibLogWithCategory(LogLocalStore, Error, "Missing log reading: {}", Key);
					continue;
				}

				if (bIsDataFiltered && !fg_FilterLogValue(Value.m_Data, _Filter.m_LogDataFilter))
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
		}
		catch ([[maybe_unused]] CException const &_Exception)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error reading log data from database (f_GetLogEntries): {}", _Exception);
			co_return DMibErrorInstance("Internal error reading from database, see log");
		}

		co_return {};
	}
}
