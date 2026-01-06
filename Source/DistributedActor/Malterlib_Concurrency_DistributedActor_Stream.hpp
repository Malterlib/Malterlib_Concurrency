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

	namespace NPrivate
	{
		template <typename t_CReturnType>
		struct TCStreamingFunctionAsyncGenerator : public TCStreamingFunction<NFunction::TCFunctionMovable<TCFuture<NStorage::TCOptional<t_CReturnType>> ()>>
		{
			using CFunction = NFunction::TCFunctionMovable<TCFuture<NStorage::TCOptional<t_CReturnType>> ()>;
			using CSuper = TCStreamingFunction<NFunction::TCFunctionMovable<TCFuture<NStorage::TCOptional<t_CReturnType>> ()>>;

			TCStreamingFunctionAsyncGenerator(CFunction &&_fFunction, bool _bIsCoroutine)
				: CSuper(fg_Move(_fFunction))
				, mp_bIsCoroutine(_bIsCoroutine)
			{
			}

			~TCStreamingFunctionAsyncGenerator()
			{
				if (!mp_bIsCoroutine || !this->m_fFunction)
					return;

				auto pFunctor = static_cast<NPrivate::TCAsyncGeneratorCoroutineFunctor<t_CReturnType> *>(this->m_fFunction.f_GetFunctor());

				TCAsyncGeneratorDataShared<t_CReturnType>::fs_AsyncDestroy(pFunctor->m_pRunState).f_DiscardResult();
			}

		private:
			bool mp_bIsCoroutine = false;
		};
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
				auto &fGetNext = this->mp_pData->m_fGetNext;

				NStorage::TCSharedPointer<NPrivate::CStreamingFunction> pFunction;
				if (!fGetNext.f_IsEmpty())
					pFunction = fg_Construct<NPrivate::TCStreamingFunctionAsyncGenerator<t_CReturnType>>(fg_Move(fGetNext.f_GetFunctor()), this->mp_pData->m_bIsCoroutine);

				_Stream.f_FeedActorFunctor
					(
						fg_Move(fGetNext.f_GetActor())
						, fg_Move(pFunction)
						, pContext->f_GetAutomaticNotRequiredSubscriptionID()
						, fg_Move(fGetNext.f_GetSubscription())
					)
				;
			}
		}
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::TCAsyncGeneratorWithID(TCAsyncGenerator<t_CReturnType> &&_ActorFunctor, uint32 _SubscriptionID)
		: TCAsyncGenerator<t_CReturnType>(fg_Move(_ActorFunctor))
		, mp_SubscriptionID(_SubscriptionID)
	{
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::TCAsyncGeneratorWithID(CNullPtr)
	{
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	uint32 TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::f_GetID() const
	{
		return mp_SubscriptionID;
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	void TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::f_SetID(uint32 _SubscriptionID)
	{
		mp_SubscriptionID = _SubscriptionID;
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	TCAsyncGenerator<t_CReturnType> &TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::f_Generator()
	{
		return *this;
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	void TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::f_SetSubscription(CActorSubscription &&_Subscription)
	{
		DMibFastCheck(this->mp_pData);
		this->mp_pData->m_fGetNext.f_GetSubscription() = g_ActorSubscription
			/ [Subscription0 = fg_Move(_Subscription), Subscription1 = fg_Move(this->mp_pData->m_fGetNext.f_GetSubscription())]() mutable -> TCFuture<void>
			{
				TCFutureVector<void> Results;
				if (Subscription0)
					fg_Exchange(Subscription0, nullptr)->f_Destroy() > Results;
				if (Subscription1)
					fg_Exchange(Subscription1, nullptr)->f_Destroy() > Results;

				co_await fg_AllDone(Results);

				co_return {};
			}
		;
	}

	template <typename t_CReturnType, uint32 t_SubscriptionID>
	template <typename tf_CStream>
	void TCAsyncGeneratorWithID<t_CReturnType, t_SubscriptionID>::f_Stream(tf_CStream &_Stream)
	{
		bool bHasGenerator = !!this->mp_pData;
		_Stream % bHasGenerator;
		if (bHasGenerator)
		{
			auto *pContext = (NPrivate::CDistributedActorStreamContext *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());

			if constexpr (tf_CStream::mc_Direction == NStream::EStreamDirection_Consume)
			{
				TCActorFunctorWithID<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> fGetNext;
				fGetNext.f_SetID(mp_SubscriptionID);

				_Stream % fg_Move(fGetNext);
				this->mp_pData = fg_Construct(fg_Move(fGetNext), pContext->f_ActorProtocolVersion() >= EDistributedActorProtocolVersion_PipelinedAsyncGenerators, false);
			}
			else
			{
				auto &fGetNext = this->mp_pData->m_fGetNext;

				NStorage::TCSharedPointer<NPrivate::CStreamingFunction> pFunction;
				if (!fGetNext.f_IsEmpty())
					pFunction = fg_Construct<NPrivate::TCStreamingFunctionAsyncGenerator<t_CReturnType>>(fg_Move(fGetNext.f_GetFunctor()), this->mp_pData->m_bIsCoroutine);

				_Stream.f_FeedActorFunctor
					(
						fg_Move(fGetNext.f_GetActor())
						, fg_Move(pFunction)
						, mp_SubscriptionID
						, fg_Move(fGetNext.f_GetSubscription())
					)
				;
			}
		}
	}
}


#include "Malterlib_Concurrency_DistributedActor_Stream_Call.hpp"
