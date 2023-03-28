// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore.h"
#include "Malterlib_Concurrency_DistributedApp_SensorLocalStore_Database.h"

namespace NMib::NConcurrency::NSensorStoreLocalDatabase
{
	constexpr NStr::CStr const CSensorKey::mc_Prefix = NStr::gc_Str<"Sensor">;
	constexpr NStr::CStr const CSensorReadingKey::mc_Prefix = NStr::gc_Str<"SensorReading">;
	constexpr NStr::CStr const CSensorReadingByTime::mc_Prefix = NStr::gc_Str<"SensorReadingByDate">;

	CSensorReadingKey CSensorReadingByTime::f_Key() const
	{
		CSensorReadingKey Key;
		Key.m_DbPrefix = m_DbPrefix;
		Key.m_Prefix = CSensorReadingKey::mc_Prefix;

		Key.m_UniqueSequence = m_UniqueSequence;

		Key.m_HostID = m_HostID;
		Key.m_ApplicationName = m_ApplicationName;
		Key.m_Identifier = m_Identifier;
		Key.m_IdentifierScope = m_IdentifierScope;

		return Key;
	}

	CSensorKey CSensorReadingKey::f_SensorKey() const &
	{
		CSensorKey Return = *this;

		Return.m_Prefix = CSensorKey::mc_Prefix;

		return Return;
	}

	CSensorKey CSensorReadingKey::f_SensorKey() &&
	{
		CSensorKey Return = fg_Move(*this);

		Return.m_Prefix = CSensorKey::mc_Prefix;

		return Return;
	}

	CDistributedAppSensorReporter::CSensorInfoKey CSensorKey::f_SensorInfoKey() const &
	{
		return
			{
				m_HostID
				, m_ApplicationName
				? CDistributedAppSensorReporter::CSensorScope{CDistributedAppSensorReporter::CSensorScope_Application{m_ApplicationName}}
				: CDistributedAppSensorReporter::CSensorScope_Host{}
				, m_Identifier
				, m_IdentifierScope
			}
		;
	}

	CDistributedAppSensorReporter::CSensorInfoKey CSensorKey::f_SensorInfoKey() &&
	{
		return
			{
				fg_Move(m_HostID)
				, m_ApplicationName
				? CDistributedAppSensorReporter::CSensorScope{CDistributedAppSensorReporter::CSensorScope_Application{fg_Move(m_ApplicationName)}}
				: CDistributedAppSensorReporter::CSensorScope_Host{}
				, fg_Move(m_Identifier)
				, fg_Move(m_IdentifierScope)
			}
		;
	}

	CDistributedAppSensorReporter::CSensorReading CSensorReadingValue::f_Reading(CSensorReadingKey const &_Key) const &
	{
		return
			{
				_Key.m_UniqueSequence
				, m_Timestamp
				, m_Data
			}
		;
	}

	CDistributedAppSensorReporter::CSensorReading CSensorReadingValue::f_Reading(CSensorReadingKey const &_Key) &&
	{
		return
			{
				_Key.m_UniqueSequence
				, m_Timestamp
				, fg_Move(m_Data)
			}
		;
	}
}
