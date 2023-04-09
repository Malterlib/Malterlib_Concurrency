// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	CDistributedAppLogStoreLocal::CDistributedAppLogStoreLocal(TCActor<CActorDistributionManager> const &_DistributionManager, TCActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_pInternal(fg_Construct(this, _DistributionManager, _TrustManager))
	{
	}

	CDistributedAppLogStoreLocal::~CDistributedAppLogStoreLocal()
	{
	}

	TCFuture<void> CDistributedAppLogStoreLocal::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_CleanupTimerSubscription)
		{
			co_await Internal.m_CleanupTimerSubscription->f_Destroy();
			Internal.m_CleanupTimerSubscription.f_Clear();
		}

		auto CanDestroyFuture = Internal.m_pCanDestroyStoringLocal->f_Future();
		Internal.m_pCanDestroyStoringLocal.f_Clear();
		co_await fg_Move(CanDestroyFuture).f_Timeout(10.0, "Timeout").f_Wrap();

		TCActorResultVector<void> Destroys;
		for (auto &Log : Internal.m_Logs)
		{
			Log.m_LogSequencer.f_Abort() > Destroys.f_AddResult();
			for (auto &Reporter : Log.m_LogReporters)
			{
				Reporter.m_WriteSequencer.f_Abort() > Destroys.f_AddResult();
				if (Reporter.m_Reporter.m_fReportEntries)
					fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Destroys.f_AddResult();
			}
		}

		co_await Destroys.f_GetResults();

		if (Internal.m_bOwnDatabase)
			co_await Internal.m_Database.f_Destroy();

		Internal.m_LogInterfaces.f_Clear();
		Internal.m_Logs.f_Clear();

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::f_StartWithDatabase
		(
			TCActor<NDatabase::CDatabaseActor> &&_Database
			, CStr const &_Prefix
			, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction)> &&_fCleanup
		)
	{
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (f_IsDestroyed())
						return DMibErrorInstance("Shutting down");
					return {};
				}
			)
		;

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Move(_Database);
		Internal.m_fCleanup = fg_Move(_fCleanup);
		Internal.m_Prefix = _Prefix;

		co_await fg_CallSafe(&Internal, &CInternal::f_Start);
		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::f_StartWithDatabasePath(CStr const &_DatabasePath, uint64 _RetentionDays)
	{
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (f_IsDestroyed())
						return DMibErrorInstance("Shutting down");
					return {};
				}
			)
		;

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Construct(fg_Construct(), "");
		Internal.m_bOwnDatabase = true;
		Internal.m_Prefix = "mib.llog"; // llog = local log
		Internal.m_RetentionDays = _RetentionDays;

		uint64 MaxDatabaseSize = constant_int64(1) * 1024 * 1024 * 1024;

		co_await Internal.m_Database(&CDatabaseActor::f_OpenDatabase, _DatabasePath, MaxDatabaseSize);

		[[maybe_unused]] auto Stats = co_await (Internal.m_Database(&CDatabaseActor::f_GetAggregateStatistics));
		[[maybe_unused]] auto TotalSizeUsed = Stats.f_GetTotalUsedSize();

		DMibLogWithCategory
			(
				LogLocalStore
				, Debug
				, "Database at '{}' uses {fe2}% of allotted space ({ns } / {ns } bytes)"
				, _DatabasePath
				, fp64(TotalSizeUsed) / fp64(MaxDatabaseSize) * 100.0
				, TotalSizeUsed
				, MaxDatabaseSize
			)
		;

		co_await fg_CallSafe(&Internal, &CInternal::f_Start);

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_PerformLocalCleanup()
	{
		co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [this](CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_return co_await fg_CallSafe(*this, &CInternal::f_Cleanup, fg_Move(_Transaction));
				}
			)
		;
		
		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_Start()
	{
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (m_pThis->f_IsDestroyed())
						return DMibErrorInstance("Shutting down");
					return {};
				}
			)
		;

		{
			auto CaptureScope = co_await (g_CaptureExceptions % "Error reading log data from database (f_Start)");

			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			for (auto iLog = ReadTransaction.m_Transaction.f_ReadCursor(m_Prefix, CLogKey::mc_Prefix); iLog; ++iLog)
			{
				auto Key = iLog.f_Key<CLogKey>();
				auto Value = iLog.f_Value<CLogValue>();

				auto &Log = *m_Logs(Key.f_LogInfoKey(), CLog{fg_Move(Value.m_Info)});

				Log.m_LastSeenUniqueSequence = Value.m_UniqueSequenceAtLastCleanup;

				{
					auto DatabaseKey = f_GetDatabaseKey<CLogEntryKey>(Log.m_LogInfo);
					auto ReadCursor = ReadTransaction.m_Transaction.f_ReadCursor((CLogKey const &)DatabaseKey);
					if (ReadCursor.f_Last())
						Log.m_LastSeenUniqueSequence = fg_Max(ReadCursor.f_Key<CLogEntryKey>().m_UniqueSequence, Log.m_LastSeenUniqueSequence);
				}
			}
		}

		if (m_RetentionDays && !m_fCleanup)
		{
			co_await fg_CallSafe(this, &CInternal::f_PerformLocalCleanup).f_Wrap() > fg_LogError("LogLocalStore", "Failed to perform initial database cleanup");

			m_CleanupTimerSubscription = co_await fg_RegisterTimer
				(
					24.0 * 60.0 * 60.0
					, [this]() -> TCFuture<void>
					{
						auto Result = co_await fg_CallSafe(this, &CInternal::f_PerformLocalCleanup).f_Wrap();

						if (!Result)
							DMibLogWithCategory(LogLocalStore, Error, "Failed to do nightly cleanup: {}", Result.f_GetExceptionStr());

						co_return {};
					}
				)
			;
		}

		m_LogsInterfaceSubscription = co_await m_TrustManager->f_SubscribeTrustedActors<CDistributedAppLogReporter>();

		co_await m_LogsInterfaceSubscription.f_OnActor
			(
				g_ActorFunctor / [this](TCDistributedActor<CDistributedAppLogReporter> const &_Actor, CTrustedActorInfo const &_TrustInfo) -> TCFuture<void>
				{
					auto Result = co_await fg_CallSafe(*this, &CInternal::f_LogReporterInterfaceAdded, fg_TempCopy(_Actor), _TrustInfo).f_Wrap();
					if (!Result)
						DMibLogWithCategory(LogLocalStore, Error, "Failures adding new log reporter: {}", Result.f_GetExceptionStr());

					co_return {};
				}
				, g_ActorFunctor / [this](TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo) -> TCFuture<void>
				{
					co_await fg_CallSafe(*this, &CInternal::f_LogReporterInterfaceRemoved, _Actor, fg_Move(_TrustInfo));

					co_return {};
				}
				, "Malterlib/Concurrency/Log"
				, "Error when calling {} for log reporter"
			)
		;

		m_bStarted = true;

		co_return {};
	}

	TCFuture<CActorSubscription> CDistributedAppLogStoreLocal::f_AddExtraLogReporter
		(
			TCDistributedActorInterface<CDistributedAppLogReporter> &&_LogReporter
			, CTrustedActorInfo const &_TrustInfo
		)
	{
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (f_IsDestroyed())
						return DMibErrorInstance("Shutting down");
					return {};
				}
			)
		;

		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Subscription = g_ActorSubscription / [this, _TrustInfo, LogInterfaceWeak = _LogReporter.f_Weak(), TrustInfo = _TrustInfo]() mutable
			{
				auto &Internal = *mp_pInternal;
				fg_CallSafe(Internal, &CInternal::f_LogReporterInterfaceRemoved, LogInterfaceWeak, fg_Move(TrustInfo))
					> fg_LogError("Malterlib/Concurrency/Log", "Error removing log reporter")
				;
			}
		;

		auto Result = co_await fg_CallSafe(Internal, &CInternal::f_LogReporterInterfaceAdded, fg_Move(_LogReporter), _TrustInfo).f_Wrap();
		if (!Result)
			DMibLogWithCategory(LogLocalStore, Error, "Failures adding extra log reporter: {}", Result.f_GetExceptionStr());

		co_return fg_Move(Subscription);
	}
}
