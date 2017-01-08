// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CDistributedActorWriteStream::f_FeedActorFunctor
		(
			TCActor<> &&_Actor
			, NPtr::TCSharedPointer<NPrivate::CStreamingFunction> &&_pFunction
			, uint32 _SequenceID
			, CActorSubscription &&_Subscription
		)
	{
		auto &State = f_GetState();
		
		*this << _SequenceID;
		
		uint8 bHasSubscription = !!_Subscription;
		*this << bHasSubscription;
		if (bHasSubscription)
			f_Feed(fg_Move(_Subscription), _SequenceID);
		
		auto &ActorFunctors = State.m_ActorFunctors[_SequenceID];
		auto &ActorFunctor = ActorFunctors.m_ActorFunctors.f_Insert();
		NStr::CStr ActorID = NCryptography::fg_RandomID();
		*this << ActorID;
		ActorFunctor.m_ActorID = ActorID;
		ActorFunctor.m_Actor = fg_Move(_Actor);
		
		NStr::CStr FunctionID = NCryptography::fg_RandomID();
		*this << FunctionID;
		ActorFunctor.m_Functions[FunctionID] = fg_Move(_pFunction);
	}
}
