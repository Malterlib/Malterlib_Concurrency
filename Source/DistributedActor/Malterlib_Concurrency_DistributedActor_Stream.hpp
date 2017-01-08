// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
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
			, NFunction::TCFunctionMutable<t_CFunction> &&_fFunctor
			, CActorSubscription &&_Subscription
			, uint32 _SubscriptionID
		)
		: TCActorFunctor<t_CFunction>(fg_Move(_Actor), fg_Move(_fFunctor), fg_Move(_Subscription))
		, mp_SubscriptionID(_SubscriptionID)
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
}

#include "Malterlib_Concurrency_DistributedActor_Stream_Call.hpp"
