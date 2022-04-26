// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CFunction, uint32 tf_SubscriptionID>
	void CDistributedActorWriteStream::f_Feed(TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID> &&_ActorFunctor)
	{
		using CFunction = typename TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID>::CFunction;

		NStorage::TCSharedPointer<NPrivate::CStreamingFunction> pFunction;
		if (!_ActorFunctor.f_IsEmpty())
			pFunction = fg_Construct<NPrivate::TCStreamingFunction<CFunction>>(fg_Move(_ActorFunctor.f_GetFunctor()));

		f_FeedActorFunctor
			(
				fg_Move(_ActorFunctor.f_GetActor())
				, fg_Move(pFunction)
				, _ActorFunctor.f_GetID()
				, fg_Move(_ActorFunctor.f_GetSubscription())
			)
		;
	}

	template <typename tf_CFunction, uint32 tf_SubscriptionID>
	void CDistributedActorReadStream::f_Consume(TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID> &_ActorFunctor)
	{
		uint32 SequenceID;
		*this >> SequenceID;

		uint8 bHasSubscription;
		*this >> bHasSubscription;

		CActorSubscription Subscription;
		if (bHasSubscription)
			f_Consume(Subscription, SequenceID);

		TCActor<> Actor;
		f_ConsumeActor(Actor, SequenceID);

		using CFunction = typename TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID>::CFunction;
		using FFunctionSignature = typename NFunction::TCFunctionInfo<CFunction>::template TCCallType<0>;

		NStr::CStr FunctionID;
		*this >> FunctionID;

		if (!FunctionID.f_IsEmpty())
			_ActorFunctor = {fg_Move(Actor), NPrivate::TCStreamingFunctionHelper<FFunctionSignature>::fs_Functor(FunctionID), fg_Move(Subscription), SequenceID};
	}
}
