// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
	void CDistributedAppLogReader_LogKeyAndEntry::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_LogInfoKey;
		_Stream % m_Entry;
	}
}
