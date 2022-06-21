// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	void CDistributedAppLogStoreLocal::CInternal::f_ScheduleFailedRetry()
	{
		if (m_bScheduledFailedReportersRetry)
			return;

		m_bScheduledFailedReportersRetry = true;
		fg_Timeout(60.0) > [this]
			{
				fg_CallSafe(*this, &CInternal::f_RetryFailedReporters) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(LogLocalStore, Error, "Failures during retry opening log reporters: {}", _Result.f_GetExceptionStr());
					}
				;
			}
		;
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_UpdateLogForReporter
		(
			CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
			, TCWeakDistributedActor<CActor> const &_WeakActor
		)
	{
		CLog *pLog;
		CLogReporter *pLogReporter;
		CRemoteLogReporterInterface *pLogInterface;

		auto OnResume = g_OnResume / [&]
			{
				if (m_pThis->f_IsDestroyed())
					DMibError("Shutting down");

				pLog = m_Logs.f_FindEqual(_LogInfoKey);
				if (!pLog)
					DMibError("Aborted");

				pLogReporter = pLog->m_LogReporters.f_FindEqual(_WeakActor);
				if (!pLogReporter)
					DMibError("Aborted");

				pLogInterface = m_LogInterfaces.f_FindEqual(_WeakActor);
				if (!pLogInterface)
					DMibError("Aborted");
			}
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

		auto Reporter = co_await pLogInterface->m_Actor.f_CallActor(&CDistributedAppLogReporter::f_OpenLogReporter)(pLog->m_LogInfo);
		if (!Reporter.m_fReportEntries)
			co_return DMibErrorInstance("Invalid report readings functor returned from f_OpenLogReporter");

		if (Reporter.m_LastSeenUniqueSequence < pLog->m_LastSeenUniqueSequence)
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			auto DatabaseKey = f_GetDatabaseKey<NLogStoreLocalDatabase::CLogEntryKey>(pLog->m_LogInfo);
			DatabaseKey.m_UniqueSequence = Reporter.m_LastSeenUniqueSequence;

			auto iLogEntry = ReadTransaction.m_Transaction.f_ReadCursor((NLogStoreLocalDatabase::CLogKey const &)DatabaseKey);
			iLogEntry.f_FindLowerBound(DatabaseKey);

			if (iLogEntry)
			{
				auto Key = iLogEntry.f_Key<NLogStoreLocalDatabase::CLogEntryKey>();
				if (Key.m_UniqueSequence == Reporter.m_LastSeenUniqueSequence)
					++iLogEntry;
			}

			NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> Batch;

			static constexpr mint c_BatchSize = 32768;

			for (; iLogEntry; ++iLogEntry)
			{
				auto Key = iLogEntry.f_Key<NLogStoreLocalDatabase::CLogEntryKey>();
				auto Value = iLogEntry.f_Value<NLogStoreLocalDatabase::CLogEntryValue>();

				Batch.f_Insert(fg_Move(Value).f_Entry(Key));

				if (Batch.f_GetLen() >= c_BatchSize)
					co_await Reporter.m_fReportEntries(fg_Move(Batch));
			}

			if (!Batch.f_IsEmpty())
				co_await Reporter.m_fReportEntries(fg_Move(Batch));
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
					, pLog->m_LogInfo
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

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogInfoChanged(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey, bool _bWasCreated)
	{
		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);

		if (!pLog)
			co_return {};

		TCActorResultVector<void> Results;

		if (_bWasCreated)
		{
			for (auto &LogInterface : m_LogInterfaces)
			{
				auto &WeakActor = m_LogInterfaces.fs_GetKey(LogInterface);

				auto &LogReporter = pLog->m_LogReporters[WeakActor];
				LogReporter.m_pLog = pLog;
				LogReporter.m_pInterface = &LogInterface;
				LogInterface.m_Reporters.f_Insert(LogReporter);
				fg_CallSafe(*this, &CInternal::f_UpdateLogForReporter, _LogInfoKey, WeakActor).f_Dispatch() % f_GetLogUpdateFailedMessage(LogReporter) > Results.f_AddResult();
			}
		}
		else
		{
			for (auto &LogReporter : pLog->m_LogReporters)
			{
				auto &WeakActor = pLog->m_LogReporters.fs_GetKey(LogReporter);
				fg_CallSafe(*this, &CInternal::f_UpdateLogForReporter, _LogInfoKey, WeakActor).f_Dispatch() % f_GetLogUpdateFailedMessage(LogReporter) > Results.f_AddResult();
			}
		}

		co_await Results.f_GetResults() | g_Unwrap;

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_RetryFailedReporters()
	{
		TCActorResultVector<void> Results;

		for (auto iFailed = m_FailedReporters.f_GetIterator(); iFailed;)
		{
			auto &Failed = *iFailed;
			++iFailed;

			auto &LogKey = Failed.m_pLog->f_GetKey();

			fg_CallSafe(*this, &CInternal::f_UpdateLogForReporter, LogKey, Failed.f_GetID()).f_Dispatch() % f_GetLogUpdateFailedMessage(Failed) > Results.f_AddResult();
		}

		auto AwaitedResults = co_await Results.f_GetResults();

		m_bScheduledFailedReportersRetry = false;
		if (!m_FailedReporters.f_IsEmpty())
			f_ScheduleFailedRetry();

		fg_Move(AwaitedResults) | g_Unwrap;

		co_return {};
	}

	TCFuture<uint32> CDistributedAppLogStoreLocal::CInternal::f_NewLogEntries
		(
			CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
			, TCVector<CDistributedAppLogReporter::CLogEntry> const &_Entries
		)
	{
		auto *pLog = m_Logs.f_FindEqual(_LogInfoKey);
		if (!pLog)
			co_return 1;

		TCVector<TCVector<CDistributedAppLogReporter::CLogEntry>> EntriesChunks;

		static constexpr mint c_ChunkSize = 32768;

		mint nEntries = _Entries.f_GetLen();

		if (nEntries < c_ChunkSize)
			EntriesChunks.f_Insert(_Entries);
		else
		{
			for (mint iStartEntry = 0; iStartEntry < nEntries;)
			{
				mint ThisTime = fg_Min(nEntries - iStartEntry, c_ChunkSize);
				
				EntriesChunks.f_Insert(TCVector<CDistributedAppLogReporter::CLogEntry>(_Entries.f_GetArray() + iStartEntry, ThisTime));

				iStartEntry += ThisTime;
			}
		}

		TCActorResultVector<uint32> Results;

		for (auto &Entries : EntriesChunks)
		{
			for (auto &Reporter : pLog->m_LogReporters)
			{
				auto WeakActor = pLog->m_LogReporters.fs_GetKey(Reporter);
				Reporter.m_WriteSequencer / [this, Entries, WeakActor, _LogInfoKey](CActorSubscription &&_DoneSubscription) mutable -> TCFuture<uint32>
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

						auto ReportEntriesResult = co_await pReporter->m_Reporter.m_fReportEntries(fg_Move(Entries));

						(void)_DoneSubscription;

						co_return ReportEntriesResult.m_ReportDepth;
					}
					> Results.f_AddResult()
				;
			}
		}

		uint32 MaxReportDepth = 0;
		for (auto ReportDepth : co_await Results.f_GetResults() | g_Unwrap)
			MaxReportDepth = fg_Max(ReportDepth, MaxReportDepth);

		co_return MaxReportDepth + 1;
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogReporterInterfaceAdded
		(
			TCDistributedActorInterface<CDistributedAppLogReporter> &&_Actor
			, CTrustedActorInfo const &_TrustInfo
		)
	{
		TCWeakDistributedActor<CActor> WeakActor = _Actor.f_Weak();
		auto &LogInterface = m_LogInterfaces[WeakActor];
		LogInterface.m_Actor = fg_Move(_Actor);

		TCActorResultVector<void> Results;
		for (auto &Log : m_Logs)
		{
			auto &LogReporter = Log.m_LogReporters[WeakActor];
			LogReporter.m_pLog = &Log;
			LogReporter.m_pInterface = &LogInterface;
			LogInterface.m_Reporters.f_Insert(LogReporter);

			auto &LogKey = m_Logs.fs_GetKey(Log);

			fg_CallSafe(*this, &CInternal::f_UpdateLogForReporter, LogKey, WeakActor).f_Dispatch() % f_GetLogUpdateFailedMessage(LogReporter) > Results.f_AddResult();
		}

		co_await Results.f_GetResults() | g_Unwrap;

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_LogReporterInterfaceRemoved(TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo)
 	{
		auto *pInterface = m_LogInterfaces.f_FindEqual(_Actor);

		if (!pInterface)
			co_return {};

		TCActorResultVector<void> Results;
		for (auto iReporter = pInterface->m_Reporters.f_GetIterator(); iReporter;)
		{
			auto &Reporter = *iReporter;
			++iReporter;
			Reporter.m_WriteSequencer.f_Abort() > Results.f_AddResult();
			fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Results.f_AddResult();
			Reporter.m_pLog->m_LogReporters.f_Remove(&Reporter);
		}

		m_LogInterfaces.f_Remove(pInterface);

		co_await Results.f_GetResults();

		co_return {};
	}
}
