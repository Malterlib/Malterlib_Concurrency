// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedAppLogReader_LogFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_Scope;
		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
		if (_Stream.f_GetVersion() >= CDistributedAppLogReporter::EProtocolVersion_IgnoreRemoved)
			_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader_LogDataFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_CategoriesWildcards;
		_Stream % m_OperationsWildcards;
		_Stream % m_MessageWildcards;
		_Stream % m_SourceLocationFileWildcards;
		_Stream % m_Severities;
		_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader_LogEntryFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_LogFilter;
		_Stream % m_LogDataFilter;

		_Stream % m_MinSequence;
		_Stream % m_MaxSequence;

		_Stream % m_MinTimestamp;
		_Stream % m_MaxTimestamp;

		_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader_LogEntrySubscriptionFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_LogFilter;
		_Stream % m_LogDataFilter;

		_Stream % m_MinSequence;
		_Stream % m_MinTimestamp;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader_LogKeyAndEntry::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_LogInfoKey;
		_Stream % m_Entry;
	}

	template <typename tf_CStream, typename tf_CFilter>
	void CDistributedAppLogReader::fs_StreamFilterVector(tf_CStream &_Stream, NContainer::TCVector<tf_CFilter> &o_Filters)
	{
		if (_Stream.f_GetVersion() >= CDistributedAppLogReporter::EProtocolVersion_MultipleFilters)
			_Stream % o_Filters;
		else
		{
			if constexpr (tf_CStream::mc_bConsume)
			{
				tf_CFilter Filter;
				_Stream >> Filter;
				o_Filters.f_Clear();
				o_Filters.f_Insert(fg_Move(Filter));
			}
			else
			{
				if (o_Filters.f_GetLen() >= 0)
					_Stream << o_Filters[0];
				else
				{
					tf_CFilter Default;
					_Stream << Default;
				}
			}
		}
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader::CGetLogs::f_Stream(tf_CStream &_Stream)
	{
		fs_StreamFilterVector(_Stream, m_Filters);
		_Stream % m_BatchSize;
	}


	template <typename tf_CStream>
	void CDistributedAppLogReader::CGetLogEntries::f_Stream(tf_CStream &_Stream)
	{
		fs_StreamFilterVector(_Stream, m_Filters);
		_Stream % m_BatchSize;
		if (_Stream.f_GetVersion() >= CDistributedAppLogReporter::EProtocolVersion_LastSeenEntrySentinel)
			_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReader::CSubscribeLogEntries::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Filters;
		_Stream % m_fOnEntry;
		if (_Stream.f_GetVersion() >= CDistributedAppLogReporter::EProtocolVersion_LastSeenEntrySentinel)
			_Stream % m_Flags;
	}
}
