// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NEncoding;

	uint32 CDistributedAppActor::f_CommandLine_RemoveAllTrust(bool _bConfirm)
	{
		CStr TrustDatabaseLocation = mp_Settings.m_RootDirectory / ("TrustDatabase.{}"_f << mp_Settings.m_AppName);
		CStr TrustDatabaseCommandLineLocation = mp_Settings.m_RootDirectory / ("CommandLineTrustDatabase.{}"_f << mp_Settings.m_AppName);

		auto fLogAction = [&](CStr const &_ToLog)
			{
				if (_bConfirm)
					DMibConOut("{cc}{\n}", _ToLog);
				else
					DMibConOut("Would {}{\n}", _ToLog);
			}
		;

		auto fClearDatabase = [&](CStr const &_Root)
			{
				CStr BasicConfigFile = CFile::fs_AppendPath(_Root, "BasicConfig.json");
				if (CFile::fs_FileExists(BasicConfigFile))
				{
					auto BasicConfig = CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(BasicConfigFile), BasicConfigFile);
					BasicConfig.f_RemoveMember("CAPrivateKey");
					BasicConfig.f_RemoveMember("CACertificate");
					if (_bConfirm)
						CFile::fs_WriteStringToFile(BasicConfigFile, BasicConfig.f_ToString());
					fLogAction(fg_Format("write '{}': {}", BasicConfigFile, BasicConfig.f_ToString(nullptr)));
				}
				CStr ClientConnections = CFile::fs_AppendPath(_Root, "ClientConnections");
				if (CFile::fs_FileExists(ClientConnections))
				{
					if (_bConfirm)
						CFile::fs_DeleteDirectoryRecursive(ClientConnections);
					fLogAction(fg_Format("delete directory '{}'", ClientConnections));
				}
				CStr ListenConfigs = CFile::fs_AppendPath(_Root, "ListenConfigs");
				if (CFile::fs_FileExists(ListenConfigs))
				{
					if (_bConfirm)
						CFile::fs_DeleteDirectoryRecursive(ListenConfigs);
					fLogAction(fg_Format("delete directory '{}'", ListenConfigs));
				}
				CStr Clients = CFile::fs_AppendPath(_Root, "Clients");
				if (CFile::fs_FileExists(Clients))
				{
					if (_bConfirm)
						CFile::fs_DeleteDirectoryRecursive(Clients);
					fLogAction(fg_Format("delete directory '{}'", Clients));
				}
				CStr ServerCertificates = CFile::fs_AppendPath(_Root, "ServerCertificates");
				if (CFile::fs_FileExists(ServerCertificates))
				{
					if (_bConfirm)
						CFile::fs_DeleteDirectoryRecursive(ServerCertificates);
					fLogAction(fg_Format("delete directory '{}'", ServerCertificates));
				}
			}
		;

		fClearDatabase(TrustDatabaseLocation);
		if (CFile::fs_FileExists(TrustDatabaseCommandLineLocation))
		{
			if (_bConfirm)
				CFile::fs_DeleteDirectoryRecursive(TrustDatabaseCommandLineLocation);
			fLogAction(fg_Format("delete directory '{}'", TrustDatabaseCommandLineLocation));
		}

		if (!_bConfirm)
		{
			DMibConErrOut("{\n}No actions were performed.{\n}{\n}");
			DMibConErrOut("Are you really sure you want to delete all certificates, client connections and clients?{\n}{\n}");
			DMibConErrOut("Confirm with --yes{\n}{\n}");
		}

		return 0;
	}
}
