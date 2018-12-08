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
	namespace NPrivate
	{
		CStreamingFunction::CStreamingFunction() = default;
		CStreamingFunction::~CStreamingFunction() = default;
	}
	
	void CDistributedActorWriteStream::f_FeedFunction(NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &&_pFunction)
	{
		auto &State = f_GetState();
		
		NStr::CStr FunctionID = NCryptography::fg_RandomID();
		*this << FunctionID;
		
		auto &ActorFunctors = State.m_ActorFunctors[TCLimitsInt<uint32>::mc_Max];
		if (ActorFunctors.m_ActorFunctors.f_IsEmpty())
			ActorFunctors.m_ActorFunctors.f_Insert();
		DMibCheck(ActorFunctors.m_ActorFunctors.f_GetLen() == 1); // This could happen if you use -1 as subscription ID. This has been reserved for free actors/functions. 
		auto &ActorFunctor = ActorFunctors.m_ActorFunctors.f_GetFirst();
		
		ActorFunctor.m_Functions[FunctionID] = fg_Move(_pFunction);
	}
}
