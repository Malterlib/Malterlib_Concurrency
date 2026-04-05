// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
			, NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &&_pFunction
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

		NStr::CStr ActorID;
		NStr::CStr FunctionID;

		if (_Actor && _pFunction && !_pFunction->f_IsEmpty())
		{
			auto &ActorFunctors = State.m_ActorFunctors[_SequenceID];
			auto &ActorFunctor = ActorFunctors.m_ActorFunctors.f_Insert();
			ActorID = NCryptography::fg_RandomID();
			ActorFunctor.m_ActorID = ActorID;
			ActorFunctor.m_Actor = fg_Move(_Actor);

			FunctionID = NCryptography::fg_RandomID(ActorFunctor.m_Functions);
			ActorFunctor.m_Functions[FunctionID] = fg_Move(_pFunction);
		}

		*this << ActorID;
		*this << FunctionID;
	}
}
