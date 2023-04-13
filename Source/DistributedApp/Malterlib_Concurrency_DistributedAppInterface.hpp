// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStr>
	void CDistributedAppInterfaceServer::CConfigFile::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Type: {}") << CDistributedAppInterfaceServer::fs_MonitorConfigTypeToString(m_Type);
	}

	template <typename tf_CStream>
	void CDistributedAppInterfaceServer::CConfigFile::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Type;
	}

	template <typename tf_CStream>
	void CDistributedAppInterfaceServer::CConfigFiles::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Files;
	}
}
