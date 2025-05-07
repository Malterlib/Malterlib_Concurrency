// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Daemon/Daemon>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JsonDirectory>

#include "Malterlib_Concurrency_App_DDPBridgeHelper.h"

namespace NMib::NConcurrency::NDDPBridgeHelper
{
	CDDPBridgeHelperActor::CDDPBridgeHelperActor()
		: CDistributedAppActor(CDistributedAppActor_Settings{"TrustDDPBridge"})
	{
	}
	
	CDDPBridgeHelperActor::~CDDPBridgeHelperActor()
	{
	}

	TCFuture<void> CDDPBridgeHelperActor::fp_StartApp(NEncoding::CEJsonSorted const _Params)
	{
		co_return {};
	}
	
	TCFuture<void> CDDPBridgeHelperActor::fp_StopApp()
	{	
		TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();
		
		return pCanDestroy->f_Future();
	}
}
