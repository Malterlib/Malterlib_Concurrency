// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedAppSensorReader_SensorFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_Scope;
		_Stream % m_Identifier;
		_Stream % m_IdentifierScope;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReader_SensorReadingFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_SensorFilter;

		_Stream % m_MinSequence;
		_Stream % m_MaxSequence;

		_Stream % m_MinTimestamp;
		_Stream % m_MaxTimestamp;

		_Stream % m_Flags;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReader_SensorKeyAndReading::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_SensorInfoKey;
		_Stream % m_Reading;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReader_SensorStatusFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_SensorFilter;
		if (_Stream.f_GetVersion() >= CDistributedAppSensorReporter::EProtocolVersion_SensorStatusFilter)
			_Stream % m_Flags;
	}
}
