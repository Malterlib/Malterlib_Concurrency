// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	bool CDistributedAppLogStoreLocal::CCleanupHelper::f_Init(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, CStr const &_Prefix, NTime::CTime &o_Time)
	{
		mp_iEntryByTime = _WriteTransaction.m_Transaction.f_WriteCursor(_Prefix, CLogEntryByTime::mc_Prefix);

		if (mp_iEntryByTime)
		{
			o_Time = mp_iEntryByTime.f_Key<CLogEntryByTime>().m_Timestamp;
			return true;
		}

		return false;
	}

	bool CDistributedAppLogStoreLocal::CCleanupHelper::f_DeleteOne(NDatabase::CDatabaseActor::CTransactionWrite &_WriteTransaction, NTime::CTime &o_Time)
	{
		if (!mp_iEntryByTime)
			return {};

		auto KeyByTime = mp_iEntryByTime.f_Key<CLogEntryByTime>();

		auto Key = KeyByTime.f_Key();

		if (_WriteTransaction.m_Transaction.f_Exists(Key))
			_WriteTransaction.m_Transaction.f_Delete(Key);

		mp_iEntryByTime.f_Delete();

		if (mp_iEntryByTime)
		{
			o_Time = mp_iEntryByTime.f_Key<CLogEntryByTime>().m_Timestamp;
			return true;
		}

		return false;
	}

	TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> CDistributedAppLogStoreLocal::f_PrepareForCleanup(NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)
	{
		struct CLogPair
		{
			CLogKey m_Key;
			CLogValue m_Value;
		};

		TCVector<CLogPair> ToUpsert;

		auto &Internal = *mp_pInternal;
		for (auto &Log : Internal.m_Logs)
		{
			CLogValue DatabaseLogInfo;
			DatabaseLogInfo.m_Info = Log.m_Info;
			DatabaseLogInfo.m_UniqueSequenceAtLastCleanup = Log.m_LastSeenUniqueSequence;

			ToUpsert.f_Insert(CLogPair{.m_Key = Internal.f_GetDatabaseKey<CLogKey>(Log.m_Info), .m_Value = fg_Move(DatabaseLogInfo)});
		}

		co_await fg_ContinueRunningOnActor(_WriteTransaction.f_Checkout());

		for (auto &Entry : ToUpsert)
			_WriteTransaction.m_Transaction.f_Upsert(Entry.m_Key, Entry.m_Value);

		co_return fg_Move(_WriteTransaction);
	}

	auto CDistributedAppLogStoreLocal::CInternal::fs_Cleanup(CInternal *_pThis, NDatabase::CDatabaseActor::CTransactionWrite _WriteTransaction)
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
		NTime::CTime OldestAllowedEntry = NTime::CTime::fs_NowUTC() - NTime::CTimeSpanConvert::fs_CreateDaySpan(RetentionDays);

		if (OriginalStats.m_UsedBytes < TargetSize)
		{
			if (!bDoRetention)
				co_return fg_Move(WriteTransaction);

			if (StartTime.f_IsValid() && StartTime > OldestAllowedEntry)
				co_return fg_Move(WriteTransaction);
		}

		auto CurrentStats = OriginalStats;

		[[maybe_unused]] umint nEntriesDeleted = 1;
		NTime::CTime EndTime;

		while (Helper.f_DeleteOne(WriteTransaction, EndTime))
		{
			CurrentStats = WriteTransaction.m_Transaction.f_SizeStatistics();
			if (CurrentStats.m_UsedBytes < TargetSize)
			{
				if (!bDoRetention)
					break;

				if (EndTime.f_IsValid() && EndTime > OldestAllowedEntry)
					break;
			}

			++nEntriesDeleted;
		}

		NTime::CTimeSpan UtcOffset;
		NTime::CSystem_Time::fs_TimeGetUTCOffset(&UtcOffset);

		[[maybe_unused]] auto UtcHourOffset = NTime::CTimeSpanConvert(UtcOffset).f_GetHours();
		[[maybe_unused]] auto UtcMinuteOffset = NTime::CTimeSpanConvert(UtcOffset).f_GetMinuteOfHour();
		[[maybe_unused]] ch8 const *pUtcSign = UtcHourOffset < 0 ? "-" : "+";
		UtcHourOffset = fg_Abs(UtcHourOffset);

		DMibLogWithCategory
			(
				LogLocalStore
				, Info
				, "Freed up {ns } bytes by deleting {} log entries spanning from {tc5} UTC{}{sj2,sf0}:{sj2,sf0} to {tc5} UTC{}{sj2,sf0}:{sj2,sf0}"
				, OriginalStats.m_UsedBytes - CurrentStats.m_UsedBytes
				, nEntriesDeleted
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
