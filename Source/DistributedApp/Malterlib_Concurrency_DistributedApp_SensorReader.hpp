// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		if (_Stream.f_GetVersion() >= CDistributedAppSensorReporter::EProtocolVersion_IgnoreRemoved)
			_Stream % m_Flags;
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
	void CDistributedAppSensorReader_SensorReadingSubscriptionFilter::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_SensorFilter;

		_Stream % m_MinSequence;
		_Stream % m_MinTimestamp;

		_Stream % m_Flags;
	};

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

	template <typename tf_CStream, typename tf_CFilter>
	void CDistributedAppSensorReader::fs_StreamFilterVector(tf_CStream &_Stream, NContainer::TCVector<tf_CFilter> &o_Filters)
	{
		if (_Stream.f_GetVersion() >= CDistributedAppSensorReporter::EProtocolVersion_MultipleFilters)
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
	void CDistributedAppSensorReader::CGetSensors::f_Stream(tf_CStream &_Stream)
	{
		fs_StreamFilterVector(_Stream, m_Filters);
		_Stream % m_BatchSize;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReader::CGetSensorReadings::f_Stream(tf_CStream &_Stream)
	{
		fs_StreamFilterVector(_Stream, m_Filters);
		_Stream % m_BatchSize;
	}

	template <typename tf_CStream>
	void CDistributedAppSensorReader::CGetSensorStatus::f_Stream(tf_CStream &_Stream)
	{
		fs_StreamFilterVector(_Stream, m_Filters);
		_Stream % m_BatchSize;
	}
}
