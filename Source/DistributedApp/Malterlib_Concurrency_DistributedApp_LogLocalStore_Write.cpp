// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	TCFuture<void> CDistributedAppLogStoreLocal::f_SeenHosts(NContainer::TCMap<NStr::CStr, NTime::CTime> &&_HostsSeen)
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Result = co_await Internal.m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, HostsSeen = fg_Move(_HostsSeen)]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto &Internal = *mp_pInternal;

					auto WriteTransaction = fg_Move(_Transaction);

					if (_bCompacting)
						WriteTransaction = co_await fg_CallSafe(Internal, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

					for (auto &LastSeen : HostsSeen)
					{
						auto &HostID = HostsSeen.fs_GetKey(LastSeen);

						CKnownHostKey Key{.m_DbPrefix = Internal.m_Prefix, .m_HostID = HostID};

						CKnownHostValue Value;
						_Transaction.m_Transaction.f_Get(Key, Value);

						Value.m_LastSeen = fg_Max(Value.m_LastSeen, LastSeen);
						Value.m_bRemoved = false;

						WriteTransaction.m_Transaction.f_Upsert(Key, Value);
					}

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error saving knowns hosts to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::f_RemoveHosts(NContainer::TCSet<NStr::CStr> &&_RemovedHostIDs)
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Result = co_await Internal.m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, RemovedHostIDs = fg_Move(_RemovedHostIDs)]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto &Internal = *mp_pInternal;

					auto WriteTransaction = fg_Move(_Transaction);

					if (_bCompacting)
						WriteTransaction = co_await fg_CallSafe(Internal, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

					for (auto &HostID : RemovedHostIDs)
					{
						CKnownHostKey Key{.m_DbPrefix = Internal.m_Prefix, .m_HostID = HostID};

						CKnownHostValue Value;
						if (!_Transaction.m_Transaction.f_Get(Key, Value))
							continue;

						Value.m_bRemoved = true;
						WriteTransaction.m_Transaction.f_Upsert(Key, Value);
					}

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error saving knowns hosts remove to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_StoreLogEntries
		(
			CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
			, CLogEntryKey const &_DatabaseKey
			, NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> const &_pEntries
		)
	{
		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
		if (!pLog)
			co_return {};

		auto pCanDestroyTracker = m_pCanDestroyStoringLocal;

		DMibLogOperation(DisableDistributedLogReporter); // Prevent recursive logs

		auto Result = co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, HostID = _LogInfoKey.m_HostID]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto DatabaseKey = _DatabaseKey;
					auto WriteTransaction = fg_Move(_Transaction);

					auto SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
					smint BatchLimit = fg_Min(SizeStats.m_MapSize / (20 * 3), 4 * 1024 * 1024);
					auto SizeLimit = smint(SizeStats.m_MapSize) - fg_Min(fg_Max(smint(SizeStats.m_MapSize) / 20, BatchLimit * 3, 32 * smint(SizeStats.m_PageSize)), smint(SizeStats.m_MapSize) / 3);
					smint nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);

					CKnownHostKey KnownHostKey{.m_DbPrefix = m_Prefix, .m_HostID = HostID};

					CKnownHostValue KnownHostValue;
					if (HostID)
						_Transaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

					for (auto &Entry : *_pEntries)
					{
						smint nBytesInserted = WriteTransaction.m_Transaction.f_SizeStatistics().m_UsedBytes - SizeStats.m_UsedBytes;
						bool bFreeUpSpace = nBytesInserted >= nBytesLimit || _bCompacting;

						if (bFreeUpSpace)
							WriteTransaction = co_await fg_CallSafe(this, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

						if (nBytesInserted > BatchLimit || bFreeUpSpace)
						{
							if (_bCompacting)
								co_await m_Database(&CDatabaseActor::f_Compact, fg_Move(WriteTransaction), 0);
							else
							{
								co_await m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
								if (bFreeUpSpace)
									co_await m_Database(&CDatabaseActor::f_WaitForAllReaders);
							}
							WriteTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionWrite);
							SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
							nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);
						}

						DatabaseKey.m_UniqueSequence = Entry.m_UniqueSequence;

						if (WriteTransaction.m_Transaction.f_Exists(DatabaseKey))
							continue; // Already reported

						CLogEntryValue Value;
						Value.m_Timestamp = Entry.m_Timestamp;
						Value.m_Data = Entry.m_Data;

						CLogEntryByTime ByTime;
						ByTime.m_DbPrefix = DatabaseKey.m_DbPrefix;
						ByTime.m_Prefix = CLogEntryByTime::mc_Prefix;
						ByTime.m_Timestamp = Entry.m_Timestamp;
						ByTime.m_UniqueSequence = DatabaseKey.m_UniqueSequence;

						ByTime.m_HostID = DatabaseKey.m_HostID;
						ByTime.m_ApplicationName = DatabaseKey.m_ApplicationName;
						ByTime.m_Identifier = DatabaseKey.m_Identifier;
						ByTime.m_IdentifierScope = DatabaseKey.m_IdentifierScope;

						KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, Entry.m_Timestamp);

						WriteTransaction.m_Transaction.f_Insert(DatabaseKey, Value);
						WriteTransaction.m_Transaction.f_Insert(ByTime, CLogEntryByTimeValue());
					}

					if (HostID)
						_Transaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error saving log data to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_CleanupLogReporter(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey)
	{
		CInternal::CLog *pLog = nullptr;
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pLog = m_Logs.f_FindEqual(_LogInfoKey);
					if (!pLog)
						return DMibErrorInstance("Log removed");
					return {};
				}
			)
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

	namespace
	{
		TCVector<CDistributedAppLogReporter::CLogEntry> fg_SplitEntriesByMaxSize(TCVector<CDistributedAppLogReporter::CLogEntry> &&_Entries)
		{
			TCVector<CDistributedAppLogReporter::CLogEntry> Entries = fg_Move(_Entries);
			TCVector<CDistributedAppLogReporter::CLogEntry> ResizedEntries;

#if DMibPPtrBits < 64
			static constexpr mint c_MaxSize = 768 * 1024; // Workaround bugs in memory mapping for overflow values in lmdb
#else
			static constexpr mint c_MaxSize = CActorDistributionManager::mc_HalfMaxMessageSize;
#endif

			bool bNeedResize = false;
			mint nEntriesProcessed = 0;
			for (auto &Entry : Entries)
			{
				auto CountEntry = g_OnScopeExit / [&]
					{
						++nEntriesProcessed;
					}
				;

				mint Size = NStream::fg_GetBinaryStreamSize(Entry);

				if (Entry.m_UniqueSequence != TCLimitsInt<uint64>::mc_Max)
				{
					if (bNeedResize)
						ResizedEntries.f_Insert(fg_Move(Entry));
					continue;
				}

				if (Size <= c_MaxSize)
				{
					if (bNeedResize)
						ResizedEntries.f_Insert(fg_Move(Entry));
					continue;
				}

				if (!bNeedResize)
				{
					bNeedResize = true;
					for (mint iEntry = 0; iEntry < nEntriesProcessed; ++iEntry)
						ResizedEntries.f_Insert(fg_Move(Entries[iEntry]));
				}

				auto SourceMessage = fg_Move(Entry.m_Data.m_Message);
				auto AllLines = SourceMessage.f_SplitLine();

				auto pOutEntry = &ResizedEntries.f_Insert(Entry);
				auto *pOutMessage = &pOutEntry->m_Data.m_Message;

				for (auto Line : AllLines)
				{
					mint LineLen = Line.f_GetLen() + 1;
					if ((pOutMessage->f_GetLen() + LineLen) > c_MaxSize)
					{
						if (LineLen > c_MaxSize)
						{
							if (!pOutMessage->f_IsEmpty())
								*pOutMessage += "\n";

							for (mint iStart = 0; iStart < LineLen; iStart += c_MaxSize)
							{
								pOutEntry = &ResizedEntries.f_Insert(Entry);
								pOutMessage = &pOutEntry->m_Data.m_Message;
								*pOutMessage = Line.f_Extract(iStart, fg_Min(LineLen - iStart, c_MaxSize));
							}
						}
						else
						{
							DMibFastCheck(!pOutMessage->f_IsEmpty());
							*pOutMessage += "\n";

							pOutEntry = &ResizedEntries.f_Insert(Entry);
							pOutMessage = &pOutEntry->m_Data.m_Message;
							*pOutMessage = fg_Move(Line);
						}
					}
					else
					{
						if (!pOutMessage->f_IsEmpty())
							*pOutMessage += "\n";
						*pOutMessage += Line;
					}
				}
			}

			if (bNeedResize)
				return ResizedEntries;
			else
				return Entries;
		}
	}

	auto CDistributedAppLogStoreLocal::CInternal::f_GetReportEntriesFunctor
		(
			CDistributedAppLogReporter::CLogInfoKey _LogInfoKey
			, CLogEntryKey _DatabaseKey
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

				auto Entries = fg_SplitEntriesByMaxSize(fg_Move(_Entries));

				for (auto &Entry : Entries)
				{
					if (Entry.m_UniqueSequence == TCLimitsInt<uint64>::mc_Max)
						Entry.m_UniqueSequence = ++pLog->m_LastSeenUniqueSequence;
					else
						pLog->m_LastSeenUniqueSequence = fg_Max(pLog->m_LastSeenUniqueSequence, Entry.m_UniqueSequence);
				}

				NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> pEntries = fg_Construct(fg_Move(Entries));

				f_Subscription_Entries(*pLog, *pEntries);

				auto [DatabaseResult, UpstreamResult] = co_await
					(
						fg_CallSafe(this, &CInternal::f_StoreLogEntries, _LogInfoKey, _DatabaseKey, pEntries)
						+ fg_CallSafe(this, &CInternal::f_NewLogEntries, _LogInfoKey, pEntries)
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

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					auto &Internal = *mp_pInternal;

					if (f_IsDestroyed())
						return DMibErrorInstance("Shutting down");

					pLog = Internal.m_Logs.f_FindEqual(LogInfoKey);
					if (!pLog)
						return DMibErrorInstance("Aborted");

					return {};
				}
			)
		;

		bool bWasCreated = LogMap.f_WasCreated();
		bool bWasAdded = ++pLog->m_ActiveRefCount == 1;

		if (!pLog->m_pCanDestroy)
			pLog->m_pCanDestroy = fg_Construct();
		{
			auto SequenceSubscription = co_await pLog->m_LogSequencer.f_Sequence();
			auto &Internal = *mp_pInternal;

			bool bWasChanged = bWasCreated || _LogInfo != pLog->m_Info;

			if (bWasChanged)
				pLog->m_Info = _LogInfo;

			if (bWasChanged || bWasAdded)
				co_await fg_CallSafe(Internal, &CInternal::f_LogInfoChanged, LogInfoKey, bWasAdded || bWasCreated);
		}

		CDistributedAppLogReporter::CLogReporter Reporter;
		Reporter.m_LastSeenUniqueSequence = pLog->m_LastSeenUniqueSequence;
		Reporter.m_fReportEntries = Internal.f_GetReportEntriesFunctor(LogInfoKey, Internal.f_GetDatabaseKey<CLogEntryKey>(_LogInfo));

		co_return fg_Move(Reporter);
	}
}
