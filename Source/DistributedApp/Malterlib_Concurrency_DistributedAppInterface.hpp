// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedAppInterfaceBackup::CManifestConfig::f_Stream(tf_CStream &_Stream)
	{
		CDirectoryManifestConfig::EManifestConfigStreamVersion Version = CDirectoryManifestConfig::EManifestConfigStreamVersion_Min;

		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportManifestConfigFlagsAndMaxDigestSize)
			Version = CDirectoryManifestConfig::EManifestConfigStreamVersion_SupportFlagsAndMaxDigestSize;

		NFile::CDirectoryManifestConfig::f_Stream(_Stream, Version);
	}

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
