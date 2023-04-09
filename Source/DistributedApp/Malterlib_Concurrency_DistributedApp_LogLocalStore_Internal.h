// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency
{
	using namespace NDatabase;
	using namespace NStr;
	using namespace NIntrusive;
	using namespace NContainer;
	using namespace NException;
	using namespace NStorage;

	struct CDistributedAppLogStoreLocal::CInternal
	{
		struct CLog;
		struct CRemoteLogReporterInterface;

		struct CLogReporter
		{
			auto &f_GetID() const
			{
				return TCMap<TCWeakDistributedActor<CActor>, CLogReporter>::fs_GetKey(*this);
			}

			CLog *m_pLog = nullptr;
			CRemoteLogReporterInterface *m_pInterface = nullptr;
			CDistributedAppLogReporter::CLogReporter m_Reporter;
			TCActorSequencerAsync<uint32> m_WriteSequencer;
			DMibListLinkDS_Link(CLogReporter, m_LinkInterface);
			DMibListLinkDS_Link(CLogReporter, m_LinkFailed);
		};

		struct CLog
		{
			CDistributedAppLogReporter::CLogInfoKey const &f_GetKey() const
			{
				return TCMap<CDistributedAppLogReporter::CLogInfoKey, CLog>::fs_GetKey(*this);
			}

			CDistributedAppLogReporter::CLogInfo m_Info;
			uint64 m_LastSeenUniqueSequence = 0;
			mint m_ActiveRefCount = 0;

			TCActorSequencerAsync<void> m_LogSequencer;
			TCMap<TCWeakDistributedActor<CActor>, CLogReporter> m_LogReporters;

			TCSharedPointer<CCanDestroyTracker> m_pCanDestroy;
		};

		struct CRemoteLogReporterInterface
		{
			TCDistributedActorInterface<CDistributedAppLogReporter> m_Actor;
			DMibListLinkDS_List(CLogReporter, m_LinkInterface) m_Reporters;
		};

		struct CLogSubscription
		{
			TCVector<CDistributedAppLogReader_LogFilter> m_Filters;
			TCActorFunctor<TCFuture<void> (CDistributedAppLogReader::CLogChange &&_Change)> m_fOnChange;

			TCMap<CDistributedAppLogReporter::CLogInfoKey, CDistributedAppLogReader::CLogChange> m_QueuedChanges;
		};

		struct CEntryFilter
		{
			CDistributedAppLogReader_LogEntryFilter m_Filter;
			bool m_bIsDataFiltered = false;
		};

		struct CEntrySubscriptionFilter
		{
			CDistributedAppLogReader_LogEntrySubscriptionFilter m_Filter;
			bool m_bIsDataFiltered = false;
		};

		struct CEntrySubscription
		{
			TCMap<CDistributedAppLogReporter::CLogInfoKey, zuint64> m_LastSeenEntry;
			TCActorFunctor<TCFuture<void> (CDistributedAppLogReader_LogKeyAndEntry &&_Entry)> m_fOnEntry;
			TCVector<CDistributedAppLogReader_LogKeyAndEntry> m_QueuedEntries;
			TCVector<CEntrySubscriptionFilter> m_Filters;
		};

		CInternal
			(
				CDistributedAppLogStoreLocal *_pThis
				, TCActor<CActorDistributionManager> const &_DistributionManager
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
			)
			: m_pThis(_pThis)
			, m_DistributionManager(_DistributionManager)
			, m_TrustManager(_TrustManager)
		{
		}

		TCFuture<void> f_Start();

		TCFuture<void> f_UpdateLogForReporter(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey, TCWeakDistributedActor<CActor> const &_Actor);

		TCFuture<void> f_LogReporterInterfaceAdded(TCDistributedActorInterface<CDistributedAppLogReporter> &&_Actor, CTrustedActorInfo const &_TrustInfo);
		TCFuture<void> f_LogReporterInterfaceRemoved(TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo);
		TCFuture<void> f_LogInfoChanged(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey, bool _bWasAdded);
		TCFuture<uint32> f_NewLogEntries
			(
				CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
				, NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> const &_pEntries
			)
		;
		TCFuture<void> f_StoreLogEntries
			(
				CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey
				, NLogStoreLocalDatabase::CLogEntryKey const &_DatabaseKey
				, NStorage::TCSharedPointer<TCVector<CDistributedAppLogReporter::CLogEntry> const> const &_pEntries
			)
		;
		TCFuture<void> f_CleanupLogReporter(CDistributedAppLogReporter::CLogInfoKey const &_LogInfoKey);

		auto f_GetReportEntriesFunctor(CDistributedAppLogReporter::CLogInfoKey _LogInfoKey, NLogStoreLocalDatabase::CLogEntryKey _DatabaseKey)
			-> TCActorFunctorWithID<TCFuture<CDistributedAppLogReporter::CReportEntriesResult> (NContainer::TCVector<CDistributedAppLogReporter::CLogEntry> &&_Entries)>
		;

		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> f_Cleanup(NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction);
		TCFuture<void> f_PerformLocalCleanup();

		void f_ScheduleFailedRetry();
		TCFuture<void> f_RetryFailedReporters();

		CStr f_GetLogUpdateFailedMessage(CLogReporter const &_Reporter);

		void f_Subscription_Entries(CLog const &_Log, TCVector<CDistributedAppLogReporter::CLogEntry> const &_Entries);
		void f_Subscription_LogInfoChanged(CLog const &_Log);

		template <typename tf_CKey, typename tf_CInfoOrKey>
		tf_CKey f_GetDatabaseKey(tf_CInfoOrKey const &_LogInfo) const;

		TCMap<TCWeakDistributedActor<CActor>, CRemoteLogReporterInterface> m_LogInterfaces;

		DMibListLinkDS_List(CLogReporter, m_LinkFailed) m_FailedReporters;

		CDistributedAppLogStoreLocal *m_pThis;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CDatabaseActor> m_Database;

		TCMap<CDistributedAppLogReporter::CLogInfoKey, CLog> m_Logs;

		TCSharedPointer<CCanDestroyTracker> m_pCanDestroyStoringLocal = fg_Construct();

		TCTrustedActorSubscription<CDistributedAppLogReporter> m_LogsInterfaceSubscription;
		TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction)> m_fCleanup;
		CActorSubscription m_CleanupTimerSubscription;
		CStr m_Prefix;

		TCMap<CStr, CLogSubscription> m_LogSubscriptions;
		TCMap<CStr, CEntrySubscription> m_EntrySubscriptions;

		uint64 m_RetentionDays = 0;

		bool m_bStarted = false;
		bool m_bStarting = false;
		bool m_bOwnDatabase = false;
		bool m_bScheduledFailedReportersRetry = false;
	};
}

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.hpp"
