// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedAppLogReporter::CSourceLocation::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_File;
		_Stream % m_Line;
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CSourceLocation::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat(DMibPFileLineFormat) << m_File << m_Line;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogScope_Unsupported::f_Stream(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogScope_Host::f_Stream(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogScope_Application::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_ApplicationName;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogData::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Categories;
		_Stream % m_Operations;
		_Stream % m_Message;
		_Stream % m_SourceLocation;
		_Stream % m_Metadata;
		_Stream % m_Severity;
		_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogEntry::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UniqueSequence;
		_Stream % m_Timestamp;
		_Stream % m_Data;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogInfoAutomatic::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_HostName;
		_Stream % m_Scope;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogInfo::f_Stream(tf_CStream &_Stream)
	{
		CLogInfoAutomatic::f_Stream(_Stream);

		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
		_Stream % m_Name;

		if (_Stream.f_GetVersion() >= EProtocolVersion_IgnoreRemoved)
		{
			_Stream % m_LastSeen;
			_Stream % m_bRemoved;
		}
		else if constexpr (tf_CStream::mc_bConsume)
			m_LastSeen = NTime::CTime();

		if (_Stream.f_GetVersion() >= EProtocolVersion_InfoMetadata)
			_Stream % m_Metadata;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogReporter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_fReportEntries);
		_Stream % m_LastSeenUniqueSequence;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CLogInfoKey::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_Scope;
		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CLogInfo::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat
			(
				"    HostID: {}\n"
				"    HostName: {}\n"
				"    Scope: {}\n"
				"    Identifier: {}\n"
				"    IdentifierScope: {}\n"
				"    Name: {}\n"
				"    LastSeen: {}\n"
				"    Removed: {}"
			)
			<< m_HostID
			<< m_HostName
			<< m_Scope
			<< m_Identifier
			<< m_IdentifierScope
			<< m_Name
			<< m_LastSeen
			<< (m_bRemoved ? "true" : "false")
		;
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CLogInfoKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("HostID: {} Scope: {} Identifier: {} IdentifierScope: {}") << m_HostID << m_Scope << m_Identifier << m_IdentifierScope;
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CLogScope_Unsupported::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "Unsupported";
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CLogScope_Host::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "Host";
	}

	template <typename tf_CStr>
	void CDistributedAppLogReporter::CLogScope_Application::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Application: {}") << m_ApplicationName;
	}

	template <typename tf_CStream>
	void CDistributedAppLogReporter::CReportEntriesResult::f_Stream(tf_CStream &_Stream)
	{
		if (_Stream.f_GetVersion() >= EProtocolVersion_ReportEntriesDepth)
			_Stream % m_ReportDepth;
	}
}
