// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency
{
	using namespace NSensorStoreLocalDatabase;

	bool CDistributedAppSensorStoreLocal::CCleanupHelper::f_Init(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, CStr const &_Prefix, NTime::CTime &o_Time)
	{
		mp_iReadingByTime = _WriteTransaction.m_Transaction.f_WriteCursor(_Prefix, CSensorReadingByTime::mc_Prefix);

		if (mp_iReadingByTime)
		{
			o_Time = mp_iReadingByTime.f_Key<CSensorReadingByTime>().m_Timestamp;
			return true;
		}

		return false;
	}

	bool CDistributedAppSensorStoreLocal::CCleanupHelper::f_DeleteOne(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NTime::CTime &o_Time)
	{
		if (!mp_iReadingByTime)
			return {};

		auto KeyByTime = mp_iReadingByTime.f_Key<CSensorReadingByTime>();

		auto Key = KeyByTime.f_Key();

		if (_WriteTransaction.m_Transaction.f_Exists(Key))
			_WriteTransaction.m_Transaction.f_Delete(Key);

		mp_iReadingByTime.f_Delete();

		if (mp_iReadingByTime)
		{
			o_Time = mp_iReadingByTime.f_Key<CSensorReadingByTime>().m_Timestamp;
			return true;
		}

		return false;
	}

	TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> CDistributedAppSensorStoreLocal::f_PrepareForCleanup(NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)
	{
		auto &Internal = *mp_pInternal;
		for (auto &Sensor : Internal.m_Sensors)
		{
			auto DatabaseKey = Internal.f_GetDatabaseKey<CSensorKey>(Sensor.m_Info);

			CSensorValue DatabaseSensorInfo;
			DatabaseSensorInfo.m_Info = Sensor.m_Info;
			DatabaseSensorInfo.m_UniqueSequenceAtLastCleanup = Sensor.m_LastSeenUniqueSequence;

			_WriteTransaction.m_Transaction.f_Upsert(DatabaseKey, DatabaseSensorInfo);
		}

		co_return fg_Move(_WriteTransaction);
	}

	TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> CDistributedAppSensorStoreLocal::fp_Cleanup(NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)
	{
		auto &Internal = *mp_pInternal;
		co_return co_await CInternal::fs_Cleanup(&Internal, fg_Move(_WriteTransaction));
	}

	auto CDistributedAppSensorStoreLocal::CInternal::fs_Cleanup(CInternal *_pThis, NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)
		-> TCFuture<NDatabase::CDatabaseActor::CTransactionWrite>
	{
		if (_pThis->m_fCleanup)
			co_return co_await _pThis->m_fCleanup(fg_Move(_WriteTransaction));

		auto WriteTransaction = co_await _pThis->m_pThis->f_PrepareForCleanup(fg_Move(_WriteTransaction));
		auto Prefix = _pThis->m_Prefix;
		auto RetentionDays = _pThis->m_RetentionDays;

		co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

		auto CaptureScope = co_await g_CaptureExceptions;

		auto OriginalStats = WriteTransaction.m_Transaction.f_SizeStatistics();

		auto TargetSize = OriginalStats.m_MapSize - fg_Min(fg_Max(OriginalStats.m_MapSize / 5, 32 * OriginalStats.m_PageSize), OriginalStats.m_MapSize / 2);

		CCleanupHelper Helper;
		NTime::CTime StartTime;
		if (!Helper.f_Init(WriteTransaction, Prefix, StartTime))
			co_return fg_Move(WriteTransaction);

		bool bDoRetention = RetentionDays > 0;
		NTime::CTime OldestAllowedReading = NTime::CTime::fs_NowUTC() - NTime::CTimeSpanConvert::fs_CreateDaySpan(RetentionDays);

		if (OriginalStats.m_UsedBytes < TargetSize)
		{
			if (!bDoRetention)
				co_return fg_Move(WriteTransaction);

			if (StartTime.f_IsValid() && StartTime > OldestAllowedReading)
				co_return fg_Move(WriteTransaction);
		}

		auto CurrentStats = OriginalStats;

		[[maybe_unused]] umint nReadingsDeleted = 1;
		NTime::CTime EndTime;

		while (Helper.f_DeleteOne(WriteTransaction, EndTime))
		{
			CurrentStats = WriteTransaction.m_Transaction.f_SizeStatistics();
			if (CurrentStats.m_UsedBytes < TargetSize)
			{
				if (!bDoRetention)
					break;

				if (EndTime.f_IsValid() && EndTime > OldestAllowedReading)
					break;
			}

			++nReadingsDeleted;
		}

		NTime::CTimeSpan UtcOffset;
		NTime::CSystem_Time::fs_TimeGetUTCOffset(&UtcOffset);

		[[maybe_unused]] auto UtcHourOffset = NTime::CTimeSpanConvert(UtcOffset).f_GetHours();
		[[maybe_unused]] auto UtcMinuteOffset = NTime::CTimeSpanConvert(UtcOffset).f_GetMinuteOfHour();
		[[maybe_unused]] ch8 const *pUtcSign = UtcHourOffset < 0 ? "-" : "+";
		UtcHourOffset = fg_Abs(UtcHourOffset);

		DMibLogWithCategory
			(
				SensorLocalStore
				, Info
				, "Freed up {ns } bytes by deleting {} sensor readings spanning from {tc5} UTC{}{sj2,sf0}:{sj2,sf0} to {tc5} UTC{}{sj2,sf0}:{sj2,sf0}"
				, OriginalStats.m_UsedBytes - CurrentStats.m_UsedBytes
				, nReadingsDeleted
				, StartTime.f_ToLocal()
				, pUtcSign
				, UtcHourOffset
				, UtcMinuteOffset
				, EndTime.f_ToLocal()
				, pUtcSign
				, UtcHourOffset
				, UtcMinuteOffset
			)
		;

		co_return fg_Move(WriteTransaction);
	}
}
