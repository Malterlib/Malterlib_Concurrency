// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NSensorStoreLocalDatabase
{
	template <typename tf_CStream>
	void CSensorKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_DbPrefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_HostID);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_ApplicationName);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Identifier);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_IdentifierScope);
	}

	template <typename tf_CStream>
	void CSensorKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_DbPrefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_HostID);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_ApplicationName);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Identifier);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_IdentifierScope);
	}

	template <typename tf_CStream>
	void CSensorValue::f_Stream(tf_CStream &_Stream)
	{
		uint32 Version = gc_Version;
		_Stream % Version;
		DMibBinaryStreamVersion(_Stream, Version);
		_Stream % m_Info;

		if (_Stream.f_GetVersion() >= CDistributedAppSensorReporter::EProtocolVersion_UniqueSequenceAtLastCleanup)
			_Stream % m_UniqueSequenceAtLastCleanup;
	}

	template <typename tf_CStream>
	void CSensorReadingKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		CSensorKey::f_FeedLexicographic(_Stream);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueSequence);
	}

	template <typename tf_CStream>
	void CSensorReadingKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		CSensorKey::f_ConsumeLexicographic(_Stream);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueSequence);
	}

	template <typename tf_CStr>
	void CSensorReadingKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}:{}:{}:{}:{}:{}:{}") << m_DbPrefix << m_Prefix << m_HostID << m_ApplicationName << m_Identifier << m_IdentifierScope << m_UniqueSequence;
	}

	template <typename tf_CStream>
	void CSensorReadingByTime::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_DbPrefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Timestamp);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueSequence);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_HostID);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_ApplicationName);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Identifier);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_IdentifierScope);
	}

	template <typename tf_CStream>
	void CSensorReadingByTime::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_DbPrefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Timestamp);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueSequence);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_HostID);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_ApplicationName);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Identifier);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_IdentifierScope);
	}

	template <typename tf_CStream>
	void CSensorReadingByTimeValue::f_Stream(tf_CStream &_Stream)
	{
	}

	template <typename tf_CStream>
	void CSensorReadingValue::f_Stream(tf_CStream &_Stream)
	{
		uint32 Version = gc_Version;
		_Stream % Version;
		DMibBinaryStreamVersion(_Stream, Version);
		_Stream % m_Timestamp;
		_Stream % m_Data;
 	}
}
