// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CKey>
	tf_CKey CDistributedAppSensorStoreLocal::CInternal::f_GetDatabaseKey(CDistributedAppSensorReporter::CSensorInfo const &_SensorInfo) const
	{
		tf_CKey Key;
		Key.m_DbPrefix = m_Prefix;
		Key.m_Prefix = tf_CKey::mc_Prefix;
		Key.m_HostID = _SensorInfo.m_HostID;
		if (_SensorInfo.m_Scope.f_IsOfType<CDistributedAppSensorReporter::CSensorScope_Application>())
			Key.m_ApplicationName = _SensorInfo.m_Scope.f_GetAsType<CDistributedAppSensorReporter::CSensorScope_Application>().m_ApplicationName;
		Key.m_Identifier = _SensorInfo.m_Identifier;
		Key.m_IdentifierScope = _SensorInfo.m_IdentifierScope;

		return Key;
	}
}
