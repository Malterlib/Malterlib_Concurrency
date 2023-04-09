// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Internal.h"
#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Database.h"

namespace NMib::NConcurrency::NLogStore
{
	template <typename tf_CKey>
	bool fg_FilterLogKey(tf_CKey const &_Key, CDistributedAppLogReader_LogFilter const &_Filter);
	bool fg_MatchMultipleValuesWildcards(NContainer::TCVector<NStr::CStr> const &_Values, NContainer::TCSet<NStr::CStr> const &_Wildcards);
	bool fg_FilterLogValue(CDistributedAppLogReporter::CLogData const &_Value, CDistributedAppLogReader_LogDataFilter const &_Filter);
}

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.hpp"
