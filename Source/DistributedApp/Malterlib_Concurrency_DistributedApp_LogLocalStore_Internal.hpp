// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CKey>
	tf_CKey CDistributedAppLogStoreLocal::CInternal::f_GetDatabaseKey(CDistributedAppLogReporter::CLogInfo const &_LogInfo) const
	{
		tf_CKey Key;
		Key.m_DbPrefix = m_Prefix;
		Key.m_Prefix = tf_CKey::mc_Prefix;
		Key.m_HostID = _LogInfo.m_HostID;
		if (_LogInfo.m_Scope.f_IsOfType<CDistributedAppLogReporter::CLogScope_Application>())
			Key.m_ApplicationName = _LogInfo.m_Scope.f_GetAsType<CDistributedAppLogReporter::CLogScope_Application>().m_ApplicationName;
		Key.m_Identifier = _LogInfo.m_Identifier;
		Key.m_IdentifierScope = _LogInfo.m_IdentifierScope;

		return Key;
	}
}
