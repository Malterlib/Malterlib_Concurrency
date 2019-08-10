// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CDistributedActorWriteStream::f_FeedActor(TCActor<> const &_Actor)
	{
		auto &State = f_GetState();
		auto &ActorFunctors = State.m_ActorFunctors[TCLimitsInt<uint32>::mc_Max];
		if (ActorFunctors.m_ActorFunctors.f_IsEmpty())
			ActorFunctors.m_ActorFunctors.f_Insert();
		DMibCheck(ActorFunctors.m_ActorFunctors.f_GetLen() == 1); // This could happen if you use -1 as subscription ID. This has been reserved for free actors/functions. 
		auto &ActorFunctor = ActorFunctors.m_ActorFunctors.f_GetFirst();
		DMibCheck(ActorFunctor.m_ActorID.f_IsEmpty()); // You can send one and only one actor in the call, otherwise it's impossible to know which actor to dispatch the functions to
		NStr::CStr ActorID = NCryptography::fg_RandomID();
		*this << ActorID;
		ActorFunctor.m_ActorID = ActorID;
		ActorFunctor.m_Actor = _Actor;
	}

	class CRemoteDispatchActorActorHolder : public CDefaultActorHolder
	{
	public:
		CRemoteDispatchActorActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		{
		}

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor)
		{
			CDefaultActorHolder::fp_QueueProcessDestroy
				(
					[this, Functor = fg_Move(_Functor)]() mutable
					{
						// this is referenced by shared pointer in Functor

						auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
						auto pOldDispatchData = ThreadLocal.m_pCurrentRemoteDispatchActorData;
						ThreadLocal.m_pCurrentRemoteDispatchActorData = static_cast<NPrivate::CDistributedActorData *>(mp_pDistributedActorData.f_Get());
						auto Cleanup = g_OnScopeExit > [&]
							{
								ThreadLocal.m_pCurrentRemoteDispatchActorData = pOldDispatchData;
							}
						;
						Functor();
					}
				)
			;
		}

		void fp_QueueProcess(FActorQueueDispatch &&_Functor)
		{
			CDefaultActorHolder::fp_QueueProcess
				(
					[this, Functor = fg_Move(_Functor)]() mutable
					{
						// this is referenced by shared pointer in CDefaultActorHolder::fp_QueueProcess

						auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
						auto pOldDispatchData = ThreadLocal.m_pCurrentRemoteDispatchActorData;
						ThreadLocal.m_pCurrentRemoteDispatchActorData = static_cast<NPrivate::CDistributedActorData *>(mp_pDistributedActorData.f_Get());
						auto Cleanup = g_OnScopeExit > [&]
							{
								ThreadLocal.m_pCurrentRemoteDispatchActorData = pOldDispatchData;
							}
						;
						Functor();
					}
				)
			;
		}
	};
	
	struct CRemoteDispatchActor : public CActor
	{
		using CActorHolder = CRemoteDispatchActorActorHolder;
	};
	
	void CDistributedActorReadStream::f_ConsumeActor(TCActor<> &_Actor, uint32 _SubscriptionSequenceID)
	{
		auto &State = f_GetState();
		using namespace NActorDistributionManagerInternal;
		NStr::CStr ActorID;
		*this >> ActorID;
		
		if (ActorID.f_IsEmpty())
			return;
		
		auto DistributionManager = State.m_DistributionManager.f_Lock();
		if (!DistributionManager)
			DMibError("Distribution manager unexpectedly gone");
		
		_Actor = fg_ConstructRemoteDistributedActor<CRemoteDispatchActor>
			(
				DistributionManager
				, ActorID
				, State.m_pHost
				, f_GetVersion()
			)
		;
		
		State.m_Subscriptions[_SubscriptionSequenceID].m_ActorReferences.f_Insert(_Actor);
	}
}
