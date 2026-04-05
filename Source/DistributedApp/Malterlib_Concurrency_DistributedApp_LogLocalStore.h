// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedAppLogReporter>
#include <Mib/Concurrency/DistributedAppLogReader>
#include <Mib/Database/DatabaseActor>

namespace NMib::NConcurrency
{
	struct CDistributedAppLogStoreLocal : public CActor
	{
		CDistributedAppLogStoreLocal(TCActor<CActorDistributionManager> const &_DistributionManager, TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CDistributedAppLogStoreLocal();

		TCFuture<void> f_DestroyRemote();

		TCFuture<void> f_StartWithDatabase
			(
				TCActor<NDatabase::CDatabaseActor> _Database
				, NStr::CStr _Prefix
				, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)> _fCleanup
			)
		;
		TCFuture<void> f_StartWithDatabasePath(NStr::CStr _DatabasePath, uint64 _RetentionDays);

		TCFuture<CActorSubscription> f_AddExtraLogReporter(TCDistributedActorInterface<CDistributedAppLogReporter> _LogReporter, CTrustedActorInfo _TrustInfo);
		TCFuture<CDistributedAppLogReporter::CLogReporter> f_OpenLogReporter(CDistributedAppLogReporter::CLogInfo _LogInfo);

		TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> f_GetLogs(CDistributedAppLogReader::CGetLogs _Params);
		auto f_GetLogEntries(CDistributedAppLogReader::CGetLogEntries _Params) -> TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>>;
		auto f_SubscribeLogs
			(
				NContainer::TCVector<CDistributedAppLogReader_LogFilter> _Filters
				, TCActorFunctor<TCFuture<void> (CDistributedAppLogReader::CLogChange _Change)> _fOnChange
			)
			-> TCFuture<CActorSubscription>
		;
		auto f_SubscribeLogEntries(CDistributedAppLogReader::CSubscribeLogEntries _Params) -> TCFuture<CActorSubscription>;

		TCFuture<void> f_SeenHosts(NContainer::TCMap<NStr::CStr, NTime::CTime> _HostsSeen);
		TCFuture<void> f_RemoveHosts(NContainer::TCSet<NStr::CStr> _RemovedHostIDs);
		TCFuture<uint32> f_RemoveLogs(NContainer::TCSet<CDistributedAppLogReporter::CLogInfoKey> _LogInfoKeys);

		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> f_PrepareForCleanup(NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction);

		struct CCleanupHelper
		{
			bool f_Init(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NStr::CStr const &_Prefix, NTime::CTime &o_Time);
			bool f_DeleteOne(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NTime::CTime &o_Time);

		private:
			NDatabase::CDatabaseCursorWrite mp_iEntryByTime = NDatabase::CDatabaseCursorWrite::fs_InitEmpty();
		};

	private:
		struct CInternal;

		TCFuture<void> fp_Destroy() override;
		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fp_Cleanup(NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction);

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
