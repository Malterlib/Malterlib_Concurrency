// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency::NLogStore
{
	struct CFilterLogKeyContext
	{
		struct CLogCacheValue
		{
			NLogStoreLocalDatabase::CLogValue m_Value;
			CDistributedAppLogReporter::CLogInfo const *m_pValue = nullptr;
		};

		CDistributedAppLogReporter::CLogInfo const *f_GetLog(NLogStoreLocalDatabase::CLogKey const &_Key);

		NDatabase::CDatabaseSubReadTransaction &m_Transaction;
		CStr const &m_ThisHostID;
		CStr const &m_Prefix;

		TCMap<NLogStoreLocalDatabase::CLogKey, CLogCacheValue> m_LogValues;
	};

	template <typename tf_CKey>
	bool fg_FilterLogKey
		(
			tf_CKey const &_Key
			, CDistributedAppLogReader_LogFilter const &_Filter
			, CFilterLogKeyContext &_Context
			, CDistributedAppLogReporter::CLogInfo const *_pLogInfo
		)
	;
	template <typename tf_CKey>
	bool fg_FilterLogKey
		(
			tf_CKey const &_Key
			, TCVector<CDistributedAppLogReader_LogFilter> const &_Filters
			, CFilterLogKeyContext &_Context
			, CDistributedAppLogReporter::CLogInfo const *_pLogInfo
		)
	;
	bool fg_MatchMultipleValuesWildcards(NContainer::TCVector<NStr::CStr> const &_Values, NContainer::TCSet<NStr::CStr> const &_Wildcards);
	bool fg_FilterLogValue(CDistributedAppLogReporter::CLogData const &_Value, CDistributedAppLogReader_LogDataFilter const &_Filter);
}

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.hpp"
