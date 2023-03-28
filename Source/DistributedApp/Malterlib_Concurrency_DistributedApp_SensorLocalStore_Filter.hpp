// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib::NConcurrency::NSensorStore
{
	template <typename tf_CKey>
	bool fg_FilterSensorKey(tf_CKey const &_Key, CDistributedAppSensorReader_SensorFilter const &_Filter)
	{
		if (_Filter.m_HostID && _Key.m_HostID != *_Filter.m_HostID)
			return false;

		if (_Filter.m_Scope)
		{
			switch (_Filter.m_Scope->f_GetTypeID())
			{
			case CDistributedAppSensorReporter::ESensorScope_Unsupported:
				{
					return false;
				}
				break;
			case CDistributedAppSensorReporter::ESensorScope_Host:
				{
					if (!_Key.m_ApplicationName.f_IsEmpty())
						return false;
				}
				break;
			case CDistributedAppSensorReporter::ESensorScope_Application:
				{
					auto &Application = _Filter.m_Scope->f_Get<CDistributedAppSensorReporter::ESensorScope_Application>();
					if (_Key.m_ApplicationName != Application.m_ApplicationName)
						return false;
				}
				break;
			}
		}

		if (_Filter.m_Identifier && _Key.m_Identifier != *_Filter.m_Identifier)
			return false;

		if (_Filter.m_IdentifierScope && _Key.m_IdentifierScope != *_Filter.m_IdentifierScope)
			return false;

		return true;
	}
}
