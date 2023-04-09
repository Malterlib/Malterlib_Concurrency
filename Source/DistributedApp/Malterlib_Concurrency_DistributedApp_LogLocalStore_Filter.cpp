// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp_LogLocalStore_Filter.h"

namespace NMib::NConcurrency::NLogStore
{
	bool fg_MatchMultipleValuesWildcards(NContainer::TCVector<NStr::CStr> const &_Values, NContainer::TCSet<NStr::CStr> const &_Wildcards)
	{
		bool bMatched = false;
		for (auto &Category : _Values)
		{
			if (fg_StrMatchesAnyWildcardInContainer(Category, _Wildcards))
			{
				bMatched = true;
				break;
			}
		}

		return bMatched;
	}

	bool fg_FilterLogValue(CDistributedAppLogReporter::CLogData const &_Value, CDistributedAppLogReader_LogDataFilter const &_Filter)
	{
		if (_Filter.m_Flags)
		{
			if (!(_Value.m_Flags & *_Filter.m_Flags))
				return false;
		}

		if (_Filter.m_Severities)
		{
			if (!_Filter.m_Severities->f_FindEqual(_Value.m_Severity))
				return false;
		}

		if (_Filter.m_CategoriesWildcards)
		{
			if (!fg_MatchMultipleValuesWildcards(_Value.m_Categories, *_Filter.m_CategoriesWildcards))
				return false;
		}

		if (_Filter.m_OperationsWildcards)
		{
			if (!fg_MatchMultipleValuesWildcards(_Value.m_Operations, *_Filter.m_OperationsWildcards))
				return false;
		}

		if (_Filter.m_MessageWildcards)
		{
			if (!fg_StrMatchesAnyWildcardInContainer(_Value.m_Message, *_Filter.m_MessageWildcards))
				return false;
		}

		if (_Filter.m_SourceLocationFileWildcards)
		{
			if (!_Value.m_SourceLocation)
				return false;

			if (!fg_StrMatchesAnyWildcardInContainer(_Value.m_SourceLocation->m_File, *_Filter.m_SourceLocationFileWildcards))
				return false;
		}

		return true;
	}
}
