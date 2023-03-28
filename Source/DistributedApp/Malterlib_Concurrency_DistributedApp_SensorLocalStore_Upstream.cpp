// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	void CDistributedAppSensorStoreLocal::CInternal::f_ScheduleFailedRetry()
	{
		if (m_bScheduledFailedReportersRetry)
			return;

		m_bScheduledFailedReportersRetry = true;
		fg_Timeout(60.0) > [this]
			{
				fg_CallSafe(*this, &CInternal::f_RetryFailedReporters) > [](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(SensorLocalStore, Error, "Failures during retry opening sensor reporters: {}", _Result.f_GetExceptionStr());
					}
				;
			}
		;
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_UpdateSensorForReporter
		(
			CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey
			, TCWeakDistributedActor<CActor> const &_WeakActor
		)
	{
		CSensor *pSensor;
		CSensorReporter *pSensorReporter;
		CRemoteSensorReporterInterface *pSensorInterface;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					if (m_pThis->f_IsDestroyed())
						return DMibErrorInstance("Shutting down");

					pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
					if (!pSensor)
						return DMibErrorInstance("Aborted");

					pSensorReporter = pSensor->m_SensorReporters.f_FindEqual(_WeakActor);
					if (!pSensorReporter)
						return DMibErrorInstance("Aborted");

					pSensorInterface = m_SensorInterfaces.f_FindEqual(_WeakActor);
					if (!pSensorInterface)
						return DMibErrorInstance("Aborted");

					return {};
				}
			)
		;

		auto SequenceSubscription = co_await pSensorReporter->m_WriteSequencer.f_Sequence();

		auto OnFailure = g_OnScopeExit / [&]
			{
				pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
				if (!pSensor)
					return;

				pSensorReporter = pSensor->m_SensorReporters.f_FindEqual(_WeakActor);
				if (!pSensorReporter)
					return;

				m_FailedReporters.f_Insert(pSensorReporter);
				f_ScheduleFailedRetry();
			}
		;

		// Destroy old reporter
		if (pSensorReporter->m_Reporter.m_fReportReadings)
			co_await fg_Move(pSensorReporter->m_Reporter.m_fReportReadings).f_Destroy();

		auto Reporter = co_await pSensorInterface->m_Actor.f_CallActor(&CDistributedAppSensorReporter::f_OpenSensorReporter)(pSensor->m_Info);
		if (!Reporter.m_fReportReadings)
			co_return DMibErrorInstance("Invalid report readings functor returned from f_OpenSensorReporter");

		if (Reporter.m_LastSeenUniqueSequence < pSensor->m_LastSeenUniqueSequence)
		{
			auto ReadTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionRead);

			auto DatabaseKey = f_GetDatabaseKey<CSensorReadingKey>(pSensor->m_Info);
			DatabaseKey.m_UniqueSequence = Reporter.m_LastSeenUniqueSequence;

			auto iSensorReading = ReadTransaction.m_Transaction.f_ReadCursor((CSensorKey const &)DatabaseKey);
			iSensorReading.f_FindLowerBound(DatabaseKey);

			if (iSensorReading)
			{
				auto Key = iSensorReading.f_Key<CSensorReadingKey>();
				if (Key.m_UniqueSequence == Reporter.m_LastSeenUniqueSequence)
					++iSensorReading;
			}

			NContainer::TCVector<CDistributedAppSensorReporter::CSensorReading> Batch;

			static constexpr mint c_BatchSize = 32768;

			for (; iSensorReading; ++iSensorReading)
			{
				auto Key = iSensorReading.f_Key<CSensorReadingKey>();
				auto Value = iSensorReading.f_Value<CSensorReadingValue>();

				Batch.f_Insert(fg_Move(Value).f_Reading(Key));

				if (Batch.f_GetLen() >= c_BatchSize)
					co_await Reporter.m_fReportReadings(fg_Move(Batch));
			}

			if (!Batch.f_IsEmpty())
				co_await Reporter.m_fReportReadings(fg_Move(Batch));
		}
		else if (Reporter.m_LastSeenUniqueSequence > pSensor->m_LastSeenUniqueSequence)
		{
			DMibLogWithCategory
				(
					SensorLocalStore
					, Warning
					, "Detected incorrect database last unique sequence. Reset from {} to {}\n{}"
					, pSensor->m_LastSeenUniqueSequence
					, Reporter.m_LastSeenUniqueSequence
					, pSensor->m_Info
				)
			;
			pSensor->m_LastSeenUniqueSequence = Reporter.m_LastSeenUniqueSequence;
		}

		pSensorReporter->m_Reporter = fg_Move(Reporter);

		OnFailure.f_Clear();

		pSensorReporter->m_LinkFailed.f_Unlink();

		co_return {};
	}

	CStr CDistributedAppSensorStoreLocal::CInternal::f_GetSensorUpdateFailedMessage(CSensorReporter const &_Reporter)
	{
		return "Failed to update sensor '{}' for reporter on host '{}'"_f << _Reporter.m_pSensor->f_GetKey() << _Reporter.m_pInterface->m_Actor->f_GetHostInfo().m_RealHostID;
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorInfoChanged(CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey, bool _bWasCreated)
	{
		auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);

		if (!pSensor)
			co_return {};

		TCActorResultVector<void> Results;

		if (_bWasCreated)
		{
			for (auto &SensorInterface : m_SensorInterfaces)
			{
				auto &WeakActor = m_SensorInterfaces.fs_GetKey(SensorInterface);

				auto &SensorReporter = pSensor->m_SensorReporters[WeakActor];
				SensorReporter.m_pSensor = pSensor;
				SensorReporter.m_pInterface = &SensorInterface;
				SensorInterface.m_Reporters.f_Insert(SensorReporter);
				fg_CallSafe(*this, &CInternal::f_UpdateSensorForReporter, _SensorInfoKey, WeakActor).f_Dispatch() % f_GetSensorUpdateFailedMessage(SensorReporter) > Results.f_AddResult();
			}
		}
		else
		{
			for (auto &SensorReporter : pSensor->m_SensorReporters)
			{
				auto &WeakActor = pSensor->m_SensorReporters.fs_GetKey(SensorReporter);
				fg_CallSafe(*this, &CInternal::f_UpdateSensorForReporter, _SensorInfoKey, WeakActor).f_Dispatch() % f_GetSensorUpdateFailedMessage(SensorReporter) > Results.f_AddResult();
			}
		}

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_RetryFailedReporters()
	{
		TCActorResultVector<void> Results;

		for (auto iFailed = m_FailedReporters.f_GetIterator(); iFailed;)
		{
			auto &Failed = *iFailed;
			++iFailed;

			auto &SensorKey = Failed.m_pSensor->f_GetKey();

			fg_CallSafe(*this, &CInternal::f_UpdateSensorForReporter, SensorKey, Failed.f_GetID()).f_Dispatch() % f_GetSensorUpdateFailedMessage(Failed) > Results.f_AddResult();
		}

		auto AwaitedResults = co_await Results.f_GetResults();

		m_bScheduledFailedReportersRetry = false;
		if (!m_FailedReporters.f_IsEmpty())
			f_ScheduleFailedRetry();

		co_await (fg_Move(AwaitedResults) | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_NewSensorReadings
		(
			CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey
			, TCVector<CDistributedAppSensorReporter::CSensorReading> const &_Readings
		)
	{
		auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
		if (!pSensor)
			co_return {};

		TCVector<TCVector<CDistributedAppSensorReporter::CSensorReading>> ReadingsChunks;

		static constexpr mint c_ChunkSize = 32768;

		mint nReadings = _Readings.f_GetLen();

		if (nReadings < c_ChunkSize)
			ReadingsChunks.f_Insert(_Readings);
		else
		{
			for (mint iStartReading = 0; iStartReading < nReadings;)
			{
				mint ThisTime = fg_Min(nReadings - iStartReading, c_ChunkSize);

				ReadingsChunks.f_Insert(TCVector<CDistributedAppSensorReporter::CSensorReading>(_Readings.f_GetArray() + iStartReading, ThisTime));

				iStartReading += ThisTime;
			}
		}

		TCActorResultVector<void> Results;

		for (auto &Readings : ReadingsChunks)
		{
			for (auto &Reporter : pSensor->m_SensorReporters)
			{
				auto WeakActor = pSensor->m_SensorReporters.fs_GetKey(Reporter);
				Reporter.m_WriteSequencer / [this, Readings, WeakActor, _SensorInfoKey](CActorSubscription &&_DoneSubscription) mutable -> TCFuture<void>
					{
						auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
						if (!pSensor)
							co_return {};

						auto pReporter = pSensor->m_SensorReporters.f_FindEqual(WeakActor);
						if (!pReporter)
							co_return {};

						if (pReporter->m_LinkFailed.f_IsInList())
							co_return {};

						if (!pReporter->m_Reporter.m_fReportReadings)
							co_return {};

						co_await pReporter->m_Reporter.m_fReportReadings(fg_Move(Readings));

						(void)_DoneSubscription;

						co_return {};
					}
					> Results.f_AddResult()
				;
			}
		}

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorReporterInterfaceAdded
		(
			TCDistributedActorInterface<CDistributedAppSensorReporter> &&_Actor
			, CTrustedActorInfo const &_TrustInfo
		)
	{
		TCWeakDistributedActor<CActor> WeakActor = _Actor.f_Weak();
		auto &SensorInterface = m_SensorInterfaces[WeakActor];
		SensorInterface.m_Actor = fg_Move(_Actor);

		TCActorResultVector<void> Results;
		for (auto &Sensor : m_Sensors)
		{
			auto &SensorReporter = Sensor.m_SensorReporters[WeakActor];
			SensorReporter.m_pSensor = &Sensor;
			SensorReporter.m_pInterface = &SensorInterface;
			SensorInterface.m_Reporters.f_Insert(SensorReporter);

			auto &SensorKey = m_Sensors.fs_GetKey(Sensor);

			fg_CallSafe(*this, &CInternal::f_UpdateSensorForReporter, SensorKey, WeakActor).f_Dispatch() % f_GetSensorUpdateFailedMessage(SensorReporter) > Results.f_AddResult();
		}

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorReporterInterfaceRemoved(TCWeakDistributedActor<CActor> const &_Actor, CTrustedActorInfo &&_TrustInfo)
	{
		auto *pInterface = m_SensorInterfaces.f_FindEqual(_Actor);

		if (!pInterface)
			co_return {};

		TCActorResultVector<void> Results;
		for (auto iReporter = pInterface->m_Reporters.f_GetIterator(); iReporter;)
		{
			auto &Reporter = *iReporter;
			++iReporter;
			Reporter.m_WriteSequencer.f_Abort() > Results.f_AddResult();
			fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Results.f_AddResult();
			Reporter.m_pSensor->m_SensorReporters.f_Remove(&Reporter);
		}

		m_SensorInterfaces.f_Remove(pInterface);

		co_await Results.f_GetResults();

		co_return {};
	}
}
