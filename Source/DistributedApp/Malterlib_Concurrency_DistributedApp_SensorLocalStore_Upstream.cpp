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
		fg_Timeout(60.0) > [this]() -> TCFuture<void>
			{
				auto Result = co_await f_RetryFailedReporters().f_Wrap();

				if (!Result)
					DMibLogWithCategory(SensorLocalStore, Error, "Failures during retry opening sensor reporters: {}", Result.f_GetExceptionStr());

				co_return {};
			}
		;
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_UpdateSensorForReporter
		(
			CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey
			, TCWeakDistributedActor<CActor> _WeakActor
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

			auto [iSensorReading, ReadTransactionMove] = co_await fg_Move(ReadTransaction).f_BlockingDispatch
				(
					[DatabaseKey, LastSeenUniqueSequence = Reporter.m_LastSeenUniqueSequence](CDatabaseActor::CTransactionRead _ReadTransaction)
					{
						auto iSensorReading = _ReadTransaction.m_Transaction.f_ReadCursor((CSensorKey const &)DatabaseKey);
						iSensorReading.f_FindLowerBound(DatabaseKey);

						if (iSensorReading)
						{
							auto Key = iSensorReading.f_Key<CSensorReadingKey>();
							if (Key.m_UniqueSequence == LastSeenUniqueSequence)
								++iSensorReading;
						}

						return fg_Tuple(fg_Move(iSensorReading), fg_Move(_ReadTransaction));
					}
				)
			;
			ReadTransaction = fg_Move(ReadTransactionMove);

			static constexpr mint c_BatchSize = 32768;

			auto fGetBatch = [iSensorReading = fg_Move(iSensorReading)]() mutable -> TCFuture<TCOptional<TCVector<CDistributedAppSensorReporter::CSensorReading>>>
				{
					if (!iSensorReading)
						co_return {};

					auto BlockingActorCheckout = fg_BlockingActor();
					co_await fg_ContinueRunningOnActor(BlockingActorCheckout);

					TCVector<CDistributedAppSensorReporter::CSensorReading> Batch;

					for (; iSensorReading; ++iSensorReading)
					{
						auto Key = iSensorReading.f_Key<CSensorReadingKey>();
						auto Value = iSensorReading.f_Value<CSensorReadingValue>();

						Batch.f_Insert(fg_Move(Value).f_Reading(Key));

						if (Batch.f_GetLen() >= c_BatchSize)
							break;
					}

					co_return fg_Move(Batch);
				}
			;

			auto pGetBatch = fg_ToSharedPointer(fg_Move(fGetBatch));

			while (auto Batch = co_await fg_CallSafe(pGetBatch))
				co_await Reporter.m_fReportReadings(fg_Move(*Batch));
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

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorInfoChanged(CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey, bool _bWasAdded)
	{
		auto CheckDestroy = co_await m_pThis->f_CheckDestroyedOnResume();
		
		auto Result = co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, pThis = this, ThisActor = fg_ThisActor(m_pThis), DatabaseKey = f_GetDatabaseKey<CSensorKey>(_SensorInfoKey), Prefix = m_Prefix]
				(CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					auto WriteTransaction = fg_Move(_Transaction);

					if (_bCompacting)
						WriteTransaction = co_await ThisActor(&CDistributedAppSensorStoreLocal::fp_Cleanup, fg_Move(WriteTransaction));

					CDistributedAppSensorReporter::CSensorInfo SensorInfo;
					uint64 LastSeenUniqueSequence = 0;
					{
						CSensor *pSensor = pThis->m_Sensors.f_FindEqual(_SensorInfoKey);
						if (!pSensor)
							co_return DMibErrorInstance("Sensor was removed");

						SensorInfo = pSensor->m_Info;
						LastSeenUniqueSequence = pSensor->m_LastSeenUniqueSequence;

						if (DatabaseKey.m_HostID)
						{
							auto &KnownHostValue = pThis->m_KnownHosts[DatabaseKey.m_HostID];
							KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, SensorInfo.m_LastSeen);
							if (!_bCompacting)
								pThis->f_Subscription_SensorInfoChanged(*pSensor, &KnownHostValue);
						}
						else if (!_bCompacting)
							pThis->f_Subscription_SensorInfoChanged(*pSensor, nullptr);

					}

					co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

					if (DatabaseKey.m_HostID)
					{
						CKnownHostKey KnownHostKey{.m_DbPrefix = Prefix, .m_HostID = DatabaseKey.m_HostID};

						CKnownHostValue KnownHostValue;
						WriteTransaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

						KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, SensorInfo.m_LastSeen);

						WriteTransaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);
					}

					CSensorValue DatabaseSensorInfo;
					DatabaseSensorInfo.m_Info = SensorInfo;
					DatabaseSensorInfo.m_UniqueSequenceAtLastCleanup = LastSeenUniqueSequence;

					WriteTransaction.m_Transaction.f_Upsert(DatabaseKey, DatabaseSensorInfo);

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor data to database: {}", Result.f_GetExceptionStr());
			co_return DMibErrorInstance("Failed to store sensor in database, see log for error.");
		}

		auto pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
		if (!pSensor)
			co_return {};

		TCFutureVector<void> Results;

		if (_bWasAdded)
		{
			for (auto &SensorInterface : m_SensorInterfaces)
			{
				auto &WeakActor = m_SensorInterfaces.fs_GetKey(SensorInterface);

				auto &SensorReporter = pSensor->m_SensorReporters[WeakActor];
				SensorReporter.m_pSensor = pSensor;
				SensorReporter.m_pInterface = &SensorInterface;
				SensorInterface.m_Reporters.f_Insert(SensorReporter);
				f_UpdateSensorForReporter(_SensorInfoKey, WeakActor) % f_GetSensorUpdateFailedMessage(SensorReporter) > Results;
			}
		}
		else
		{
			for (auto &SensorReporter : pSensor->m_SensorReporters)
			{
				auto &WeakActor = pSensor->m_SensorReporters.fs_GetKey(SensorReporter);
				f_UpdateSensorForReporter(_SensorInfoKey, WeakActor) % f_GetSensorUpdateFailedMessage(SensorReporter) > Results;
			}
		}

		auto UpstreamResult = co_await fg_AllDone(Results).f_Wrap();

		if (!UpstreamResult)
			DMibLogWithCategory(SensorLocalStore, Error, "Failed to add/change sensor info in upstream: {}", UpstreamResult.f_GetExceptionStr());

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_RetryFailedReporters()
	{
		TCFutureVector<void> Results;

		for (auto iFailed = m_FailedReporters.f_GetIterator(); iFailed;)
		{
			auto &Failed = *iFailed;
			++iFailed;

			auto &SensorKey = Failed.m_pSensor->f_GetKey();

			f_UpdateSensorForReporter(SensorKey, Failed.f_GetID()) % f_GetSensorUpdateFailedMessage(Failed) > Results;
		}

		auto AwaitedResults = co_await fg_AllDoneWrapped(Results);

		m_bScheduledFailedReportersRetry = false;
		if (!m_FailedReporters.f_IsEmpty())
			f_ScheduleFailedRetry();

		co_await (fg_Move(AwaitedResults) | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_NewSensorReadings
		(
			CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey
			, TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const> _pReadings
		)
	{
		auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
		if (!pSensor)
			co_return {};

		TCVector<TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const>> ReadingsChunks;

		static constexpr mint c_ChunkSize = 32768;

		mint nReadings = _pReadings->f_GetLen();

		if (nReadings < c_ChunkSize)
			ReadingsChunks.f_Insert(fg_Move(_pReadings));
		else
		{
			for (mint iStartReading = 0; iStartReading < nReadings;)
			{
				mint ThisTime = fg_Min(nReadings - iStartReading, c_ChunkSize);

				ReadingsChunks.f_Insert
					(
						TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const>
						(
							fg_Construct<TCVector<CDistributedAppSensorReporter::CSensorReading>>(_pReadings->f_GetArray() + iStartReading, ThisTime)
						)
					)
				;

				iStartReading += ThisTime;
			}
		}

		TCFutureVector<void> Results;

		for (auto &pReadings : ReadingsChunks)
		{
			for (auto &Reporter : pSensor->m_SensorReporters)
			{
				auto WeakActor = pSensor->m_SensorReporters.fs_GetKey(Reporter);
				Reporter.m_WriteSequencer.f_RunSequenced
					(
						g_ActorFunctorWeak / [this, pReadings, WeakActor, _SensorInfoKey](CActorSubscription _DoneSubscription) mutable -> TCFuture<void>
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

							co_await pReporter->m_Reporter.m_fReportReadings(*pReadings);

							(void)_DoneSubscription;

							co_return {};
						}
					)
					> Results
				;
			}
		}

		co_await fg_AllDone(Results);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorReporterInterfaceAdded
		(
			TCDistributedActorInterface<CDistributedAppSensorReporter> _Actor
			, CTrustedActorInfo _TrustInfo
		)
	{
		TCWeakDistributedActor<CActor> WeakActor = _Actor.f_Weak();
		auto &SensorInterface = m_SensorInterfaces[WeakActor];
		SensorInterface.m_Actor = fg_Move(_Actor);

		TCFutureVector<void> Results;
		for (auto &Sensor : m_Sensors)
		{
			auto &SensorReporter = Sensor.m_SensorReporters[WeakActor];
			SensorReporter.m_pSensor = &Sensor;
			SensorReporter.m_pInterface = &SensorInterface;
			SensorInterface.m_Reporters.f_Insert(SensorReporter);

			auto &SensorKey = m_Sensors.fs_GetKey(Sensor);

			f_UpdateSensorForReporter(SensorKey, WeakActor) % f_GetSensorUpdateFailedMessage(SensorReporter) > Results;
		}

		co_await fg_AllDone(Results);

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_SensorReporterInterfaceRemoved(TCWeakDistributedActor<CActor> _Actor, CTrustedActorInfo _TrustInfo)
	{
		auto *pInterface = m_SensorInterfaces.f_FindEqual(_Actor);

		if (!pInterface)
			co_return {};

		TCFutureVector<void> Results;
		for (auto iReporter = pInterface->m_Reporters.f_GetIterator(); iReporter;)
		{
			auto &Reporter = *iReporter;
			++iReporter;
			fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Results;
			fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Results;
			Reporter.m_pSensor->m_SensorReporters.f_Remove(&Reporter);
		}

		m_SensorInterfaces.f_Remove(pInterface);

		co_await fg_AllDoneWrapped(Results);

		co_return {};
	}
}
