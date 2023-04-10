// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.h"

namespace NMib::NConcurrency
{
	using namespace NLogStoreLocalDatabase;

	auto CDistributedAppLogStoreLocal::f_SubscribeLogs
		(
			TCVector<CDistributedAppLogReader_LogFilter> &&_Filters
			, TCActorFunctor<TCFuture<void> (CDistributedAppLogReader::CLogChange &&_Change)> &&_fOnChange
		)
		-> TCFuture<CActorSubscription>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		if (_fOnChange.f_IsEmpty())
			co_return DMibErrorInstance("Local store not yet started");

		CStr SubscriptionID = NCryptography::fg_RandomID(Internal.m_LogSubscriptions);
		Internal.m_LogSubscriptions[SubscriptionID].m_Filters = fg_Move(_Filters);

		CInternal::CLogSubscription *pSubscription = nullptr;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSubscription = Internal.m_LogSubscriptions.f_FindEqual(SubscriptionID);
					if (!pSubscription)
						return DMibErrorInstance("Subscription deleted");
					return {};
				}
			)
		;

		// Send initial
		auto Logs = co_await self(&CDistributedAppLogStoreLocal::f_GetLogs, CDistributedAppLogReader::CGetLogs{.m_Filters = {pSubscription->m_Filters}});
		for (auto iLog = co_await fg_Move(Logs).f_GetIterator(); iLog; co_await ++iLog)
		{
			TCActorResultVector<void> Results;
			for (auto &Log : *iLog)
				_fOnChange(fg_Move(Log)) > Results.f_AddResult();

			co_await (co_await Results.f_GetResults() | g_Unwrap);
		}

		// Send entries that came in while sending initial
		TCActorResultVector<void> Results;
		for (auto &Change : pSubscription->m_QueuedChanges)
			_fOnChange(fg_Move(Change)) > Results.f_AddResult();
		pSubscription->m_QueuedChanges.f_Clear();

		pSubscription->m_fOnChange = fg_Move(_fOnChange);

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				auto pSubscription = Internal.m_LogSubscriptions.f_FindEqual(SubscriptionID);
				TCFuture<void> DestroyFuture;
				if (pSubscription)
				{
					DestroyFuture = fg_Move(pSubscription->m_fOnChange).f_Destroy();
					Internal.m_LogSubscriptions.f_Remove(pSubscription);
				}

				if (DestroyFuture.f_IsValid())
					co_await fg_Move(DestroyFuture);

				co_return {};
			}
		;
	}

	auto CDistributedAppLogStoreLocal::f_SubscribeLogEntries
		(
			TCVector<CDistributedAppLogReader_LogEntrySubscriptionFilter> &&_Filters
			, TCActorFunctor<TCFuture<void> (CDistributedAppLogReader_LogKeyAndEntry &&_Entry)> &&_fOnEntry
		)
		-> TCFuture<CActorSubscription>
	{
		auto &Internal = *mp_pInternal;

		if (!Internal.m_bStarted)
			co_return DMibErrorInstance("Local store not yet started");

		if (_fOnEntry.f_IsEmpty())
			co_return DMibErrorInstance("Local store not yet started");

		CStr SubscriptionID = NCryptography::fg_RandomID(Internal.m_EntrySubscriptions);
		Internal.m_EntrySubscriptions[SubscriptionID];

		CInternal::CEntrySubscription *pSubscription = nullptr;

		auto OnResume = co_await fg_OnResume
			(
				[&]() -> CExceptionPointer
				{
					pSubscription = Internal.m_EntrySubscriptions.f_FindEqual(SubscriptionID);
					if (!pSubscription)
						return DMibErrorInstance("Subscription deleted");
					return {};
				}
			)
		;

		for (auto &Filter : _Filters)
		{
			auto &OutFilter = pSubscription->m_Filters.f_Insert();
			OutFilter.m_Filter = fg_Move(Filter);
			OutFilter.m_bIsDataFiltered = OutFilter.m_Filter.m_LogDataFilter.f_IsFiltered();
		}

		bool bNeedInitial = false;
		CDistributedAppLogReader::CGetLogEntries GetLogEntriesParams;
		for (auto &Filter : pSubscription->m_Filters)
		{
			if (Filter.m_Filter.m_MinSequence || Filter.m_Filter.m_MinTimestamp)
				bNeedInitial = true;

			GetLogEntriesParams.m_Filters.f_Insert(Filter.m_Filter.f_ToEntryFilter());
		}

		if (bNeedInitial)
		{
			auto LogEntries = co_await self(&CDistributedAppLogStoreLocal::f_GetLogEntries, fg_Move(GetLogEntriesParams));
			for (auto iEntries = co_await fg_Move(LogEntries).f_GetIterator(); iEntries; co_await ++iEntries)
			{
				TCActorResultVector<void> Results;
				for (auto &Entry : *iEntries)
				{
					auto &LastSeenEntry = pSubscription->m_LastSeenEntry[Entry.m_LogInfoKey];
					LastSeenEntry = fg_Max(LastSeenEntry, Entry.m_Entry.m_UniqueSequence);

					_fOnEntry(fg_Move(Entry)) > Results.f_AddResult();
				}

				co_await (co_await Results.f_GetResults() | g_Unwrap);
			}
		}

		// Send entries that came in while sending initial
		TCActorResultVector<void> Results;
		for (auto &Entry : pSubscription->m_QueuedEntries)
		{
			auto *pLastSeen = pSubscription->m_LastSeenEntry.f_FindEqual(Entry.m_LogInfoKey);
			if (pLastSeen && Entry.m_Entry.m_UniqueSequence <= *pLastSeen)
				continue;
			_fOnEntry(fg_Move(Entry)) > Results.f_AddResult();
		}
		pSubscription->m_QueuedEntries.f_Clear();

		pSubscription->m_fOnEntry = fg_Move(_fOnEntry);

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				auto pSubscription = Internal.m_EntrySubscriptions.f_FindEqual(SubscriptionID);
				TCFuture<void> DestroyFuture;
				if (pSubscription)
				{
					DestroyFuture = fg_Move(pSubscription->m_fOnEntry).f_Destroy();
					Internal.m_EntrySubscriptions.f_Remove(pSubscription);
				}

				if (DestroyFuture.f_IsValid())
					co_await fg_Move(DestroyFuture);

				co_return {};
			}
		;
	}

	void CDistributedAppLogStoreLocal::CInternal::f_Subscription_Entries
		(
			CLog const &_Log
			, TCVector<CDistributedAppLogReporter::CLogEntry> const &_Entries
			, NDatabase::CDatabaseSubReadTransaction &_Transaction
		)
	{
		auto LogDatabaseKey = f_GetDatabaseKey<CLogKey>(_Log.m_Info);
		auto LogKey = _Log.f_GetKey();

		NLogStore::CFilterLogKeyContext FilterContext{.m_pTransaction = &_Transaction, .m_ThisHostID = m_ThisHostID, .m_Prefix = m_Prefix};

		for (auto &Entry : _Entries)
		{
			for (auto &Subscription : m_EntrySubscriptions)
			{
				{
					bool bPassFilter = Subscription.m_Filters.f_IsEmpty();

					for (auto &Filter : Subscription.m_Filters)
					{
						if (!NLogStore::fg_FilterLogKey(LogDatabaseKey, Filter.m_Filter.m_LogFilter, FilterContext, &_Log.m_Info))
							continue;

						if (Filter.m_bIsDataFiltered && !NLogStore::fg_FilterLogValue(Entry.m_Data, Filter.m_Filter.m_LogDataFilter))
							continue;

						bPassFilter = true;
						break;
					}

					if (!bPassFilter)
						continue;
				}

				CDistributedAppLogReader_LogKeyAndEntry OutEntry;
				OutEntry.m_Entry = Entry;
				OutEntry.m_LogInfoKey = LogKey;

				if (Subscription.m_fOnEntry.f_IsEmpty())
					Subscription.m_QueuedEntries.f_Insert(fg_Move(OutEntry));
				else
					Subscription.m_fOnEntry(fg_Move(OutEntry)) > fg_LogError("LogLocalStore", "Failed to send log entry to subscription");
			}
		}
	}

	void CDistributedAppLogStoreLocal::CInternal::f_Subscription_LogInfoChanged(CLog const &_Log, NDatabase::CDatabaseSubReadTransaction &_Transaction)
	{
		auto DatabaseKey = f_GetDatabaseKey<CLogKey>(_Log.m_Info);

		NLogStore::CFilterLogKeyContext FilterContext{.m_pTransaction = &_Transaction, .m_ThisHostID = m_ThisHostID, .m_Prefix = m_Prefix};

		for (auto &Subscription : m_LogSubscriptions)
		{
			if (!NLogStore::fg_FilterLogKey(DatabaseKey, Subscription.m_Filters, FilterContext, &_Log.m_Info))
				continue;

			if (Subscription.m_fOnChange)
				Subscription.m_fOnChange(_Log.m_Info) > fg_LogError("LogLocalStore", "Failed to send log change to subscription");
			else
				Subscription.m_QueuedChanges[_Log.f_GetKey()] = _Log.m_Info;
		}
	}
}
