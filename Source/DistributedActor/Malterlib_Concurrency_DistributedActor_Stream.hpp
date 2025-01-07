// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/AsyncGenerator>

namespace NMib::NConcurrency
{
	template <uint32 t_SubscriptionID>
	TCActorSubscriptionWithID<t_SubscriptionID>::TCActorSubscriptionWithID(CNullPtr)
	{
	}

	template <uint32 t_SubscriptionID>
	TCActorSubscriptionWithID<t_SubscriptionID>::TCActorSubscriptionWithID(uint32 _SubscriptionID)
		: mp_ID(_SubscriptionID)
	{
	}
	
	template <uint32 t_SubscriptionID>
	TCActorSubscriptionWithID<t_SubscriptionID>::TCActorSubscriptionWithID(CActorSubscription &&_Subscription, uint32 _SubscriptionID)
		: CActorSubscription(fg_Move(_Subscription))
		, mp_ID(_SubscriptionID)
	{
	}

	template <uint32 t_SubscriptionID>
	CActorSubscription &&TCActorSubscriptionWithID<t_SubscriptionID>::f_GetSubscription() &&
	{
		return fg_Move(*this);
	}

	template <uint32 t_SubscriptionID>
	CActorSubscription &TCActorSubscriptionWithID<t_SubscriptionID>::f_GetSubscription() &
	{
		return *this;
	}

	template <uint32 t_SubscriptionID>
	CActorSubscription const &TCActorSubscriptionWithID<t_SubscriptionID>::f_GetSubscription() const &
	{
		return *this;
	}

	template <uint32 t_SubscriptionID>
	uint32 TCActorSubscriptionWithID<t_SubscriptionID>::f_GetID() const
	{
		return mp_ID;
	}
	
	template <uint32 t_SubscriptionID>
	void TCActorSubscriptionWithID<t_SubscriptionID>::f_SetID(uint32 _SubscriptionID)
	{
		mp_ID = _SubscriptionID;
	}
	
	template <uint32 t_SubscriptionID>
	auto TCActorSubscriptionWithID<t_SubscriptionID>::operator = (CActorSubscription &&_Subscription) -> TCActorSubscriptionWithID &
	{
		*static_cast<CActorSubscription *>(this) = fg_Move(_Subscription);
		return *this;
	}
	
	template <typename t_CFunction, uint32 t_SubscriptionID>
	TCActorFunctorWithID<t_CFunction, t_SubscriptionID>::TCActorFunctorWithID
		(
			TCActor<CActor> &&_Actor
			, typename TCActorFunctor<t_CFunction>::CFunction &&_fFunctor
			, CActorSubscription &&_Subscription
			, uint32 _SubscriptionID
		)
		: TCActorFunctor<t_CFunction>(fg_Move(_Actor), fg_Move(_fFunctor), fg_Move(_Subscription))
		, mp_SubscriptionID(_SubscriptionID)
	{
	}

	template <typename t_CFunction, uint32 t_SubscriptionID>
	TCActorFunctorWithID<t_CFunction, t_SubscriptionID>::TCActorFunctorWithID(CNullPtr)
		: TCActorFunctor<t_CFunction>()
	{
	}
	
	template <typename t_CFunction, uint32 t_SubscriptionID>
	TCActorFunctorWithID<t_CFunction, t_SubscriptionID>::TCActorFunctorWithID(TCActorFunctor<t_CFunction> &&_ActorFunctor)
		: TCActorFunctor<t_CFunction>(fg_Move(_ActorFunctor))
	{
	}

	template <typename t_CFunction, uint32 t_SubscriptionID>
	uint32 TCActorFunctorWithID<t_CFunction, t_SubscriptionID>::f_GetID() const
	{
		return mp_SubscriptionID;
	}

	template <typename t_CFunction, uint32 t_SubscriptionID>
	void TCActorFunctorWithID<t_CFunction, t_SubscriptionID>::f_SetID(uint32 _SubscriptionID)
	{
		mp_SubscriptionID = _SubscriptionID;
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::TCDistributedActorInterfaceWithID(CNullPtr)
	{
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::TCDistributedActorInterfaceWithID
		(
			TCDistributedActorInterfaceShare<t_CInterface> &&_Share
			, CActorSubscription &&_Subscription
			, uint32 _SubscriptionID
		)
		: TCDistributedActorInterface<t_CInterface>(fg_TempCopy(_Share.f_GetActor()), fg_Move(_Subscription))
		, mp_Hierarchy(fg_Move(_Share.m_Hierarchy))
		, mp_ProtocolVersions(fg_Move(_Share.m_ProtocolVersions))
		, mp_SubscriptionID(_SubscriptionID)
	{
	}

	template <typename t_CInterface, uint32 t_SubscriptionID>
	uint32 TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_GetID() const
	{
		return mp_SubscriptionID;
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	void TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_SetID(uint32 _SubscriptionID)
	{
		mp_SubscriptionID = _SubscriptionID;
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	NContainer::TCVector<uint32> const &TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_GetHierarchy() const
	{
		return mp_Hierarchy;
	}

	template <typename t_CInterface, uint32 t_SubscriptionID>
	NContainer::TCVector<uint32> &TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_GetHierarchy()
	{
		return mp_Hierarchy;
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	CDistributedActorProtocolVersions const &TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_GetProtocolVersions() const
	{
		return mp_ProtocolVersions;
	}
	
	template <typename t_CInterface, uint32 t_SubscriptionID>
	CDistributedActorProtocolVersions &TCDistributedActorInterfaceWithID<t_CInterface, t_SubscriptionID>::f_GetProtocolVersions()
	{
		return mp_ProtocolVersions;
	}

	template <typename t_CReturnType>
	template <typename tf_CStream>
	void TCAsyncGenerator<t_CReturnType>::f_Stream(tf_CStream &_Stream)
	{
		bool bHasGenerator = !!mp_pData;
		_Stream % bHasGenerator;
		if (bHasGenerator)
		{
			auto *pContext = (NPrivate::CDistributedActorStreamContext *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());

			if constexpr (tf_CStream::mc_Direction == NStream::EStreamDirection_Consume)
			{
				TCActorFunctorWithID<TCFuture<NStorage::TCOptional<t_CReturnType>> (), gc_SubscriptionNotRequired> fGetNext;
				fGetNext.f_SetID(pContext->f_GetAutomaticNotRequiredSubscriptionID());
				
				_Stream % fg_Move(fGetNext);
				mp_pData = fg_Construct(fg_Move(fGetNext), pContext->f_ActorProtocolVersion() >= EDistributedActorProtocolVersion_PipelinedAsyncGenerators, false);
			}
			else
			{
				TCActorFunctorWithID<TCFuture<NStorage::TCOptional<t_CReturnType>> (), gc_SubscriptionNotRequired> fGetNext(fg_Move(mp_pData->m_fGetNext));
				fGetNext.f_SetID(pContext->f_GetAutomaticNotRequiredSubscriptionID());

				_Stream % fg_Move(fGetNext);
			}
		}
	}
}


#include "Malterlib_Concurrency_DistributedActor_Stream_Call.hpp"
