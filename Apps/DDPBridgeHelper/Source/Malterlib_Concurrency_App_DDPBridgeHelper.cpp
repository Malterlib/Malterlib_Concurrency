// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Daemon/Daemon>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>

#include "Malterlib_Concurrency_App_DDPBridgeHelper.h"

namespace NMib::NConcurrency::NDDPBridgeHelper
{
	CDDPBridgeHelperActor::CDDPBridgeHelperActor()
		: CDistributedAppActor(CDistributedAppActor_Settings{"TrustDDPBridge", false})
	{
	}
	
	CDDPBridgeHelperActor::~CDDPBridgeHelperActor()
	{
	}

	TCContinuation<void> CDDPBridgeHelperActor::fp_StartApp(NEncoding::CEJSON const &_Params)
	{
		TCContinuation<void> Continuation;
		Continuation.f_SetResult();
		return Continuation;				
	}
	
	TCContinuation<void> CDDPBridgeHelperActor::fp_StopApp()
	{	
		TCSharedPointer<CCanDestroyTracker> pCanDestroy = fg_Construct();
		
		return pCanDestroy->m_Continuation;
	}
}
