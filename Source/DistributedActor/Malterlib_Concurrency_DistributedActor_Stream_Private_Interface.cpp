// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CDistributedActorWriteStream::f_FeedInterface
		(
			uint32 _SequenceID
			, CActorSubscription &&_Subscription
			, TCDistributedActor<> const &_Actor
			, NContainer::TCVector<uint32> &&_Hierarchy
			, CDistributedActorProtocolVersions &&_ProtocolVersions
		)
	{
		auto &State = f_GetState();
		
		*this << _SequenceID;
		
		uint8 bHasSubscription = !!_Subscription;
		*this << bHasSubscription;
		if (bHasSubscription)
			f_Feed(fg_Move(_Subscription), _SequenceID);
		
		*this << _Hierarchy;
		*this << _ProtocolVersions;
		
		NStr::CStr ActorID;

		if (_Actor)
		{
			auto &ActorFunctors = State.m_ActorFunctors[_SequenceID];
			auto &Interface = ActorFunctors.m_Interfaces.f_Insert();
			ActorID = NCryptography::fg_RandomID();
			Interface.m_ActorID = ActorID;
			Interface.m_Actor = _Actor;
			Interface.m_Hierarchy = fg_Move(_Hierarchy);
			Interface.m_ProtocolVersions = fg_Move(_ProtocolVersions);
		}

		*this << ActorID;
	}
	
	TCDistributedActor<> CDistributedActorReadStream::f_ConsumeInterface
		(
			uint32 _TypeHash
			, CDistributedActorProtocolVersions const &_SupportedVersions
			, NContainer::TCVector<uint32> &o_Hierarchy
			, CDistributedActorProtocolVersions &o_ProtocolVersions
			, CActorSubscription &o_Subscription
			, uint32 &o_SubscriptionID
		)
	{
		auto &State = f_GetState();
		uint32 SequenceID;
		*this >> SequenceID;
		o_SubscriptionID = SequenceID; 

		uint8 bHasSubscription;
		*this >> bHasSubscription;
		
		if (bHasSubscription)
			f_Consume(o_Subscription, SequenceID);
		
		*this >> o_Hierarchy;
		o_Hierarchy.f_Sort();
		*this >> o_ProtocolVersions;
		
		NStr::CStr ActorID;
		*this >> ActorID;
		
		if (ActorID.f_IsEmpty())
			return {};
		
		auto DistributionManager = State.m_DistributionManager.f_Lock();
		
		if (o_Hierarchy.f_BinarySearch(_TypeHash) < 0)
			DMibError("Expected actor type not found in shared remote actor interface");
		
		uint32 Version;
		if (!o_ProtocolVersions.f_HighestSupportedVersion(_SupportedVersions, Version)) 
			DMibError("No commonly supported interface version");
		
		auto Actor = fg_ConstructRemoteDistributedActor<CActor>(DistributionManager, ActorID, State.m_pHost, Version);
		State.m_Subscriptions[SequenceID].m_ActorReferences.f_Insert(Actor);
		
		return {Actor};
	}
}
