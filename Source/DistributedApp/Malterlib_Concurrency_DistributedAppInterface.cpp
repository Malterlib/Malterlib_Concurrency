// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedAppInterface.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceBackup::CDistributedAppInterfaceBackup()
	{
		DMibPublishActorFunction(CDistributedAppInterfaceBackup::f_AppendManifest);
		DMibPublishActorFunction(CDistributedAppInterfaceBackup::f_SubscribeInitialFinished);
		DMibPublishActorFunction(CDistributedAppInterfaceBackup::f_SubscribeBackupStopped);
	}

	CDistributedAppInterfaceBackup::~CDistributedAppInterfaceBackup() = default;

	CDistributedAppInterfaceClient::CDistributedAppInterfaceClient()
	{
		DMibPublishActorFunction(CDistributedAppInterfaceClient::f_PreUpdate);
		DMibPublishActorFunction(CDistributedAppInterfaceClient::f_PreStop);
		DMibPublishActorFunction(CDistributedAppInterfaceClient::f_GetAppStartResult);
		DMibPublishActorFunction(CDistributedAppInterfaceClient::f_StartBackup);
	}

	CDistributedAppInterfaceClient::~CDistributedAppInterfaceClient() = default;

	CDistributedAppInterfaceServer::CDistributedAppInterfaceServer()
	{
		DMibPublishActorFunction(CDistributedAppInterfaceServer::f_RegisterDistributedApp);
		DMibPublishActorFunction(CDistributedAppInterfaceServer::f_RegisterConfigFiles);
		DMibPublishActorFunction(CDistributedAppInterfaceServer::f_GetSensorReporter);
		DMibPublishActorFunction(CDistributedAppInterfaceServer::f_GetLogReporter);
	}

	CDistributedAppInterfaceServer::~CDistributedAppInterfaceServer() = default;

	template <typename tf_CStream>
	void CDistributedAppInterfaceServer::CRegisterInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UpdateType;
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportResources)
		{
			_Stream % m_Resources_Files;
			_Stream % m_Resources_FilesPerProcess;
			_Stream % m_Resources_Threads;
			_Stream % m_Resources_Processes;
		}
		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportLaunchID)
			_Stream % m_LaunchID;

		if (_Stream.f_GetVersion() >= EProtocolVersion_SupportMaxMapCount)
			_Stream % m_Resources_MaxMapCount;
	}
	DMibDistributedStreamImplement(CDistributedAppInterfaceServer::CRegisterInfo);

	bool CDistributedAppInterfaceServer::CRegisterInfo::f_IsSameIgnoringLaunchID(CRegisterInfo const &_Right) const
	{
		return m_UpdateType == _Right.m_UpdateType
			&& m_Resources_Files == _Right.m_Resources_Files
			&& m_Resources_FilesPerProcess == _Right.m_Resources_FilesPerProcess
			&& m_Resources_Threads == _Right.m_Resources_Threads
			&& m_Resources_Processes == _Right.m_Resources_Processes
			&& m_Resources_MaxMapCount == _Right.m_Resources_MaxMapCount
		;
	}

	NStr::CStr CDistributedAppInterfaceServer::fs_MonitorConfigTypeToString(EMonitorConfigType _Type)
	{
		switch (_Type)
		{
		case EMonitorConfigType_GeneralText: return NStr::gc_Str<"General Text">;
		case EMonitorConfigType_GeneralBinary: return NStr::gc_Str<"General Binary">;
		case EMonitorConfigType_Registry: return NStr::gc_Str<"Registry">;
		case EMonitorConfigType_Json: return NStr::gc_Str<"JSON">;
		}

		return NStr::gc_Str<"Unknown">;
	}
}
