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
		CDistributedAppSensorStoreLocal(TCActor<CActorDistributionManager> const &_DistributionManager, TCActor<CDistributedActorTrustManager> const &_TrustManager);
		~CDistributedAppSensorStoreLocal();

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

		TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> f_GetSensors(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize);
		auto f_GetSensorReadings(CDistributedAppSensorReader_SensorReadingFilter &&_Filter, uint32 _BatchSize)
			-> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
		;
		auto f_GetSensorStatus(CDistributedAppSensorReader_SensorStatusFilter &&_Filter, uint32 _BatchSize)
			-> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
		;

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

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
