// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Filter.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	auto CDistributedAppSensorStoreLocal::f_SubscribeSensors
		(
			TCVector<CDistributedAppSensorReader_SensorFilter> _Filters
			, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader::CSensorChange _Change)> _fOnChange
		)
		-> TCFuture<CActorSubscription>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		if (_fOnChange.f_IsEmpty())
			co_return DMibErrorInstance("Local store not yet started");

		CStr SubscriptionID = NCryptography::fg_RandomID(Internal.m_SensorSubscriptions);
		Internal.m_SensorSubscriptions[SubscriptionID].m_Filters = fg_Move(_Filters);

		CInternal::CSensorSubscription *pSubscription = nullptr;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSubscription = Internal.m_SensorSubscriptions.f_FindEqual(SubscriptionID);
					if (!pSubscription)
						return DMibErrorInstance("Subscription deleted");
					return {};
				}
			)
		;

		// Send initial
		auto Sensors = f_GetSensors(CDistributedAppSensorReader::CGetSensors{.m_Filters = pSubscription->m_Filters});
		for (auto iSensor = co_await fg_Move(Sensors).f_GetPipelinedIterator(); iSensor; co_await ++iSensor)
		{
			TCFutureVector<void> Results;
			for (auto &Sensor : *iSensor)
				_fOnChange(fg_Move(Sensor)) > Results;

			co_await fg_AllDone(Results);
		}

		// Send readings that came in while sending initial
		TCFutureVector<void> Results;
		for (auto &Change : pSubscription->m_QueuedChanges)
			_fOnChange(fg_Move(Change)) > Results;
		pSubscription->m_QueuedChanges.f_Clear();

		pSubscription->m_fOnChange = fg_Move(_fOnChange);

		co_await fg_AllDone(Results);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				auto pSubscription = Internal.m_SensorSubscriptions.f_FindEqual(SubscriptionID);
				TCFuture<void> DestroyFuture;
				if (pSubscription)
				{
					DestroyFuture = fg_Move(pSubscription->m_fOnChange).f_Destroy();
					Internal.m_SensorSubscriptions.f_Remove(pSubscription);
				}

				if (DestroyFuture.f_IsValid())
					co_await fg_Move(DestroyFuture);

				co_return {};
			}
		;
	}

	auto CDistributedAppSensorStoreLocal::f_SubscribeSensorReadings
		(
			TCVector<CDistributedAppSensorReader_SensorReadingSubscriptionFilter> _Filters
			, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading _Reading)> _fOnReading
		)
		-> TCFuture<CActorSubscription>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		if (_fOnReading.f_IsEmpty())
			co_return DMibErrorInstance("Local store not yet started");

		CStr SubscriptionID = NCryptography::fg_RandomID(Internal.m_ReadingSubscriptions);
		Internal.m_ReadingSubscriptions[SubscriptionID].m_Filters = fg_Move(_Filters);

		CInternal::CReadingSubscription *pSubscription = nullptr;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSubscription = Internal.m_ReadingSubscriptions.f_FindEqual(SubscriptionID);
					if (!pSubscription)
						return DMibErrorInstance("Subscription deleted");
					return {};
				}
			)
		;

		bool bNeedInitial = false;
		CDistributedAppSensorReader::CGetSensorReadings GetReadingsParams;
		for (auto &Filter : pSubscription->m_Filters)
		{
			if (Filter.m_MinSequence || Filter.m_MinTimestamp)
				bNeedInitial = true;
			GetReadingsParams.m_Filters.f_Insert(Filter.f_ToReadingFilter());
		}

		if (bNeedInitial)
		{
			auto SensorReadings = f_GetSensorReadings(fg_Move(GetReadingsParams));
			for (auto iReadings = co_await fg_Move(SensorReadings).f_GetPipelinedIterator(); iReadings; co_await ++iReadings)
			{
				TCFutureVector<void> Results;
				for (auto &Reading : *iReadings)
				{
					auto &LastSeenReading = pSubscription->m_LastSeenReading[Reading.m_SensorInfoKey];
					LastSeenReading = fg_Max(LastSeenReading, Reading.m_Reading.m_UniqueSequence);

					_fOnReading(fg_Move(Reading)) > Results;
				}

				co_await fg_AllDone(Results);
			}
		}

		// Send readings that came in while sending initial
		TCFutureVector<void> Results;
		for (auto &Reading : pSubscription->m_QueuedReadings)
		{
			auto *pLastSeen = pSubscription->m_LastSeenReading.f_FindEqual(Reading.m_SensorInfoKey);
			if (pLastSeen && Reading.m_Reading.m_UniqueSequence <= *pLastSeen)
				continue;
			_fOnReading(fg_Move(Reading)) > Results;
		}
		pSubscription->m_QueuedReadings.f_Clear();

		pSubscription->m_fOnReading = fg_Move(_fOnReading);

		co_await fg_AllDone(Results);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				auto pSubscription = Internal.m_ReadingSubscriptions.f_FindEqual(SubscriptionID);
				TCFuture<void> DestroyFuture;
				if (pSubscription)
				{
					DestroyFuture = fg_Move(pSubscription->m_fOnReading).f_Destroy();
					Internal.m_ReadingSubscriptions.f_Remove(pSubscription);
				}

				if (DestroyFuture.f_IsValid())
					co_await fg_Move(DestroyFuture);

				co_return {};
			}
		;
	}

	auto CDistributedAppSensorStoreLocal::f_SubscribeSensorStatus
		(
			TCVector<CDistributedAppSensorReader_SensorStatusFilter> _Filters
			, TCActorFunctor<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading _Reading)> _fOnReading
		)
		-> TCFuture<CActorSubscription>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		if (_fOnReading.f_IsEmpty())
			co_return DMibErrorInstance("Local store not yet started");

		CStr SubscriptionID = NCryptography::fg_RandomID(Internal.m_StatusSubscriptions);
		Internal.m_StatusSubscriptions[SubscriptionID].m_Filters = fg_Move(_Filters);

		CInternal::CStatusSubscription *pSubscription = nullptr;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSubscription = Internal.m_StatusSubscriptions.f_FindEqual(SubscriptionID);
					if (!pSubscription)
						return DMibErrorInstance("Subscription deleted");
					return {};
				}
			)
		;

		// Send initial
		auto SensorReadings = f_GetSensorStatus(CDistributedAppSensorReader::CGetSensorStatus{.m_Filters = pSubscription->m_Filters});

		for (auto iReadings = co_await fg_Move(SensorReadings).f_GetPipelinedIterator(); iReadings; co_await ++iReadings)
		{
			TCFutureVector<void> Results;
			for (auto &Reading : *iReadings)
			{
				auto &LastSeenReading = pSubscription->m_LastSeenReading[Reading.m_SensorInfoKey];
				LastSeenReading = fg_Max(LastSeenReading, Reading.m_Reading.m_UniqueSequence);

				_fOnReading(fg_Move(Reading)) > Results;
			}

			co_await fg_AllDone(Results);
		}

		// Send readings that came in while sending initial
		TCFutureVector<void> Results;
		for (auto &Reading : pSubscription->m_QueuedReadings)
		{
			auto *pLastSeen = pSubscription->m_LastSeenReading.f_FindEqual(Reading.m_SensorInfoKey);
			if (pLastSeen && Reading.m_Reading.m_UniqueSequence <= *pLastSeen)
				continue;
			_fOnReading(fg_Move(Reading)) > Results;
		}
		pSubscription->m_QueuedReadings.f_Clear();

		pSubscription->m_fOnReading = fg_Move(_fOnReading);

		co_await fg_AllDone(Results);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				auto pSubscription = Internal.m_StatusSubscriptions.f_FindEqual(SubscriptionID);
				TCFuture<void> DestroyFuture;
				if (pSubscription)
				{
					DestroyFuture = fg_Move(pSubscription->m_fOnReading).f_Destroy();
					Internal.m_StatusSubscriptions.f_Remove(pSubscription);
				}

				if (DestroyFuture.f_IsValid())
					co_await fg_Move(DestroyFuture);

				co_return {};
			}
		;
	}

	void CDistributedAppSensorStoreLocal::CInternal::f_Subscription_Readings
		(
			CSensor const &_Sensor
			, TCVector<CDistributedAppSensorReporter::CSensorReading> const &_Readings
			, NDatabase::CDatabaseSubReadTransaction &_Transaction
		)
	{
		auto SensorDatabaseKey = f_GetDatabaseKey<CSensorKey>(_Sensor.m_Info);
		auto SensorKey = _Sensor.f_GetKey();
		auto *pKnownHost = m_KnownHosts.f_FindEqual(_Sensor.m_Info.m_HostID);

		NTime::CTime Now = NTime::CTime::fs_NowUTC();

		NSensorStore::CFilterSensorKeyContext FilterContext{.m_pTransaction = &_Transaction, .m_ThisHostID = m_ThisHostID, .m_Prefix = m_Prefix};

		for (auto &Reading : _Readings)
		{
			for (auto &Subscription : m_StatusSubscriptions)
			{
				{
					bool bPassFilter = Subscription.m_Filters.f_IsEmpty();

					for (auto &Filter : Subscription.m_Filters)
					{
						if (!NSensorStore::fg_FilterSensorKey(SensorDatabaseKey, Filter.m_SensorFilter, FilterContext, &_Sensor.m_Info, pKnownHost))
							continue;

						auto PauseReportingFor = _Sensor.m_Info.m_PauseReportingFor;
						if (SensorDatabaseKey.m_HostID)
						{
							NSensorStoreLocalDatabase::CKnownHostKey KnowHostKey{.m_DbPrefix = m_Prefix, .m_HostID = SensorDatabaseKey.m_HostID};
							NSensorStoreLocalDatabase::CKnownHostValue KnowHostValue;
							_Transaction.f_Get(KnowHostKey, KnowHostValue);
							PauseReportingFor = fg_MaxValidFloat(PauseReportingFor, KnowHostValue.m_PauseReportingFor);
						}

						if (!NSensorStore::fg_FilterSensorValueStatus(_Sensor, Reading, PauseReportingFor, Filter.m_Flags, Now))
							continue;

						bPassFilter = true;
						break;
					}

					if (!bPassFilter)
						continue;
				}

				CDistributedAppSensorReader_SensorKeyAndReading OutReading;
				OutReading.m_Reading = Reading;
				OutReading.m_SensorInfoKey = SensorKey;

				if (Subscription.m_fOnReading.f_IsEmpty())
					Subscription.m_QueuedReadings.f_Insert(fg_Move(OutReading));
				else
					Subscription.m_fOnReading(fg_Move(OutReading)) > fg_LogError("SensorLocalStore", "Failed to send sensor reading to subscription");
			}

			for (auto &Subscription : m_ReadingSubscriptions)
			{
				{
					bool bPassFilter = Subscription.m_Filters.f_IsEmpty();

					for (auto &Filter : Subscription.m_Filters)
					{
						if (!NSensorStore::fg_FilterSensorKey(SensorDatabaseKey, Filter.m_SensorFilter, FilterContext, &_Sensor.m_Info, pKnownHost))
							continue;

						if (!NSensorStore::fg_FilterSensorReadingValueWithSensor(_Sensor, Reading, Filter.m_Flags))
							continue;

						bPassFilter = true;
						break;
					}

					if (!bPassFilter)
						continue;
				}

				CDistributedAppSensorReader_SensorKeyAndReading OutReading;
				OutReading.m_Reading = Reading;
				OutReading.m_SensorInfoKey = SensorKey;

				if (Subscription.m_fOnReading.f_IsEmpty())
					Subscription.m_QueuedReadings.f_Insert(fg_Move(OutReading));
				else
					Subscription.m_fOnReading(fg_Move(OutReading)) > fg_LogError("SensorLocalStore", "Failed to send sensor reading to subscription");
			}
		}
	}

	void CDistributedAppSensorStoreLocal::CInternal::f_Subscription_SensorInfoChanged
		(
			CSensor const &_Sensor
			, CKnownHostValue const *_pKnownHostValue
		)
	{
		if (m_SensorSubscriptions.f_IsEmpty())
			return;

		auto DatabaseKey = f_GetDatabaseKey<CSensorKey>(_Sensor.m_Info);

		NSensorStore::CFilterSensorKeyContext FilterContext{.m_ThisHostID = m_ThisHostID, .m_Prefix = m_Prefix};

		auto Info = _Sensor.m_Info;
		if (_pKnownHostValue)
			Info.m_PauseReportingFor = fg_MaxValidFloat(Info.m_PauseReportingFor, _pKnownHostValue->m_PauseReportingFor);

		auto *pKnownHost = m_KnownHosts.f_FindEqual(_Sensor.m_Info.m_HostID);
		
		for (auto &Subscription : m_SensorSubscriptions)
		{
			if (!NSensorStore::fg_FilterSensorKey(DatabaseKey, Subscription.m_Filters, FilterContext, &_Sensor.m_Info, pKnownHost))
				continue;

			if (Subscription.m_fOnChange)
				Subscription.m_fOnChange(Info) > fg_LogError("SensorLocalStore", "Failed to send sensor change to subscription");
			else
				Subscription.m_QueuedChanges[_Sensor.f_GetKey()] = Info;
		}
	}
}
