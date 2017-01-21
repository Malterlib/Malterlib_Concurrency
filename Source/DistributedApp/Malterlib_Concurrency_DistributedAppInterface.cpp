// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedAppInterface.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceClient::CDistributedAppInterfaceClient()
	{
		DMibPublishActorFunction(CDistributedAppInterfaceClient::f_PreUpdate);
	}
	
	CDistributedAppInterfaceClient::~CDistributedAppInterfaceClient() = default;

	CDistributedAppInterfaceServer::CDistributedAppInterfaceServer()
	{
		DMibPublishActorFunction(CDistributedAppInterfaceServer::f_RegisterDistributedApp);
	}
	
	CDistributedAppInterfaceServer::~CDistributedAppInterfaceServer() = default;
}
