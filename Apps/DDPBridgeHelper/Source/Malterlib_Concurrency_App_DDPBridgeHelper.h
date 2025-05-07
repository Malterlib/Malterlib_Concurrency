// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Daemon/Daemon>

namespace NMib::NConcurrency::NDDPBridgeHelper
{
	struct CDDPBridgeHelperActor : public CDistributedAppActor
	{
		CDDPBridgeHelperActor();
		~CDDPBridgeHelperActor();
		
		struct CServer;
	private:
		TCFuture<void> fp_StartApp(NEncoding::CEJsonSorted const _Params) override;
		TCFuture<void> fp_StopApp() override;
		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override; 
	};
}
