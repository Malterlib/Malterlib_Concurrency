// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CDistributedActorReadStream::f_Consume(CActorSubscription &_Subscription, uint32 _SubscriptionSequenceID) 
	{
		auto &State = f_GetState();
		NStr::CStr SubscriptionID;
		*this >> SubscriptionID;
		if (_SubscriptionSequenceID != TCLimitsInt<uint32>::mc_Max)
			*this >> _SubscriptionSequenceID;

		NStorage::TCUniquePointer<NPrivate::CDistributedActorSubscriptionReference> pSubscription = fg_Construct();
		auto &SubscriptionState = *(pSubscription->m_pState = fg_Construct());
		SubscriptionState.m_pHost = State.m_pHost;
		SubscriptionState.m_DistributionManager = State.m_DistributionManager;
		SubscriptionState.m_SubscriptionID = SubscriptionID;
		SubscriptionState.m_LastExecutionID = State.m_LastExecutionID;

		State.m_ActorFunctors[_SubscriptionSequenceID].m_PendingSubscriptions.f_Insert(pSubscription->m_pState);
		
		auto DistributionManager = State.m_DistributionManager.f_Lock();
		DMibCheck(DistributionManager); // Should be called from place where distribution manager is referenced
		if (DistributionManager)
		{
			DistributionManager.f_Bind<&CActorDistributionManager::fp_RegisterRemoteSubscription>
				(
					f_GetStatePtr()
					, SubscriptionID
					, _SubscriptionSequenceID
				)
				.f_DiscardResult()
			;
		}

		_Subscription = fg_Move(pSubscription);
	}

	void CDistributedActorWriteStream::f_Feed(CActorSubscription &&_Subscription, uint32 _SubscriptionSequenceID)
	{
		auto &State = f_GetState();
		NStr::CStr SubscriptionID = NCryptography::fg_RandomID();
		*this << SubscriptionID;
		if (_SubscriptionSequenceID != TCLimitsInt<uint32>::mc_Max)
			*this << _SubscriptionSequenceID;
		auto &Subscription = State.m_Subscriptions[_SubscriptionSequenceID];
		DMibCheck(Subscription.m_Subscription.f_IsEmpty() && Subscription.m_SubscriptionID.f_IsEmpty());
		Subscription.m_SubscriptionID = SubscriptionID;
		Subscription.m_Subscription = fg_Move(_Subscription);
	}
}

namespace NMib::NConcurrency::NPrivate
{
	CDistributedActorSubscriptionReference::CDistributedActorSubscriptionReference() = default;
	CDistributedActorSubscriptionReference::~CDistributedActorSubscriptionReference() = default;

	CDistributedActorSubscriptionReferenceState::CDistributedActorSubscriptionReferenceState() = default;
	CDistributedActorSubscriptionReferenceState::~CDistributedActorSubscriptionReferenceState()
	{
		auto DistributionManager = m_DistributionManager.f_Lock();
		if (!DistributionManager)
			return;
		DistributionManager.f_Bind<&CActorDistributionManager::fp_DestroyRemoteSubscription>(m_pHost, m_SubscriptionID, m_LastExecutionID).f_DiscardResult();
	}

	TCFuture<void> CDistributedActorSubscriptionReferenceState::f_Destroy()
	{
		auto DistributionManager = m_DistributionManager.f_Lock();
		if (!DistributionManager)
			return g_Void;
		
		m_DistributionManager.f_Clear();
		
		return DistributionManager(&CActorDistributionManager::fp_DestroyRemoteSubscription, m_pHost, m_SubscriptionID, m_LastExecutionID);
	}
	
	TCFuture<void> CDistributedActorSubscriptionReference::f_Destroy()
	{
		if (!m_pState)
			return g_Void;
		
		return m_pState->f_Destroy();
	}
}
