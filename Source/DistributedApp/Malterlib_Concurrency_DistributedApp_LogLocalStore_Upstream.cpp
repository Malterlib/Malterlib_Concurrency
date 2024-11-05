// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	void CDistributedAppLogStoreLocal::CInternal::f_ScheduleFailedRetry()
	{
		if (m_bScheduledFailedReportersRetry)
			return;

		m_bScheduledFailedReportersRetry = true;
		fg_Timeout(60.0) > [this]() -> TCFuture<void>
			{
				auto Result = co_await f_RetryFailedReporters().f_Wrap();

				if (!Result)
					DMibLogWithCategory(LogLocalStore, Error, "Failures during retry opening log reporters: {}", Result.f_GetExceptionStr());

				co_return {};
			}
		;
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_UpdateLogForReporter
		(
			CDistributedAppLogReporter::CLogInfoKey _LogInfoKey
			, TCWeakDistributedActor<CActor> _WeakActor
		)
	{
		CLog *pLog;
		CLogReporter *pLogReporter;
		CRemoteLogReporterInterface *pLogInterface;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (m_pThis->f_IsDestroyed())
						return DMibErrorInstance("Shutting down");

					pLog = m_Logs.f_FindEqual(_LogInfoKey);
					if (!pLog)
						return DMibErrorInstance("Aborted");

					pLogReporter = pLog->m_LogReporters.f_FindEqual(_WeakActor);
					if (!pLogReporter)
						return DMibErrorInstance("Aborted");

					pLogInterface = m_LogInterfaces.f_FindEqual(_WeakActor);
					if (!pLogInterface)
						return DMibErrorInstance("Aborted");

					return {};
				}
			)
		;

		auto SequenceSubscription = co_await pLogReporter->m_WriteSequencer.f_Sequence();

		auto OnFailure = g_OnScopeExit / [&]
			{
				pLog = m_Logs.f_FindEqual(_LogInfoKey);
				if (!pLog)
					return;

				pLogReporter = pLog->m_LogReporters.f_FindEqual(_WeakActor);
				if (!pLogReporter)
					return;

				m_FailedReporters.f_Insert(pLogReporter);
				f_ScheduleFailedRetry();
			}
		;

		// Destroy old reporter
		if (pLogReporter->m_Reporter.m_fReportEntries)
			co_await fg_Move(pLogReporter->m_Reporter.m_fReportEntries).f_Destroy();

		auto Reporter = co_await pLogInterface->m_Actor.f_CallActor(&CDistributedAppLogReporter::f_OpenLogReporter)(pLog->m_Info);
		if (!Reporter.m_fReportEntries)
			co_return DMibErrorInstance("Invalid report readings functor returned from f_OpenLogReporter");

		if (Reporter.m_LastSeenUniqueSequence < pLog->m_LastSeenUniqueSequence)
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			auto DatabaseKey = f_GetDatabaseKey<CLogEntryKey>(pLog->m_Info);
			DatabaseKey.m_UniqueSequence = Reporter.m_LastSeenUniqueSequence;

			auto [iLogEntry, ReadTransactionMove] = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[DatabaseKey, LastSeenUniqueSequence = Reporter.m_LastSeenUniqueSequence](CDatabaseActor::CTransactionRead &&_ReadTransaction)
					{
						auto iLogEntry = _ReadTransaction.m_Transaction.f_ReadCursor((CLogKey const &)DatabaseKey);
						iLogEntry.f_FindLowerBound(DatabaseKey);

						if (iLogEntry)
						{
							auto Key = iLogEntry.f_Key<CLogEntryKey>();
							if (Key.m_UniqueSequence == LastSeenUniqueSequence)
								++iLogEntry;
						}

						return fg_Tuple(fg_Move(iLogEntry), fg_Move(_ReadTransaction));
					}
				)
			;
			ReadTransaction = fg_Move(ReadTransactionMove);

			static constexpr mint c_BatchSize = 32768;
			static constexpr mint c_MaxMessageSize = CActorDistributionManager::mc_MaxMessageSize - (CActorDistributionManager::mc_RemoteCallOverhead + 4 + 128);

			auto fGetBatch = [iLogEntry = fg_Move(iLogEntry), nBytesInBatch = mint(0)]() mutable -> TCFuture<TCOptional<NContainer::TCVector<CDistributedAppLogReporter::CLogEntry>>>
				{
					if (!iLogEntry)
						co_return {};

					auto BlockingActorCheckout = fg_BlockingActor();
					co_await fg_ContinueRunningOnActor(BlockingActorCheckout);

					NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> Batch;

					for (; iLogEntry; ++iLogEntry)
					{
						auto Key = iLogEntry.f_Key<CLogEntryKey>();
						auto Value = iLogEntry.f_Value<CLogEntryValue>();

						auto NewEntry = fg_Move(Value).f_Entry(Key);

						mint ThisTime = NStream::fg_GetBinaryStreamSize(NewEntry);

						if (ThisTime > c_MaxMessageSize)
						{
							// This should never happen now. For old databases do this in case the database has stored entries that are too big
							NewEntry.m_Data.m_Message = NewEntry.m_Data.m_Message.f_Left(c_MaxMessageSize);
						}

						if (nBytesInBatch + ThisTime > CActorDistributionManager::mc_HalfMaxMessageSize)
						{
							if (!Batch.f_IsEmpty())
							{
								nBytesInBatch = 0;
								break;
							}
						}

						Batch.f_Insert(fg_Move(NewEntry));
						nBytesInBatch += ThisTime;

						if (Batch.f_GetLen() >= c_BatchSize)
						{
							nBytesInBatch = 0;
							break;
						}
					}

					co_return fg_Move(Batch);
				}
			;

			auto pGetBatch = fg_ToSharedPointer(fg_Move(fGetBatch));

			while (auto BatchEntry = co_await fg_CallSafe(pGetBatch))
				co_await Reporter.m_fReportEntries(fg_Move(*BatchEntry));
		}
		else if (Reporter.m_LastSeenUniqueSequence > pLog->m_LastSeenUniqueSequence)
		{
			DMibLogWithCategory
				(
					LogLocalStore
					, Warning
					, "Detected incorrect database last unique sequence. Reset from {} to {}\n{}"
					, pLog->m_LastSeenUniqueSequence
					, Reporter.m_LastSeenUniqueSequence
					, pLog->m_Info
				)
			;
			pLog->m_LastSeenUniqueSequence = Reporter.m_LastSeenUniqueSequence;
		}

		pLogReporter->m_Reporter = fg_Move(Reporter);

		OnFailure.f_Clear();

		pLogReporter->m_LinkFailed.f_Unlink();

		co_return {};
	}

	CStr CDistributedAppLogStoreLocal::CInternal::f_GetLogUpdateFailedMessage(CLogReporter const &_Reporter)
	{
		return "Failed to update log '{}' for reporter on host '{}'"_f << _Reporter.m_pLog->f_GetKey() << _Reporter.m_pInterface->m_Actor->f_GetHostInfo().m_RealHostID;
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogInfoChanged(CDistributedAppLogReporter::CLogInfoKey _LogInfoKey, bool _bWasAdded)
	{
		auto CheckDestroy = co_await m_pThis->f_CheckDestroyedOnResume();
		
		auto Result = co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, ThisActor = fg_ThisActor(m_pThis), pThis = this, DatabaseKey = f_GetDatabaseKey<CLogKey>(_LogInfoKey), Prefix = m_Prefix]
				(CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto WriteTransaction = fg_Move(_Transaction);
					if (_bCompacting)
						WriteTransaction = co_await ThisActor(&CDistributedAppLogStoreLocal::fp_Cleanup, fg_Move(WriteTransaction));

					CDistributedAppLogReporter::CLogInfo LogInfo;
					uint64 LastSeenUniqueSequence = 0;
					{
						CLog *pLog;
						if (pLog = pThis->m_Logs.f_FindEqual(_LogInfoKey); !pLog)
							co_return DMibErrorInstance("Log was removed");

						LogInfo = pLog->m_Info;
						LastSeenUniqueSequence = pLog->m_LastSeenUniqueSequence;

						if (!_bCompacting)
							pThis->f_Subscription_LogInfoChanged(*pLog);
					}

					if (DatabaseKey.m_HostID)
					{
						auto &KnownHostValue = pThis->m_KnownHosts[DatabaseKey.m_HostID];
						KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, LogInfo.m_LastSeen);
					}
					
					co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

					if (DatabaseKey.m_HostID)
					{
						CKnownHostKey KnownHostKey{.m_DbPrefix = Prefix, .m_HostID = DatabaseKey.m_HostID};

						CKnownHostValue KnownHostValue;
						WriteTransaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

						KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, LogInfo.m_LastSeen);

						WriteTransaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);
					}

					CLogValue DatabaseLogInfo;
					DatabaseLogInfo.m_Info = fg_Move(LogInfo);
					DatabaseLogInfo.m_UniqueSequenceAtLastCleanup = LastSeenUniqueSequence;

					WriteTransaction.m_Transaction.f_Upsert(DatabaseKey, DatabaseLogInfo);

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(LogLocalStore, Critical, "Error saving log data to database: {}", Result.f_GetExceptionStr());
			co_return DMibErrorInstance("Failed to store log in database, see log for error.");
		}

		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);

		if (!pLog)
			co_return {};

		TCFutureVector<void> Results;

		if (_bWasAdded)
		{
			for (auto &LogInterface : m_LogInterfaces)
			{
				auto &WeakActor = m_LogInterfaces.fs_GetKey(LogInterface);

				auto &LogReporter = pLog->m_LogReporters[WeakActor];
				LogReporter.m_pLog = pLog;
				LogReporter.m_pInterface = &LogInterface;
				LogInterface.m_Reporters.f_Insert(LogReporter);
				f_UpdateLogForReporter(_LogInfoKey, WeakActor) % f_GetLogUpdateFailedMessage(LogReporter) > Results;
			}
		}
		else
		{
			for (auto &LogReporter : pLog->m_LogReporters)
			{
				auto &WeakActor = pLog->m_LogReporters.fs_GetKey(LogReporter);
				f_UpdateLogForReporter(_LogInfoKey, WeakActor) % f_GetLogUpdateFailedMessage(LogReporter) > Results;
			}
		}

		auto UpstreamResult = co_await fg_AllDone(Results).f_Wrap();

		if (!UpstreamResult)
			DMibLogWithCategory(LogLocalStore, Error, "Failed to add/change log info in upstream: {}", UpstreamResult.f_GetExceptionStr());

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_RetryFailedReporters()
	{
		TCFutureVector<void> Results;

		for (auto iFailed = m_FailedReporters.f_GetIterator(); iFailed;)
		{
			auto &Failed = *iFailed;
			++iFailed;

			auto &LogKey = Failed.m_pLog->f_GetKey();

			f_UpdateLogForReporter(LogKey, Failed.f_GetID()) % f_GetLogUpdateFailedMessage(Failed) > Results;
		}

		auto AwaitedResults = co_await fg_AllDoneWrapped(Results);

		m_bScheduledFailedReportersRetry = false;
		if (!m_FailedReporters.f_IsEmpty())
			f_ScheduleFailedRetry();

		co_await (fg_Move(AwaitedResults) | g_Unwrap);

		co_return {};
	}

	TCFuture<uint32> CDistributedAppLogStoreLocal::CInternal::f_NewLogEntries
		(
			CDistributedAppLogReporter::CLogInfoKey _LogInfoKey
			, NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> _pEntries
		)
	{
		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
		if (!pLog)
			co_return 1;

		auto &Entries = *_pEntries;

		TCVector<NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const>> EntriesChunks;

		auto fAddEntriesToChunks = [&](NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> const &_pEntries)
			{
				if (NStream::fg_GetBinaryStreamSize(*_pEntries) <= CActorDistributionManager::mc_HalfMaxMessageSize)
				{
					EntriesChunks.f_Insert(_pEntries);
					return;
				}

				TCVector<CDistributedAppLogReporter::CLogEntry> NewEntries;
				mint nBytesInChunk = 0;

				for (auto &Entry : *_pEntries)
				{
					mint ThisTime = NStream::fg_GetBinaryStreamSize(Entry);
					DMibFastCheck(ThisTime <= (CActorDistributionManager::mc_MaxMessageSize - 4));

					if (nBytesInChunk + ThisTime > CActorDistributionManager::mc_HalfMaxMessageSize && !NewEntries.f_IsEmpty())
					{
						EntriesChunks.f_Insert(fg_Construct(fg_Construct(fg_Move(NewEntries))));
						nBytesInChunk = 0;
					}

					NewEntries.f_Insert(Entry);
					nBytesInChunk += ThisTime;
				}

				if (!NewEntries.f_IsEmpty())
					EntriesChunks.f_Insert(fg_Construct(fg_Construct(fg_Move(NewEntries))));
			}
		;

		static constexpr mint c_ChunkSize = 32768;

		mint nEntries = Entries.f_GetLen();

		if (nEntries < c_ChunkSize)
			fAddEntriesToChunks(_pEntries);
		else
		{
			for (mint iStartEntry = 0; iStartEntry < nEntries;)
			{
				mint ThisTime = fg_Min(nEntries - iStartEntry, c_ChunkSize);
				fAddEntriesToChunks(fg_Construct(TCVector<CDistributedAppLogReporter::CLogEntry>(Entries.f_GetArray() + iStartEntry, ThisTime)));
				iStartEntry += ThisTime;
			}
		}

		TCFutureVector<uint32> Results;

		for (auto &pEntries : EntriesChunks)
		{
			for (auto &Reporter : pLog->m_LogReporters)
			{
				auto WeakActor = pLog->m_LogReporters.fs_GetKey(Reporter);

				Reporter.m_WriteSequencer.f_RunSequenced
					(
						g_ActorFunctorWeak / [this, pEntries, WeakActor, _LogInfoKey](CActorSubscription _DoneSubscription) mutable -> TCFuture<uint32>
						{
							auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
							if (!pLog)
								co_return 0;

							auto pReporter = pLog->m_LogReporters.f_FindEqual(WeakActor);
							if (!pReporter)
								co_return 0;

							if (pReporter->m_LinkFailed.f_IsInList())
								co_return 0;

							if (!pReporter->m_Reporter.m_fReportEntries)
								co_return 0;

							auto ReportEntriesResult = co_await pReporter->m_Reporter.m_fReportEntries(*pEntries);

							(void)_DoneSubscription;

							co_return ReportEntriesResult.m_ReportDepth;
						}
					)
					> Results
				;
			}
		}

		uint32 MaxReportDepth = 0;
		for (auto ReportDepth : co_await fg_AllDone(Results))
			MaxReportDepth = fg_Max(ReportDepth, MaxReportDepth);

		co_return MaxReportDepth + 1;
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogReporterInterfaceAdded
		(
			TCDistributedActorInterface<CDistributedAppLogReporter> _Actor
			, CTrustedActorInfo _TrustInfo
		)
	{
		TCWeakDistributedActor<CActor> WeakActor = _Actor.f_Weak();
		auto &LogInterface = m_LogInterfaces[WeakActor];
		LogInterface.m_Actor = fg_Move(_Actor);

		TCFutureVector<void> Results;
		for (auto &Log : m_Logs)
		{
			auto &LogReporter = Log.m_LogReporters[WeakActor];
			LogReporter.m_pLog = &Log;
			LogReporter.m_pInterface = &LogInterface;
			LogInterface.m_Reporters.f_Insert(LogReporter);

			auto &LogKey = m_Logs.fs_GetKey(Log);

			f_UpdateLogForReporter(LogKey, WeakActor) % f_GetLogUpdateFailedMessage(LogReporter) > Results;
		}

		co_await fg_AllDone(Results);

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogReporterInterfaceRemoved(TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _TrustInfo)
	{
		auto *pInterface = m_LogInterfaces.f_FindEqual(_Actor);

		if (!pInterface)
			co_return {};

		TCFutureVector<void> Results;
		for (auto iReporter = pInterface->m_Reporters.f_GetIterator(); iReporter;)
		{
			auto &Reporter = *iReporter;
			++iReporter;
			fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Results;
			fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Results;
			Reporter.m_pLog->m_LogReporters.f_Remove(&Reporter);
		}

		m_LogInterfaces.f_Remove(pInterface);

		co_await fg_AllDoneWrapped(Results);

		co_return {};
	}
}
