// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedAppSensorReporter>

namespace NMib::NConcurrency
{
	struct CDistributedAppSensorReader_SensorFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NStorage::TCOptional<NStr::CStr> m_HostID;
		NStorage::TCOptional<CDistributedAppSensorReporter::CSensorScope> m_Scope;
		NStorage::TCOptional<NStr::CStr> m_Identifier;
		NStorage::TCOptional<NStr::CStr> m_IdentifierScope;
	};

	struct CDistributedAppSensorReader_SensorReadingFilter
	{
		enum ESensorReadingsFlag
		{
			ESensorReadingsFlag_None = 0
			, ESensorReadingsFlag_ReportNewestFirst = DMibBit(0)
		};

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppSensorReader_SensorFilter m_SensorFilter;

		NStorage::TCOptional<uint64> m_MinSequence;
		NStorage::TCOptional<uint64> m_MaxSequence;

		NStorage::TCOptional<NTime::CTime> m_MinTimestamp;
		NStorage::TCOptional<NTime::CTime> m_MaxTimestamp;

		ESensorReadingsFlag m_Flags = ESensorReadingsFlag_ReportNewestFirst;
	};

	struct CDistributedAppSensorReader_SensorKeyAndReading
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppSensorReporter::CSensorInfoKey m_SensorInfoKey;
		CDistributedAppSensorReporter::CSensorReading m_Reading;
	};

	struct CDistributedAppSensorReader : public CActor
	{
		CDistributedAppSensorReader();
		~CDistributedAppSensorReader();

		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppSensorReader";

		enum : uint32
		{
			EProtocolVersion_Min = CDistributedAppSensorReporter::EProtocolVersion_Min
			, EProtocolVersion_Current = CDistributedAppSensorReporter::EProtocolVersion_Current
		};

		virtual auto f_GetSensors(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
			-> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>>> = 0
		;
		virtual auto f_GetSensorReadings(CDistributedAppSensorReader_SensorReadingFilter &&_Filter, uint32 _BatchSize)
			-> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>> = 0
		;
		virtual auto f_GetSensorStatus(CDistributedAppSensorReader_SensorFilter &&_Filter, uint32 _BatchSize)
			-> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>> = 0
		;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorReader.hpp"
