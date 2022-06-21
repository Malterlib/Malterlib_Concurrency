// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_StoreLogEntries
		(
			CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
			, NLogStoreLocalDatabase::CLogEntryKey const &_DatabaseKey
			, TCVector<CDistributedAppLogReporter::CLogEntry> const &_Entries
		)
	{
		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
		if (!pLog)
			co_return {};

		auto pCanDestroyTracker = m_pCanDestroyStoringLocal;

		DMibLogOperation(DisableDistributedLogReporter); // Prevent recursive logs

		auto WriteTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionWrite);

		auto DatabaseKey = _DatabaseKey;

		try
		{
			auto SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
			smint BatchLimit = fg_Min(SizeStats.m_MapSize / (20 * 3), 4 * 1024 * 1024);
			auto SizeLimit = smint(SizeStats.m_MapSize) - fg_Min(fg_Max(smint(SizeStats.m_MapSize) / 20, BatchLimit * 3, 32 * smint(SizeStats.m_PageSize)), smint(SizeStats.m_MapSize) / 3);
			smint nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);

			for (auto &Entry : _Entries)
			{
				smint nBytesInserted = WriteTransaction.m_Transaction.f_SizeStatistics().m_UsedBytes - SizeStats.m_UsedBytes;
				bool bFreeUpSpace = nBytesInserted >= nBytesLimit;

				if (bFreeUpSpace)
					WriteTransaction = co_await fg_CallSafe(this, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

				if (nBytesInserted > BatchLimit || bFreeUpSpace)
				{
					co_await m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
					if (bFreeUpSpace)
						co_await m_Database(&CDatabaseActor::f_WaitForAllReaders);
					WriteTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionWrite);
					SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
					nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);
				}

				DatabaseKey.m_UniqueSequence = Entry.m_UniqueSequence;

				if (WriteTransaction.m_Transaction.f_Exists(DatabaseKey))
					continue; // Already reported

				NLogStoreLocalDatabase::CLogEntryValue Value;
				Value.m_Timestamp = Entry.m_Timestamp;
				Value.m_Data = Entry.m_Data;

				NLogStoreLocalDatabase::CLogEntryByTime ByTime;
				ByTime.m_DbPrefix = DatabaseKey.m_DbPrefix;
				ByTime.m_Prefix = NLogStoreLocalDatabase::CLogEntryByTime::mc_Prefix;
				ByTime.m_Timestamp = Entry.m_Timestamp;
				ByTime.m_UniqueSequence = DatabaseKey.m_UniqueSequence;

				ByTime.m_HostID = DatabaseKey.m_HostID;
				ByTime.m_ApplicationName = DatabaseKey.m_ApplicationName;
				ByTime.m_Identifier = DatabaseKey.m_Identifier;
				ByTime.m_IdentifierScope = DatabaseKey.m_IdentifierScope;

				WriteTransaction.m_Transaction.f_Insert(DatabaseKey, Value);
				WriteTransaction.m_Transaction.f_Insert(ByTime, NLogStoreLocalDatabase::CLogEntryByTimeValue());
			}

			co_await m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
		}
		catch ([[maybe_unused]] CException const &_Exception)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error saving log data to database: {}", _Exception);
			co_return _Exception.f_ExceptionPointer();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_CleanupLogReporter(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey)
	{
		CInternal::CLog *pLog = nullptr;
		auto OnResume = g_OnResume / [&]
			{
				pLog = m_Logs.f_FindEqual(_LogInfoKey);
				if (!pLog)
					DMibError("Log removed");
			}
		;

		if (--pLog->m_ActiveRefCount == 0)
		{
			auto CanDestroyFuture = pLog->m_pCanDestroy->f_Future();
			pLog->m_pCanDestroy.f_Clear();
			co_await fg_Move(CanDestroyFuture);

			if (pLog->m_ActiveRefCount == 0) // Might have been recreated
			{
				TCActorResultVector<void> Destroys;

				for (auto &Reporter : pLog->m_LogReporters)
				{
					Reporter.m_WriteSequencer.f_Abort() > Destroys.f_AddResult();
					if (Reporter.m_Reporter.m_fReportEntries)
						fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Destroys.f_AddResult();
				}

				pLog->m_LogReporters.f_Clear();
				co_await Destroys.f_GetResults();
			}
		}

		co_return {};
	}

	auto CDistributedAppLogStoreLocal::CInternal::f_GetReportEntriesFunctor
		(
			CDistributedAppLogReporter::CLogInfoKey _LogInfoKey
			, NLogStoreLocalDatabase::CLogEntryKey _DatabaseKey
		)
		-> TCActorFunctorWithID<TCFuture<CDistributedAppLogReporter::CReportEntriesResult> (NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> &&_Entries)>
	{
		return g_ActorFunctor
			(
				g_ActorSubscription / [this, _LogInfoKey]() -> TCFuture<void>
				{
					co_return co_await fg_CallSafe(this, &CInternal::f_CleanupLogReporter, _LogInfoKey);
				}
			)
			/ [this, _LogInfoKey, _DatabaseKey, AllowDestroy = g_AllowWrongThreadDestroy]
			(TCVector<CDistributedAppLogReporter::CLogEntry> &&_Entries) mutable -> TCFuture<CDistributedAppLogReporter::CReportEntriesResult>
			{
				auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
				if (!pLog)
					co_return {};

				TCOptional<CCanDestroyTracker::CCanDestroyResultFunctor> CanDestroy;
				if (pLog->m_pCanDestroy)
					CanDestroy = pLog->m_pCanDestroy->f_Track();

				DMibLogOperation(DisableDistributedLogReporter); // Prevent recursive logs

				auto SequenceSubscription = co_await pLog->m_LogSequencer.f_Sequence();

				for (auto &Entry : _Entries)
				{
					if (Entry.m_UniqueSequence == TCLimitsInt<uint64>::mc_Max)
						Entry.m_UniqueSequence = ++pLog->m_LastSeenUniqueSequence;
					else
						pLog->m_LastSeenUniqueSequence = fg_Max(pLog->m_LastSeenUniqueSequence, Entry.m_UniqueSequence);
				}

				auto [DatabaseResult, UpstreamResult] = co_await
					(
						fg_CallSafe(this, &CInternal::f_StoreLogEntries, _LogInfoKey, _DatabaseKey, _Entries)
						+ fg_CallSafe(this, &CInternal::f_NewLogEntries, _LogInfoKey, _Entries)
					)
					.f_Wrap()
				;

				CDistributedAppLogReporter::CReportEntriesResult ReportEntriesResult;
				ReportEntriesResult.m_ReportDepth = 1;
				if (!UpstreamResult)
				{
					auto ExceptionStr = UpstreamResult.f_GetExceptionStr();
					DMibLogWithCategory(LogLocalStore, Error, "Error sending log entries to upstream: {}", ExceptionStr);
				}
				else
					ReportEntriesResult.m_ReportDepth = *UpstreamResult;

				if (!DatabaseResult)
				{
					auto ExceptionStr = DatabaseResult.f_GetExceptionStr();
					DMibLogWithCategory(LogLocalStore, Critical, "Error saving log entries to database: {}", ExceptionStr);
					co_return DMibErrorInstance("Failed to store log entries in database, see log for error.");
				}

				co_return ReportEntriesResult;
			}
		;
	}

	auto CDistributedAppLogStoreLocal::f_OpenLogReporter(CDistributedAppLogReporter::CLogInfo &&_LogInfo)
		-> TCFuture<CDistributedAppLogReporter::CLogReporter>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto LogInfoKey = _LogInfo.f_Key();

		auto LogMap = Internal.m_Logs(LogInfoKey);

		auto *pLog = &*LogMap;

		auto OnResume = g_OnResume / [&]
			{
				auto &Internal = *mp_pInternal;

				if (f_IsDestroyed())
					DMibError("Shutting down");

				pLog = Internal.m_Logs.f_FindEqual(LogInfoKey);
				if (!pLog)
					DMibError("Aborted");
			}
		;

		bool bWasCreated = LogMap.f_WasCreated();
		bool bWasAdded = ++pLog->m_ActiveRefCount == 1;

		if (!pLog->m_pCanDestroy)
			pLog->m_pCanDestroy = fg_Construct();

		{
			auto SequenceSubscription = co_await pLog->m_LogSequencer.f_Sequence();
			auto &Internal = *mp_pInternal;

			bool bWasChanged = bWasCreated || _LogInfo != pLog->m_LogInfo;

			if (bWasChanged)
			{
				pLog->m_LogInfo = _LogInfo;
				try
				{
					auto WriteTransaction = co_await Internal.m_Database(&CDatabaseActor::f_OpenTransactionWrite);

					auto DatabaseKey = Internal.f_GetDatabaseKey<NLogStoreLocalDatabase::CLogKey>(_LogInfo);

					NLogStoreLocalDatabase::CLogValue DatabaseLogInfo;
					DatabaseLogInfo.m_Info = _LogInfo;
					DatabaseLogInfo.m_UniqueSequenceAtLastCleanup = pLog->m_LastSeenUniqueSequence;

					WriteTransaction.m_Transaction.f_Upsert(DatabaseKey, DatabaseLogInfo);

					co_await Internal.m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
				}
				catch ([[maybe_unused]] CException const &_Exception)
				{
					DMibLogWithCategory(LogLocalStore, Critical, "Error saving log data to database: {}", _Exception);
					co_return DMibErrorInstance("Failed to store log in database, see log for error.");
				}
			}

			if (bWasChanged || bWasAdded)
			{
				auto ChangeInfoResult = co_await fg_CallSafe(Internal, &CInternal::f_LogInfoChanged, LogInfoKey, bWasChanged || bWasAdded).f_Wrap();
				if (!ChangeInfoResult)
					DMibLogWithCategory(LogLocalStore, Error, "Failed to add/change log info in upstream: {}", ChangeInfoResult.f_GetExceptionStr());
			}
		}

		CDistributedAppLogReporter::CLogReporter Reporter;
		Reporter.m_LastSeenUniqueSequence = pLog->m_LastSeenUniqueSequence;
		Reporter.m_fReportEntries = Internal.f_GetReportEntriesFunctor(LogInfoKey, Internal.f_GetDatabaseKey<NLogStoreLocalDatabase::CLogEntryKey>(_LogInfo));

		co_return fg_Move(Reporter);
	}
}
