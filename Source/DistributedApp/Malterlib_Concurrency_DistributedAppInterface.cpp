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
	}
	
	CDistributedAppInterfaceServer::~CDistributedAppInterfaceServer() = default;
	
	template <typename tf_CStream>
	void CDistributedAppInterfaceServer::CRegisterInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UpdateType;
		if (_Stream.f_GetVersion() >= 0x103)
		{
			_Stream % m_Resources_Files;
			_Stream % m_Resources_FilesPerProcess;
			_Stream % m_Resources_Threads;
			_Stream % m_Resources_Processes;
		}
	}
	DMibDistributedStreamImplement(CDistributedAppInterfaceServer::CRegisterInfo);
	
	bool CDistributedAppInterfaceServer::CRegisterInfo::operator == (CRegisterInfo const &_Right) const
	{
		return m_UpdateType == _Right.m_UpdateType 
			&& m_Resources_Files == _Right.m_Resources_Files 
			&& m_Resources_FilesPerProcess == _Right.m_Resources_FilesPerProcess 
			&& m_Resources_Threads == _Right.m_Resources_Threads 
			&& m_Resources_Processes == _Right.m_Resources_Processes
		;
	}
}
