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

		TCFuture<void> f_StartWithDatabase(TCActor<NDatabase::CDatabaseActor> &&_Database, NStr::CStr const &_Prefix);
		TCFuture<void> f_StartWithDatabasePath(NStr::CStr const &_DatabasePath);

		TCFuture<CActorSubscription> f_AddExtraSensorReporter(TCDistributedActorInterface<CDistributedAppSensorReporter> &&_SensorReporter, CTrustedActorInfo const &_TrustInfo);
		TCFuture<CDistributedAppSensorReporter::CSensorReporter> f_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo);

		TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> f_GetSensors(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize);
		auto f_GetSensorReadings(CDistributedAppSensorReader_SensorReadingFilter &&_Filter, uint32 _BatchSize)
			-> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
		;
		auto f_GetSensorStatus(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
			-> TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>
		;

	private:
		struct CInternal;

		TCFuture<void> fp_Destroy() override;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}
