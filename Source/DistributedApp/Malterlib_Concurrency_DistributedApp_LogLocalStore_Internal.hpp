// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CKey, typename tf_CInfoOrKey>
	tf_CKey CDistributedAppLogStoreLocal::CInternal::fs_GetDatabaseKey(CStr const &_Prefix, tf_CInfoOrKey const &_LogInfo)
	{
		tf_CKey Key;
		Key.m_DbPrefix = _Prefix;
		Key.m_Prefix = tf_CKey::mc_Prefix;
		Key.m_HostID = _LogInfo.m_HostID;
		if (_LogInfo.m_Scope.template f_IsOfType<CDistributedAppLogReporter::CLogScope_Application>())
			Key.m_ApplicationName = _LogInfo.m_Scope.template f_GetAsType<CDistributedAppLogReporter::CLogScope_Application>().m_ApplicationName;
		Key.m_Identifier = _LogInfo.m_Identifier;
		Key.m_IdentifierScope = _LogInfo.m_IdentifierScope;

		return Key;
	}

	template <typename tf_CKey, typename tf_CInfoOrKey>
	tf_CKey CDistributedAppLogStoreLocal::CInternal::f_GetDatabaseKey(tf_CInfoOrKey const &_LogInfo) const
	{
		return fs_GetDatabaseKey<tf_CKey>(m_Prefix, _LogInfo);
	}
}
