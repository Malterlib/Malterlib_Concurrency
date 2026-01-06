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

	TCFuture<void> CDistributedAppLogStoreLocal::f_DestroyRemote()
	{
		auto &Internal = *mp_pInternal;
		CLogError LogError("LogLocalStore");

		co_await Internal.m_LogsInterfaceSubscription.f_Destroy().f_Wrap() > LogError.f_Warning("Failed destroy local log reporters remote interface subscription");

		{
			TCFutureVector<void> Destroys;
			for (auto &Log : Internal.m_Logs)
			{
				for (auto &Reporter : Log.m_LogReporters)
				{
					fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Destroys;
					if (Reporter.m_Reporter.m_fReportEntries)
						fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Destroys;
				}
				Log.m_LogReporters.f_Clear();
			}

			co_await fg_AllDone(Destroys).f_Wrap() > LogError.f_Warning("Failed destroy local log reporters remote");
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		CLogError LogError("LogLocalStore");

		if (Internal.m_CleanupTimerSubscription)
		{
			co_await Internal.m_CleanupTimerSubscription->f_Destroy();
			Internal.m_CleanupTimerSubscription.f_Clear();
		}

		auto CanDestroyFuture = Internal.m_pCanDestroyStoringLocal->f_Future();
		Internal.m_pCanDestroyStoringLocal.f_Clear();
		co_await fg_Move(CanDestroyFuture).f_Timeout(10.0, "Timeout").f_Wrap() > LogError.f_Warning("Failed to wait for can destroy");

		{
			TCFutureVector<void> Destroys;
			for (auto &Log : Internal.m_Logs)
			{
				fg_Move(Log.m_LogSequencer).f_Destroy() > Destroys;
				for (auto &Reporter : Log.m_LogReporters)
				{
					fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Destroys;
					if (Reporter.m_Reporter.m_fReportEntries)
						fg_Move(Reporter.m_Reporter.m_fReportEntries).f_Destroy() > Destroys;
				}
			}

			Internal.m_LogsInterfaceSubscription.f_Destroy() > Destroys;

			co_await fg_AllDone(Destroys).f_Wrap() > LogError.f_Warning("Failed destroy local log store");;
		}

		if (Internal.m_bOwnDatabase)
			co_await fg_Move(Internal.m_Database).f_Destroy().f_Wrap() > LogError.f_Warning("Failed destroy database");;

		Internal.m_LogInterfaces.f_Clear();
		Internal.m_Logs.f_Clear();

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::f_StartWithDatabase
		(
			TCActor<NDatabase::CDatabaseActor> _Database
			, CStr _Prefix
			, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)> _fCleanup
		)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Move(_Database);
		Internal.m_fCleanup = fg_Move(_fCleanup);
		Internal.m_Prefix = _Prefix;

		co_await Internal.f_Start();
		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::f_StartWithDatabasePath(CStr _DatabasePath, uint64 _RetentionDays)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (Internal.m_bStarted || Internal.m_bStarting)
			co_return DMibErrorInstance("Already started");

		Internal.m_bStarting = true;

		Internal.m_Database = fg_Construct();
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
				, "Database at '{}' uses {fe2}% of allotted space ({ns } / {ns } bytes). {ns } records."
				, _DatabasePath
				, fp64(TotalSizeUsed) / fp64(MaxDatabaseSize) * 100.0
				, TotalSizeUsed
				, MaxDatabaseSize
				, Stats.m_nDataItems
			)
		;

		co_await Internal.f_Start();

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_PerformLocalCleanup()
	{
		auto CheckDestroy = co_await m_pThis->f_CheckDestroyedOnResume();

		co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [this](CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_return co_await CInternal::fs_Cleanup(this, fg_Move(_Transaction));
				}
			)
		;

		co_return {};
	}

	TCFuture<void> CDistributedAppLogStoreLocal::CInternal::f_Start()
	{
		auto OnResume = co_await m_pThis->f_CheckDestroyedOnResume();

		m_ThisHostID = co_await m_TrustManager(&CDistributedActorTrustManager::f_GetHostID);

		CLogGlobalStateKey GlobalStateKey{.m_Prefix = m_Prefix};
		CLogGlobalStateValue GlobalStateValue;
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			auto [StateValue, Logs] = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[GlobalStateKey, Prefix = m_Prefix](CDatabaseActor::CTransactionRead &&_ReadTransaction)
					{
						TCMap<CDistributedAppLogReporter::CLogInfoKey, CLog> Logs;

						CLogGlobalStateValue GlobalStateValue;

						_ReadTransaction.m_Transaction.f_Get(GlobalStateKey, GlobalStateValue);

						for (auto iLog = _ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CLogKey::mc_Prefix); iLog; ++iLog)
						{
							auto Key = iLog.f_Key<CLogKey>();
							auto Value = iLog.f_Value<CLogValue>();

							auto &Log = *Logs(Key.f_LogInfoKey(), CLog{fg_Move(Value.m_Info)});

							Log.m_LastSeenUniqueSequence = Value.m_UniqueSequenceAtLastCleanup;

							{
								auto DatabaseKey = fs_GetDatabaseKey<CLogEntryKey>(Prefix, Log.m_Info);
								auto ReadCursor = _ReadTransaction.m_Transaction.f_ReadCursor((CLogKey const &)DatabaseKey);
								if (ReadCursor.f_Last())
									Log.m_LastSeenUniqueSequence = fg_Max(ReadCursor.f_Key<CLogEntryKey>().m_UniqueSequence, Log.m_LastSeenUniqueSequence);
							}
						}

						return fg_Tuple(fg_Move(GlobalStateValue), fg_Move(Logs));
					}
					, "Error reading log data from database (f_Start)"
				)
			;

			GlobalStateValue = fg_Move(StateValue);
			m_Logs = fg_Move(Logs);
		}

		if (!GlobalStateValue.m_bConvertedKnownHosts)
		{
			co_await m_Database
				(
					&CDatabaseActor::f_WriteWithCompaction
					, g_ActorFunctorWeak / [=, ThisActor = fg_ThisActor(m_pThis), Prefix = m_Prefix]
					(CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;

						auto WriteTransaction = fg_Move(_Transaction);
						if (_bCompacting)
							WriteTransaction = co_await ThisActor(&CDistributedAppLogStoreLocal::fp_Cleanup, fg_Move(WriteTransaction));

						co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

						CLogGlobalStateValue GlobalStateValue;
						WriteTransaction.m_Transaction.f_Get(GlobalStateKey, GlobalStateValue);

						if (GlobalStateValue.m_bConvertedKnownHosts)
							co_return fg_Move(WriteTransaction);

						for (auto iLog = WriteTransaction.m_Transaction.f_ReadCursor(Prefix, CLogKey::mc_Prefix); iLog; ++iLog)
						{
							auto Key = iLog.f_Key<CLogKey>();
							auto Value = iLog.f_Value<CLogValue>();

							CLogKey EntryKey = Key;
							EntryKey.m_Prefix = CLogEntryKey::mc_Prefix;

							auto iEntry = WriteTransaction.m_Transaction.f_ReadCursor(EntryKey);

							if (!iEntry.f_Last())
								continue;

							auto EntryValue = iEntry.f_Value<CLogEntryValue>();

							if (!Key.m_HostID)
								continue;

							CKnownHostKey KnownHostKey{.m_DbPrefix = Prefix, .m_HostID = Key.m_HostID};

							CKnownHostValue KnownHostValue;
							_Transaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

							KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, EntryValue.m_Timestamp);
							_Transaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);
						}

						GlobalStateValue.m_bConvertedKnownHosts = true;
						WriteTransaction.m_Transaction.f_Upsert(GlobalStateKey, GlobalStateValue);

						co_return fg_Move(WriteTransaction);
					}
				)
			;
		}

		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			m_KnownHosts = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[Prefix = m_Prefix](CDatabaseActor::CTransactionRead &&_ReadTransaction)
					{
						TCMap<CStr, NLogStoreLocalDatabase::CKnownHostValue> KnownHosts;

						for (auto iKnownHost = _ReadTransaction.m_Transaction.f_ReadCursor(Prefix, CKnownHostKey::mc_Prefix); iKnownHost; ++iKnownHost)
							KnownHosts(fg_Move(iKnownHost.f_Key<CKnownHostKey>()).m_HostID, iKnownHost.f_Value<CKnownHostValue>());

						return KnownHosts;
					}
					, "Error reading known hosts data from database (f_Start)"
				)
			;
		}

		if (m_RetentionDays && !m_fCleanup)
		{
			co_await f_PerformLocalCleanup().f_Wrap() > fg_LogError("LogLocalStore", "Failed to perform initial database cleanup");

			m_CleanupTimerSubscription = co_await fg_RegisterTimer
				(
					24_hours
					, [this]() -> TCFuture<void>
					{
						auto Result = co_await f_PerformLocalCleanup().f_Wrap();

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
				g_ActorFunctor / [this](TCDistributedActor<CDistributedAppLogReporter> _Actor, CTrustedActorInfo _TrustInfo) -> TCFuture<void>
				{
					auto Result = co_await f_LogReporterInterfaceAdded(fg_TempCopy(_Actor), _TrustInfo).f_Wrap();
					if (!Result)
						DMibLogWithCategory(LogLocalStore, Error, "Failures adding new log reporter: {}", Result.f_GetExceptionStr());

					co_return {};
				}
				, g_ActorFunctor / [this](TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _TrustInfo) -> TCFuture<void>
				{
					co_await f_LogReporterInterfaceRemoved(_Actor, fg_Move(_TrustInfo));

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
			TCDistributedActorInterface<CDistributedAppLogReporter> _LogReporter
			, CTrustedActorInfo _TrustInfo
		)
	{
		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Subscription = g_ActorSubscription / [this, _TrustInfo, LogInterfaceWeak = _LogReporter.f_Weak(), TrustInfo = _TrustInfo]() mutable -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;
				co_await Internal.f_LogReporterInterfaceRemoved(LogInterfaceWeak, fg_Move(TrustInfo)).f_Wrap()
					> fg_LogError("Malterlib/Concurrency/Log", "Error removing log reporter")
				;

				co_return {};
			}
		;

		auto Result = co_await Internal.f_LogReporterInterfaceAdded(fg_Move(_LogReporter), _TrustInfo).f_Wrap();
		if (!Result)
			DMibLogWithCategory(LogLocalStore, Error, "Failures adding extra log reporter: {}", Result.f_GetExceptionStr());

		co_return fg_Move(Subscription);
	}
}
