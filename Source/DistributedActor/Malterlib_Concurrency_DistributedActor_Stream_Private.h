// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include <Mib/Core/Core>

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorStreamContextState;
	struct CDistributedActorSubscriptionReferenceState;
}

DMibDefineSharedPointerType(NMib::NConcurrency::NPrivate::CDistributedActorSubscriptionReferenceState, true, false);

#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Function.h"

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorStreamContext
	{
		CDistributedActorStreamContext(uint32 _ActorProtocolVersion, bool _bCallInitiator);
		~CDistributedActorStreamContext();

		CDistributedActorStreamContext(CDistributedActorStreamContext const &);
		CDistributedActorStreamContext(CDistributedActorStreamContext &&);
		CDistributedActorStreamContext& operator =(CDistributedActorStreamContext const &);
		CDistributedActorStreamContext& operator =(CDistributedActorStreamContext &&);

		bool f_ValidateContext(NStr::CStr &o_Error);
		uint32 f_ActorProtocolVersion() const;
		
#if DMibEnableSafeCheck > 0
		bool f_CorrectMagic() const
		{
			return m_Magic == constant_uint64(0x5541e982e952851c);
		}
		uint64 m_Magic = constant_uint64(0x5541e982e952851c);
#endif
		NStorage::TCSharedPointer<CDistributedActorStreamContextState> m_pState;
	};
}

namespace NMib::NConcurrency
{
	struct CDistributedActorWriteStream : public NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector>
	{
		inline_always NPrivate::CDistributedActorStreamContextState &f_GetState();
		inline_always NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &f_GetStatePtr();
		
		DMibStreamImplementOperators(CDistributedActorWriteStream);

		void f_Feed(CActorSubscription &&_Subscription, uint32 _SubscriptionSequenceID = TCLimitsInt<uint32>::mc_Max);
		template <uint32 tf_SubscriptionID>
		void f_Feed(TCActorSubscriptionWithID<tf_SubscriptionID> &&_Subscription);

		void f_FeedActor(TCActor<> const &_Actor);
		void f_FeedFunction(NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &&_pFunction);
		void f_FeedActorFunctor(TCActor<> &&_Actor, NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &&_pFunction, uint32 _SequenceID, CActorSubscription &&_Subscription);
		void f_FeedInterface
			(
				uint32 _SequenceID
				, CActorSubscription &&_Subscription
				, TCDistributedActor<> const &_Actor
				, NContainer::TCVector<uint32> &&_Hierarchy
				, CDistributedActorProtocolVersions &&_ProtocolVersions
			)
		;
		
		template <typename tf_CActor>
		void f_Feed(TCActor<tf_CActor> const &_Actor);
		
		template <typename tf_CActor>
		void f_Feed(TCActor<tf_CActor> &&_Actor);

		template <typename ...tf_CFunctionOptions>
		void f_Feed(NFunction::TCFunction<tf_CFunctionOptions...> const &_Function);

		template <typename tf_CSignature>
		void f_Feed(NFunction::TCFunctionMutable<tf_CSignature> const &_Function);

		template <typename tf_CSignature>
		void f_Feed(NFunction::TCFunctionMovable<tf_CSignature> &&_Function);

		template <typename tf_CSignature>
		void f_Feed(NFunction::TCFunctionMutable<tf_CSignature> &&_Function);

		template <typename ...tf_CFunctionOptions>
		void f_Feed(NFunction::TCFunction<tf_CFunctionOptions...> &&_Function);

		template <typename tf_CFunction, uint32 tf_SubscriptionID>
		void f_Feed(TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID> &&_ActorFunctor);

		template <typename tf_CFunction, uint32 tf_SubscriptionID>
		void f_Feed(TCDistributedActorInterfaceWithID<tf_CFunction, tf_SubscriptionID> &&_ActorInterface);

		template <typename tf_CType>
		void f_Feed(TCAsyncResult<tf_CType> const &_AsyncResult);

		template <typename tf_CType>
		void f_Feed(TCAsyncResult<tf_CType> &&_AsyncResult);
	};
	
	struct CDistributedActorReadStream : public NStream::CBinaryStreamMemoryPtr<>
	{
		inline_always NPrivate::CDistributedActorStreamContextState &f_GetState();
		inline_always NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &f_GetStatePtr();

		DMibStreamImplementOperators(CDistributedActorReadStream);

		void f_Consume(CActorSubscription &_Subscription, uint32 _SubscriptionSequenceID = TCLimitsInt<uint32>::mc_Max); 
		template <uint32 tf_SubscriptionID>
		void f_Consume(TCActorSubscriptionWithID<tf_SubscriptionID> &_Subscription);
		
		void f_ConsumeActor(TCActor<> &_Actor, uint32 _SubscriptionSequenceID);
		TCDistributedActor<> f_ConsumeInterface
			(
				uint32 _TypeHash
				, CDistributedActorProtocolVersions const &_SupportedVersions
				, NContainer::TCVector<uint32> &o_Hierarchy
				, CDistributedActorProtocolVersions &o_ProtocolVersions
				, CActorSubscription &o_Subscription
				, uint32 &o_SubscriptionID
			)
		;
		
		template <typename tf_CActor>
		void f_Consume(TCActor<tf_CActor> &_Actor);
		
		template <typename ...tf_CFunctionOptions>
		void f_Consume(NFunction::TCFunction<tf_CFunctionOptions...> &_Function);

		template <typename tf_CSignature>
		void f_Consume(NFunction::TCFunctionMovable<tf_CSignature> &_Function);

		template <typename tf_CSignature>
		void f_Consume(NFunction::TCFunctionMutable<tf_CSignature> &_Function);

		template <typename tf_CFunction, uint32 tf_SubscriptionID>
		void f_Consume(TCActorFunctorWithID<tf_CFunction, tf_SubscriptionID> &_ActorFunctor);

		template <typename tf_CFunction, uint32 tf_SubscriptionID>
		void f_Consume(TCDistributedActorInterfaceWithID<tf_CFunction, tf_SubscriptionID> &_ActorInterface);

		template <typename tf_CType>
		void f_Consume(TCAsyncResult<tf_CType> &_AsyncResult);
	};
}
