// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Stream.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	CDistributedActorStreamContext::CDistributedActorStreamContext(uint32 _ActorProtocolVersion, bool _bCallInitiator)
		: m_pState(fg_Construct(_ActorProtocolVersion, _bCallInitiator))
	{
	}

	CDistributedActorStreamContext::~CDistributedActorStreamContext() = default;

	CDistributedActorStreamContext::CDistributedActorStreamContext(CDistributedActorStreamContext const &) = default;
	CDistributedActorStreamContext::CDistributedActorStreamContext(CDistributedActorStreamContext &&) = default;
	CDistributedActorStreamContext& CDistributedActorStreamContext::operator =(CDistributedActorStreamContext const &) = default;
	CDistributedActorStreamContext& CDistributedActorStreamContext::operator =(CDistributedActorStreamContext &&) = default;

	bool CDistributedActorStreamContext::f_ValidateContext(NStr::CStr &o_Error)
	{
		auto &State = *m_pState;
		return State.f_IsValid(o_Error);
	}

	uint32 CDistributedActorStreamContext::f_ActorProtocolVersion() const
	{
		auto &State = *m_pState;
		return State.m_ActorProtocolVersion;
	}

	uint32 CDistributedActorStreamContext::f_GetAutomaticNotRequiredSubscriptionID()
	{
		auto &State = *m_pState;
		return State.m_CurrentAutomaticNotRequiredSubscriptionID++;
	}

	uint32 fg_ActorProtocolVersion(void *_pStreamContext)
	{
		CDistributedActorStreamContext *pContext = (CDistributedActorStreamContext *)_pStreamContext;
		DMibFastCheck(pContext && pContext->f_CorrectMagic());

		return pContext->f_ActorProtocolVersion();
	}

	bool CDistributedActorStreamContextState::f_IsValid(NStr::CStr &o_Error) const
	{
		for (auto iActorFunctor = m_ActorFunctors.f_GetIterator(); iActorFunctor; ++iActorFunctor)
		{
			auto SequenceID = iActorFunctor.f_GetKey();
			auto &ActorFunctor = *iActorFunctor;
			if (SequenceID == TCLimitsInt<uint32>::mc_Max)
			{
				if (ActorFunctor.m_ActorFunctors.f_IsEmpty())
				{
					if (ActorFunctor.m_PendingSubscriptions.f_IsEmpty())
					{
						o_Error = "Internal error (no functors and no subscription)";
						return false;
					}
					continue; // It's valid to have just a subscription
				}
				
				if (ActorFunctor.m_ActorFunctors.f_GetLen() > 1)
				{
					o_Error = NStr::fg_Format("It's not valid to map actor functons to 0x{nfh} sequence ID", TCLimitsInt<uint32>::mc_Max);
					return false;
				}
				
				auto &Info = ActorFunctor.m_ActorFunctors.f_GetFirst();
				if (Info.m_Functions.f_IsEmpty() || Info.m_Actor.f_IsEmpty())
				{
					// It does not make sense to specify just a actor, or just a function
					o_Error = "You have to include both an actor and function";
					return false;
				}
			}
			else
			{
				for (auto &Functor : ActorFunctor.m_ActorFunctors)
				{
					if (Functor.m_ActorID.f_IsEmpty())
						return false;
					if (Functor.m_Actor.f_IsEmpty())
						return false;
					if (Functor.m_Functions.f_GetLen() != 1)
						return false;
				}
			}
			if (m_bCallInitiator)
			{
				if (ActorFunctor.m_PendingSubscriptions.f_IsEmpty() && SequenceID < gc_SubscriptionNotRequired)
				{
					o_Error = "No matching subscription for actor functors or interfaces or interfaces";
					return false; // We have registered functors/actors without a matching subscription
				}
				if (ActorFunctor.m_PendingSubscriptions.f_HasMoreThanOneEntry())
				{
					o_Error = "Two subscriptions with the same sequence ID";
					return false;
				}
			}
			else
			{
				if (!m_Subscriptions.f_FindEqual(SequenceID))
				{
					o_Error = "No matching subscription for actor functors or interfaces";
					return false; // We have registered functors/actors without a matching subscription
				}
			}
		}
		
		return true; 
	}
}
