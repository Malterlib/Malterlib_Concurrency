// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedAppSensorReporter>
#include <Mib/Concurrency/DistributedAppSensorReader>
#include <Mib/Database/DatabaseActor>

namespace NMib::NConcurrency
{
	struct CDistributedAppSensorStoreLocal : public CActor
	{
		struct CSeenHost
		{
			NStorage::TCOptional<NTime::CTime> m_TimeSeen;
			NStorage::TCOptional<fp32> m_PauseReportingFor;
		};
		
		CDistributedAppSensorStoreLocal(TCActor<CActorDistributionManager> const &_DistributionManager, TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CDistributedAppSensorStoreLocal();

		TCFuture<void> f_DestroyRemote();

		TCFuture<void> f_StartWithDatabase
			(
				TCActor<NDatabase::CDatabaseActor> &&_Database
				, NStr::CStr const &_Prefix
				, TCActorFunctor<TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> (NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction)> &&_fCleanup
			)
		;
		TCFuture<void> f_StartWithDatabasePath(NStr::CStr const &_DatabasePath, uint64 _RetentionDays);

		TCFuture<CActorSubscription> f_AddExtraSensorReporter(TCDistributedActorInterface<CDistributedAppSensorReporter> &&_SensorReporter, CTrustedActorInfo const &_TrustInfo);
		TCFuture<CDistributedAppSensorReporter::CSensorReporter> f_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo);

		TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> f_GetSensors(CDistributedAppSensorReader::CGetSensors &&_Params);
		auto f_GetSensorReadings(CDistributedAppSensorReader::CGetSensorReadings &&_Params) -> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>;
		auto f_GetSensorStatus(CDistributedAppSensorReader::CGetSensorStatus &&_Params)
			-> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
		;
		auto f_SubscribeSensors
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> &&_Filters
				, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader::CSensorChange &&_Change)> &&_fOnChange
			)
			-> TCFuture<CActorSubscription>
		;
		auto f_SubscribeSensorReadings
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorReadingSubscriptionFilter> &&_Filters
				, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading &&_Reading)> &&_fOnReading
			)
			-> TCFuture<CActorSubscription>
		;
		auto f_SubscribeSensorStatus
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorStatusFilter> &&_Filters
				, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading &&_Reading)> &&_fOnReading
			)
			-> TCFuture<CActorSubscription>
		;

		TCFuture<void> f_SeenHosts(NContainer::TCMap<NStr::CStr, CSeenHost> &&_HostsSeen);
		TCFuture<void> f_RemoveHosts(NContainer::TCSet<NStr::CStr> &&_RemovedHostIDs);
		TCFuture<uint32> f_RemoveSensors(NContainer::TCSet<CDistributedAppSensorReporter::CSensorInfoKey> &&_SensorInfoKeys);
		TCFuture<uint32> f_SnoozeSensors(NContainer::TCSet<CDistributedAppSensorReporter::CSensorInfoKey> &&_SensorInfoKeys, NTime::CTimeSpan const &_SnoozeDuration);

		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> f_PrepareForCleanup(NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction);

		struct CCleanupHelper
		{
			bool f_Init(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NStr::CStr const &_Prefix, NTime::CTime &o_Time);
			bool f_DeleteOne(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NTime::CTime &o_Time);

		private:
			NDatabase::CDatabaseCursorWrite mp_iReadingByTime = NDatabase::CDatabaseCursorWrite::fs_InitEmpty();
		};

	private:
		struct CInternal;

		TCFuture<void> fp_Destroy() override;
		TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fp_Cleanup(NDatabase::CDatabaseActor::CTransactionWrite &&_WriteTransaction);

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
