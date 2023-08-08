// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	TCFuture<void> CDistributedAppSensorStoreLocal::f_SeenHosts(NContainer::TCMap<NStr::CStr, CSeenHost> &&_HostsSeen)
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Result = co_await Internal.m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, HostsSeen = fg_Move(_HostsSeen)]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto &Internal = *mp_pInternal;

					auto WriteTransaction = fg_Move(_Transaction);

					if (_bCompacting)
						WriteTransaction = co_await fg_CallSafe(Internal, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

					for (auto &SeenHost : HostsSeen)
					{
						auto &HostID = HostsSeen.fs_GetKey(SeenHost);

						CKnownHostKey Key{.m_DbPrefix = Internal.m_Prefix, .m_HostID = HostID};

						CKnownHostValue Value;
						_Transaction.m_Transaction.f_Get(Key, Value);

						if (SeenHost.m_TimeSeen)
							Value.m_LastSeen = fg_Max(Value.m_LastSeen, *SeenHost.m_TimeSeen);

						if (SeenHost.m_PauseReportingFor)
						{
							if (Value.m_PauseReportingFor != *SeenHost.m_PauseReportingFor)
							{
								Value.m_PauseReportingFor = *SeenHost.m_PauseReportingFor;

								CDistributedAppSensorReporter::CSensorInfoKey SensorKey;
								SensorKey.m_HostID = HostID;

								for (auto &Sensor : Internal.m_Sensors.f_GetIterator_SmallestGreaterThanEqual(SensorKey))
								{
									if (Sensor.m_Info.m_HostID != HostID)
										break;

									Internal.f_Subscription_SensorInfoChanged(Sensor, _Transaction.m_Transaction, &Value);
								}
							}
						}
						Value.m_bRemoved = false;

						WriteTransaction.m_Transaction.f_Upsert(Key, Value);
					}

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error saving knowns hosts to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::f_RemoveHosts(NContainer::TCSet<NStr::CStr> &&_RemovedHostIDs)
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto Result = co_await Internal.m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, RemovedHostIDs = fg_Move(_RemovedHostIDs)]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto &Internal = *mp_pInternal;

					auto WriteTransaction = fg_Move(_Transaction);

					if (_bCompacting)
						WriteTransaction = co_await fg_CallSafe(Internal, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

					for (auto &HostID : RemovedHostIDs)
					{
						CKnownHostKey Key{.m_DbPrefix = Internal.m_Prefix, .m_HostID = HostID};

						CKnownHostValue Value;
						if (!_Transaction.m_Transaction.f_Get(Key, Value))
							continue;

						Value.m_bRemoved = true;
						Value.m_PauseReportingFor = fp32::fs_QNan();
						WriteTransaction.m_Transaction.f_Upsert(Key, Value);
					}

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error saving knowns hosts remove to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<uint32> CDistributedAppSensorStoreLocal::f_RemoveSensors(NContainer::TCSet<CDistributedAppSensorReporter::CSensorInfoKey> &&_SensorInfoKeys)
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		uint32 nRemoved = 0;

		for (auto &SensorInfoKey : _SensorInfoKeys)
		{
			auto *pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
			if (!pSensor)
				continue;

			if (pSensor->m_Info.m_bRemoved)
				continue;

			++nRemoved;
			pSensor->m_Info.m_bRemoved = true;
			co_await fg_CallSafe(Internal, &CInternal::f_SensorInfoChanged, SensorInfoKey, false);
		}

		co_return nRemoved;
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_StoreSensorReadings
		(
			CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey
			, CSensorReadingKey const &_DatabaseKey
			, TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading> const> &&_pReadings
		)
	{
		auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
		if (!pSensor)
			co_return {};

		auto pCanDestroyTracker = m_pCanDestroyStoringLocal;

		auto Result = co_await m_Database
			(
				&CDatabaseActor::f_WriteWithCompaction
				, g_ActorFunctorWeak / [=, HostID = _SensorInfoKey.m_HostID]
				(CDatabaseActor::CTransactionWrite &&_Transaction, bool _bCompacting) -> TCFuture<CDatabaseActor::CTransactionWrite>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto WriteTransaction = fg_Move(_Transaction);
					auto DatabaseKey = _DatabaseKey;

					if (!_bCompacting)
					{
						if (auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey))
							f_Subscription_Readings(*pSensor, *_pReadings, WriteTransaction.m_Transaction);
					}

					auto SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
					smint BatchLimit = fg_Min(SizeStats.m_MapSize / (20 * 3), 4 * 1024 * 1024);
					auto SizeLimit = smint(SizeStats.m_MapSize) - fg_Min(fg_Max(smint(SizeStats.m_MapSize) / 20, BatchLimit * 3, 32 * smint(SizeStats.m_PageSize)), smint(SizeStats.m_MapSize) / 3);
					smint nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);

					CKnownHostKey KnownHostKey{.m_DbPrefix = m_Prefix, .m_HostID = HostID};

					CKnownHostValue KnownHostValue;
					if (HostID)
						_Transaction.m_Transaction.f_Get(KnownHostKey, KnownHostValue);

					for (auto &Reading : *_pReadings)
					{
						smint nBytesInserted = WriteTransaction.m_Transaction.f_SizeStatistics().m_UsedBytes - SizeStats.m_UsedBytes;
						bool bFreeUpSpace = nBytesInserted >= nBytesLimit || _bCompacting;

						if (bFreeUpSpace)
							WriteTransaction = co_await fg_CallSafe(this, &CInternal::f_Cleanup, fg_Move(WriteTransaction));

						if (nBytesInserted > BatchLimit || bFreeUpSpace)
						{
							if (_bCompacting)
								co_await m_Database(&CDatabaseActor::f_Compact, fg_Move(WriteTransaction), 0);
							else
							{
								co_await m_Database(&CDatabaseActor::f_CommitWriteTransaction, fg_Move(WriteTransaction));
								if (bFreeUpSpace)
									co_await m_Database(&CDatabaseActor::f_WaitForAllReaders);
							}
							WriteTransaction = co_await m_Database(&CDatabaseActor::f_OpenTransactionWrite);
							SizeStats = WriteTransaction.m_Transaction.f_SizeStatistics();
							nBytesLimit = smint(SizeLimit) - smint(SizeStats.m_UsedBytes);
						}

						DatabaseKey.m_UniqueSequence = Reading.m_UniqueSequence;

						if (WriteTransaction.m_Transaction.f_Exists(DatabaseKey))
							continue; // Already reported

						CSensorReadingValue Value;
						Value.m_Timestamp = Reading.m_Timestamp;
						Value.m_Data = Reading.m_Data;

						CSensorReadingByTime ByTime;
						ByTime.m_DbPrefix = DatabaseKey.m_DbPrefix;
						ByTime.m_Prefix = CSensorReadingByTime::mc_Prefix;
						ByTime.m_Timestamp = Reading.m_Timestamp;
						ByTime.m_UniqueSequence = DatabaseKey.m_UniqueSequence;

						ByTime.m_HostID = DatabaseKey.m_HostID;
						ByTime.m_ApplicationName = DatabaseKey.m_ApplicationName;
						ByTime.m_Identifier = DatabaseKey.m_Identifier;
						ByTime.m_IdentifierScope = DatabaseKey.m_IdentifierScope;

						KnownHostValue.m_LastSeen = fg_Max(KnownHostValue.m_LastSeen, Reading.m_Timestamp);

						WriteTransaction.m_Transaction.f_Insert(DatabaseKey, Value);
						WriteTransaction.m_Transaction.f_Insert(ByTime, CSensorReadingByTimeValue());
					}

					if (HostID)
						_Transaction.m_Transaction.f_Upsert(KnownHostKey, KnownHostValue);

					co_return fg_Move(WriteTransaction);
				}
			)
			.f_Wrap()
		;

		if (!Result)
		{
			DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor data to database: {}", Result.f_GetExceptionStr());
			co_return Result.f_GetException();
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppSensorStoreLocal::CInternal::f_CleanupSensorReporter(CDistributedAppSensorReporter::CSensorInfoKey const &_SensorInfoKey)
	{
		CInternal::CSensor *pSensor = nullptr;
		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
					if (!pSensor)
						return DMibErrorInstance("Sensor removed");
					return {};
				}
			)
		;

		if (--pSensor->m_ActiveRefCount == 0)
		{
			auto CanDestroyFuture = pSensor->m_pCanDestroy->f_Future();
			pSensor->m_pCanDestroy.f_Clear();
			co_await fg_Move(CanDestroyFuture);

			if (pSensor->m_ActiveRefCount == 0) // Might have been recreated
			{
				TCActorResultVector<void> Destroys;

				for (auto &Reporter : pSensor->m_SensorReporters)
				{
					fg_Move(Reporter.m_WriteSequencer).f_Destroy() > Destroys.f_AddResult();
					if (Reporter.m_Reporter.m_fReportReadings)
						fg_Move(Reporter.m_Reporter.m_fReportReadings).f_Destroy() > Destroys.f_AddResult();
				}

				pSensor->m_SensorReporters.f_Clear();
				co_await Destroys.f_GetResults();
			}
		}

		co_return {};
	}

	auto CDistributedAppSensorStoreLocal::CInternal::f_GetReportReadingsFunctor
		(
			CDistributedAppSensorReporter::CSensorInfoKey _SensorInfoKey
			, CSensorReadingKey _DatabaseKey
		)
		-> TCActorFunctorWithID<TCFuture<void> (NContainer::TCVector<CDistributedAppSensorReporter::CSensorReading> &&_Readings)>
	{
		return g_ActorFunctor
			(
				g_ActorSubscription / [this, _SensorInfoKey]() -> TCFuture<void>
				{
					co_return co_await fg_CallSafe(this, &CInternal::f_CleanupSensorReporter, _SensorInfoKey);
				}
			)
			/ [this, _SensorInfoKey, _DatabaseKey, AllowDestroy = g_AllowWrongThreadDestroy]
			(TCVector<CDistributedAppSensorReporter::CSensorReading> &&_Readings) mutable -> TCFuture<void>
			{
				auto *pSensor = m_Sensors.f_FindEqual(_SensorInfoKey);
				if (!pSensor)
					co_return {};

				TCOptional<CCanDestroyTracker::CCanDestroyResultFunctor> CanDestroy;
				if (pSensor->m_pCanDestroy)
					CanDestroy = pSensor->m_pCanDestroy->f_Track();

				auto SequenceSubscription = co_await pSensor->m_SensorSequencer.f_Sequence();

				TCVector<CDistributedAppSensorReporter::CSensorReading> NewReadings;

				for (auto &Reading : _Readings)
				{
					if (Reading.m_UniqueSequence == TCLimitsInt<uint64>::mc_Max)
						Reading.m_UniqueSequence = ++pSensor->m_LastSeenUniqueSequence;
					else
					{
						if (Reading.m_UniqueSequence <= pSensor->m_LastSeenUniqueSequence)
							continue; // Can happen if readings are being reported with two interfaces
						
						pSensor->m_LastSeenUniqueSequence = fg_Max(pSensor->m_LastSeenUniqueSequence, Reading.m_UniqueSequence);
					}

					NewReadings.f_Insert(fg_Move(Reading));
				}

				if (NewReadings.f_IsEmpty())
					co_return {};

				TCSharedPointer<TCVector<CDistributedAppSensorReporter::CSensorReading>> pReadings = fg_Construct(fg_Move(NewReadings));

				auto [DatabaseResult, UpstreamResult] = co_await
					(
						fg_CallSafe(this, &CInternal::f_StoreSensorReadings, _SensorInfoKey, _DatabaseKey, pReadings)
						+ fg_CallSafe(this, &CInternal::f_NewSensorReadings, _SensorInfoKey, pReadings)
					)
					.f_Wrap()
				;

				if (!UpstreamResult)
				{
					auto ExceptionStr = UpstreamResult.f_GetExceptionStr();
					DMibLogWithCategory(SensorLocalStore, Error, "Error sending sensor readings to upstream: {}", ExceptionStr);
				}

				if (!DatabaseResult)
				{
					auto ExceptionStr = DatabaseResult.f_GetExceptionStr();
					DMibLogWithCategory(SensorLocalStore, Critical, "Error saving sensor readings to database: {}", ExceptionStr);
					co_return DMibErrorInstance("Failed to store sensor readings in database, see log for error.");
				}

				co_return {};
			}
		;
	}

	auto CDistributedAppSensorStoreLocal::f_OpenSensorReporter(CDistributedAppSensorReporter::CSensorInfo &&_SensorInfo)
		-> TCFuture<CDistributedAppSensorReporter::CSensorReporter>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		auto SensorInfoKey = _SensorInfo.f_Key();

		auto SensorMap = Internal.m_Sensors(SensorInfoKey, fg_TempCopy(_SensorInfo));

		auto *pSensor = &*SensorMap;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					auto &Internal = *mp_pInternal;

					if (f_IsDestroyed())
						return DMibErrorInstance("Shutting down");

					pSensor = Internal.m_Sensors.f_FindEqual(SensorInfoKey);
					if (!pSensor)
						return DMibErrorInstance("Aborted");

					return {};
				}
			)
		;

		bool bWasCreated = SensorMap.f_WasCreated();
		bool bWasAdded = ++pSensor->m_ActiveRefCount == 1;

		if (!pSensor->m_pCanDestroy)
			pSensor->m_pCanDestroy = fg_Construct();

		{
			auto SequenceSubscription = co_await pSensor->m_SensorSequencer.f_Sequence();
			auto &Internal = *mp_pInternal;

			// Reset removed state when a sensor has been opened again after being marked as removed. Last seen will only be updated when a sensor is opened in an non-upstream context.
			if (!_SensorInfo.m_LastSeen.f_IsValid() || _SensorInfo.m_LastSeen == pSensor->m_Info.m_LastSeen)
			{
				_SensorInfo.m_bRemoved = pSensor->m_Info.m_bRemoved;
				_SensorInfo.m_PauseReportingFor = pSensor->m_Info.m_PauseReportingFor;
			}

			bool bWasChanged = bWasCreated || _SensorInfo != pSensor->m_Info;

			if (bWasChanged)
				pSensor->m_Info = _SensorInfo;

			if (bWasChanged || bWasAdded)
				co_await fg_CallSafe(Internal, &CInternal::f_SensorInfoChanged, SensorInfoKey, bWasAdded || bWasCreated);
		}

		CDistributedAppSensorReporter::CSensorReporter Reporter;
		Reporter.m_LastSeenUniqueSequence = pSensor->m_LastSeenUniqueSequence;
		Reporter.m_fReportReadings = Internal.f_GetReportReadingsFunctor(SensorInfoKey, Internal.f_GetDatabaseKey<CSensorReadingKey>(_SensorInfo));

		co_return fg_Move(Reporter);
	}
}
