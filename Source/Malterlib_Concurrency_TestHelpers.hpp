// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_TestHelpers.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename ...tfp_CDistributedActors>
		void CDistributedActorTestHelper::f_Publish(TCDistributedActor<CActor> const &_Actor, NStr::CStr const &_Namespace)
		{
			mp_PublishedActors.f_Insert(_Actor);
			mp_Publication = mp_ServerManager
				(
					&CActorDistributionManager::f_PublishActor
					, fg_TempCopy(_Actor)
					, _Namespace
					, NMib::NConcurrency::CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy<tfp_CDistributedActors...>()
				).f_CallSync(60.0)
			;
		}	
		
		template <typename tf_CActor>
		TCDistributedActor<tf_CActor> CDistributedActorTestHelper::f_GetRemoteActor()
		{
			return fg_StaticCast<tf_CActor>(mp_RemoteActor);
		}
	}
}
