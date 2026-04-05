// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/ActorFunctor>
#include <Mib/Concurrency/ActorInterface>

DMibDefineSharedPointerType(NMib::NConcurrency::NPrivate::CDistributedActorStreamContextState, true, true);

namespace NMib::NConcurrency
{
	enum EDistributedActorProtocolVersion : uint32
	{
		EDistributedActorProtocolVersion_Min = 0x102

		, EDistributedActorProtocolVersion_InitialPublishFinishedSupported = 0x103
		, EDistributedActorProtocolVersion_ClaimedSubscriptionsSupported = 0x104
		, EDistributedActorProtocolVersion_SubscriptionDestroyedSupported = 0x105
		, EDistributedActorProtocolVersion_GeneralExceptionsSupported = 0x106
		, EDistributedActorProtocolVersion_LostConnectionNotificationsSupported = 0x107
		, EDistributedActorProtocolVersion_WaitForRemotePublishProcessing = 0x108
		, EDistributedActorProtocolVersion_PipelinedAsyncGenerators = 0x109
		, EDistributedActorProtocolVersion_MultipleAuthenticationHandlers = 0x10A
		, EDistributedActorProtocolVersion_PrioritySupport = 0x10B

		, EDistributedActorProtocolVersion_Current = 0x10B
	};

	static constexpr const uint32 gc_SubscriptionNotRequired = TCLimitsInt<uint32>::mc_Max - uint32(65536);

	template <uint32 t_SubscriptionID = 0>
	struct TCActorSubscriptionWithID : public CActorSubscription
	{
		TCActorSubscriptionWithID(TCActorSubscriptionWithID &&) = default;
		TCActorSubscriptionWithID & operator = (TCActorSubscriptionWithID &&) = default;
		TCActorSubscriptionWithID(TCActorSubscriptionWithID const &) = delete;
		TCActorSubscriptionWithID & operator = (TCActorSubscriptionWithID const &) = delete;

		TCActorSubscriptionWithID(CNullPtr);
		TCActorSubscriptionWithID(uint32 _SubscriptionID = t_SubscriptionID);
		TCActorSubscriptionWithID(CActorSubscription &&_Subscription, uint32 _SubscriptionID = t_SubscriptionID);

		TCActorSubscriptionWithID &operator = (CActorSubscription &&_Subscription);

		CActorSubscription &&f_GetSubscription() &&;
		CActorSubscription &f_GetSubscription() &;
		CActorSubscription const &f_GetSubscription() const &;

		uint32 f_GetID() const;
		void f_SetID(uint32 _SubcriptionID);

	private:
		uint32 mp_ID = t_SubscriptionID;
	};

	template <typename t_CFunction, uint32 t_SubscriptionID = 0>
	struct TCActorFunctorWithID : public TCActorFunctor<t_CFunction>
	{
		TCActorFunctorWithID() = default;
		TCActorFunctorWithID(TCActorFunctorWithID &&) = default;
		TCActorFunctorWithID & operator = (TCActorFunctorWithID &&) = default;
		TCActorFunctorWithID(TCActorFunctorWithID const &) = delete;
		TCActorFunctorWithID & operator = (TCActorFunctorWithID const &) = delete;

		TCActorFunctorWithID(CNullPtr);
		TCActorFunctorWithID(TCActorFunctor<t_CFunction> &&_ActorFunctor);
		TCActorFunctorWithID
			(
				TCActor<CActor> &&_Actor
				, typename TCActorFunctor<t_CFunction>::CFunction &&_fFunctor
				, CActorSubscription &&_Subscription = nullptr
				, uint32 _SubscriptionID = t_SubscriptionID
			)
		;

		uint32 f_GetID() const;
		void f_SetID(uint32 _SubcriptionID);

	protected:
		uint32 mp_SubscriptionID = t_SubscriptionID;
	};

	template <typename t_CReturnType, uint32 t_SubscriptionID = 0>
	struct TCAsyncGeneratorWithID : public TCAsyncGenerator<t_CReturnType>
	{
		TCAsyncGeneratorWithID() = default;
		TCAsyncGeneratorWithID(TCAsyncGeneratorWithID &&) = default;
		TCAsyncGeneratorWithID & operator = (TCAsyncGeneratorWithID &&) = default;
		TCAsyncGeneratorWithID(TCAsyncGeneratorWithID const &) = delete;
		TCAsyncGeneratorWithID & operator = (TCAsyncGeneratorWithID const &) = delete;

		TCAsyncGeneratorWithID(CNullPtr);
		TCAsyncGeneratorWithID(TCAsyncGenerator<t_CReturnType> &&_ActorFunctor, uint32 _SubscriptionID = t_SubscriptionID);

		TCAsyncGenerator<t_CReturnType> &f_Generator();

		uint32 f_GetID() const;
		void f_SetID(uint32 _SubcriptionID);
		void f_SetSubscription(CActorSubscription &&_Subscription);

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

	protected:
		uint32 mp_SubscriptionID = t_SubscriptionID;
	};

	template <typename t_CInterface>
	using TCDistributedActorInterface = TCActorInterface<TCDistributedActorWrapper<t_CInterface>>;

	template <typename t_CInterface, uint32 t_SubscriptionID = 0>
	struct TCDistributedActorInterfaceWithID : public TCDistributedActorInterface<t_CInterface>
	{
		TCDistributedActorInterfaceWithID() = default;
		TCDistributedActorInterfaceWithID(TCDistributedActorInterfaceWithID &&) = default;
		TCDistributedActorInterfaceWithID & operator = (TCDistributedActorInterfaceWithID &&) = default;
		TCDistributedActorInterfaceWithID(TCDistributedActorInterfaceWithID const &) = delete;
		TCDistributedActorInterfaceWithID & operator = (TCDistributedActorInterfaceWithID const &) = delete;

		TCDistributedActorInterfaceWithID(CNullPtr);
		TCDistributedActorInterfaceWithID
			(
				TCDistributedActorInterfaceShare<t_CInterface> &&_Share
				, CActorSubscription &&_Subscription = nullptr
				, uint32 _SubscriptionID = t_SubscriptionID
			)
		;

		uint32 f_GetID() const;
		void f_SetID(uint32 _SubcriptionID);

		NContainer::TCVector<uint32> const &f_GetHierarchy() const;
		NContainer::TCVector<uint32> &f_GetHierarchy();
		CDistributedActorProtocolVersions const &f_GetProtocolVersions() const;
		CDistributedActorProtocolVersions &f_GetProtocolVersions();

	protected:
		NContainer::TCVector<uint32> mp_Hierarchy;
		CDistributedActorProtocolVersions mp_ProtocolVersions;
		uint32 mp_SubscriptionID = t_SubscriptionID;
	};

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCDistributedActorInterface<tf_CActor> &&_Actor, tfp_CParams && ...p_Params)
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CParams...>
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash), tf_VirtualCall>(fg_Move(_Actor.f_GetActor()), fg_Forward<tfp_CParams>(p_Params)...);
	}

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename tf_CInterface
		, uint32 tf_SubscriptionID
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCDistributedActorInterfaceWithID<tf_CInterface, tf_SubscriptionID> &&_Actor, tfp_CParams && ...p_Params)
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CInterface, tfp_CParams...>
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash), tf_VirtualCall>(fg_Move(_Actor.f_GetActor()), fg_Forward<tfp_CParams>(p_Params)...);
	}

#define DMibDistributedStreamDeclare(d_Class) DMibStreamDeclare(d_Class, NConcurrency::CDistributedActorWriteStream, Feed); \
	DMibStreamDeclare(d_Class, NConcurrency::CDistributedActorReadStream, Consume);

#define DMibDistributedStreamImplement(d_Class) DMibStreamImplement(d_Class, NConcurrency::CDistributedActorWriteStream, Feed); \
	DMibStreamImplement(d_Class, NConcurrency::CDistributedActorReadStream, Consume);
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor_Stream_Private.h"
