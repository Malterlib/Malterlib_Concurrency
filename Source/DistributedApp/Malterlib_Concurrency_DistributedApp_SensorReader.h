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
		enum ESensorFlag
		{
			ESensorFlag_None = 0
			, ESensorFlag_IgnoreRemoved = DMibBit(0)
		};

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NStorage::TCOptional<NStr::CStr> m_HostID;
		NStorage::TCOptional<CDistributedAppSensorReporter::CSensorScope> m_Scope;
		NStorage::TCOptional<NStr::CStr> m_Identifier; // Supports wildcards
		NStorage::TCOptional<NStr::CStr> m_IdentifierScope; // Supports wildcards
		ESensorFlag m_Flags = ESensorFlag_None;
	};

	struct CDistributedAppSensorReader_SensorReadingFilter
	{
		enum ESensorReadingsFlag
		{
			ESensorReadingsFlag_None = 0
			, ESensorReadingsFlag_ReportNewestFirst = DMibBit(0) ///< Not supported for subscriptions
			, ESensorReadingsFlag_OnlyProblems = DMibBit(1)
			, ESensorReadingsFlag_IgnoreSnoozed = DMibBit(2) ///< Only valid for querying status. Ignored for readings.
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

	struct CDistributedAppSensorReader_SensorReadingSubscriptionFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);
		CDistributedAppSensorReader_SensorReadingFilter f_ToReadingFilter() const;

		CDistributedAppSensorReader_SensorFilter m_SensorFilter;

		NStorage::TCOptional<uint64> m_MinSequence; ///< If specified historic readings will also be reported, but will not be applied to not yet generated readings
		NStorage::TCOptional<NTime::CTime> m_MinTimestamp; ///< If specified historic readings will also be reported, but will not be applied to not yet generated readings

		CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag m_Flags = CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_None;
	};

	struct CDistributedAppSensorReader_SensorStatusFilter
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDistributedAppSensorReader_SensorFilter m_SensorFilter;

		CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag m_Flags = CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_None;
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

		enum ESensorChange : uint32
		{
			ESensorChange_AddedOrUpdated
			, ESensorChange_Removed
		};

		using CSensorChange = NStorage::TCStreamableVariant
			<
				ESensorChange
				, NStorage::TCMember<CDistributedAppSensorReporter::CSensorInfo, ESensorChange_AddedOrUpdated>
				, NStorage::TCMember<CDistributedAppSensorReporter::CSensorInfoKey, ESensorChange_Removed>
			>
		;

		struct CGetSensors
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> m_Filters;
			uint32 m_BatchSize = 1024;
		};

		struct CGetSensorReadings
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDistributedAppSensorReader_SensorReadingFilter> m_Filters;
			uint32 m_BatchSize = 1024;
		};

		struct CGetSensorStatus
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CDistributedAppSensorReader_SensorStatusFilter> m_Filters;
			uint32 m_BatchSize = 1024;
		};

		template <typename tf_CStream, typename tf_CFilter>
		static void fs_StreamFilterVector(tf_CStream &_Stream, NContainer::TCVector<tf_CFilter> &o_Filters);

		virtual auto f_GetSensors(CGetSensors &&_Params) -> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>>> = 0;
		virtual auto f_GetSensorReadings(CGetSensorReadings &&_Params) -> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>> = 0;
		virtual auto f_GetSensorStatus(CGetSensorStatus &&_Params) -> TCFuture<TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>>> = 0;
		virtual auto f_SubscribeSensors
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorFilter> &&_Filters
				, TCActorFunctorWithID<TCFuture<void> (CSensorChange &&_Change)> &&_fOnChange
			)
			-> TCFuture<TCActorSubscriptionWithID<>> = 0
		;
		virtual auto f_SubscribeSensorReadings
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorReadingSubscriptionFilter> &&_Filters
				, TCActorFunctorWithID<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading &&_Reading)> &&_fOnReading
			)
			-> TCFuture<TCActorSubscriptionWithID<>> = 0
		;
		virtual auto f_SubscribeSensorStatus
			(
				NContainer::TCVector<CDistributedAppSensorReader_SensorStatusFilter> &&_Filters
				, TCActorFunctorWithID<TCFuture<void> (CDistributedAppSensorReader_SensorKeyAndReading &&_Reading)> &&_fOnReading
			)
			-> TCFuture<TCActorSubscriptionWithID<>> = 0
		;
	};
}

#include "Malterlib_Concurrency_DistributedApp_SensorReader.hpp"
