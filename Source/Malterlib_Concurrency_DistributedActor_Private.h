// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include <Mib/Core/Core>

namespace NMib::NConcurrency::NPrivate
{
	struct CDistributedActorStreamContextReceiveState;
	struct CDistributedActorStreamContextSendState;
}

DMibDefineSharedPointerType(NMib::NConcurrency::NPrivate::CDistributedActorStreamContextReceiveState, true, false);
DMibDefineSharedPointerType(NMib::NConcurrency::NPrivate::CDistributedActorStreamContextSendState, true, false);

namespace NMib::NConcurrency::NPrivate
{
	
	struct CDistributedActorData : public ICDistributedActorData
	{
		TCWeakActor<CActorDistributionManager> m_DistributionManager;
		NStr::CStr m_ActorID;
		uint32 m_ProtocolVersion = TCLimitsInt<uint32>::mc_Max;
		bool m_bWasDestroyed = false;
		bool m_bRemote = false;
	};
	
	struct CStreamingFunction : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		virtual ~CStreamingFunction();
		virtual NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call(CDistributedActorReadStream &_Stream) = 0;
	};
	
	template <typename t_FFunction, typename t_FFunctionSignature = typename NFunction::TCFunctionInfo<t_FFunction>::template TCCallType<0>>
	struct TCStreamingFunction;
	
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	struct TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)> : public CStreamingFunction
	{
		TCStreamingFunction(t_FFunction const &_fFunction);
		
		template <mint... tfp_Indices>
		NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
			(
				CDistributedActorReadStream &_Stream
				, NMeta::TCIndices<tfp_Indices...> const &_Indices
			)
		;
		
		NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call(CDistributedActorReadStream &_Stream) override;

		t_FFunction m_fFunction;
	};

	
	struct CStreamActorSend
	{
		CStreamActorSend(TCActor<CActor> const &_Actor, CDistributedActorStreamContextSendState &_State);

		void f_Feed(CDistributedActorWriteStream &_Stream) const;
		
		TCActor<CActor> const &m_Actor;
		CDistributedActorStreamContextSendState &m_State;
	};

	struct CStreamFunctionSend
	{
		CStreamFunctionSend(NPtr::TCSharedPointer<CStreamingFunction> &&_pFunction, CDistributedActorStreamContextSendState &_State);
		
		void f_Feed(CDistributedActorWriteStream &_Stream) const;
		
		mutable NPtr::TCSharedPointer<CStreamingFunction> m_pFunction;
		CDistributedActorStreamContextSendState &m_State;
	};

	struct CStreamSubscriptionSend
	{
		struct CSubscriptionReference : public CActorSubscriptionReference
		{
			virtual ~CSubscriptionReference();
		private:
			friend struct CStreamSubscriptionSend;
			NPtr::TCSharedPointer<NPrivate::CDistributedActorData> mp_pActorData;
			NStr::CStr mp_SubscriptionID;
			NStr::CStr mp_LastExecutionID;
		};

		CStreamSubscriptionSend(CActorSubscription &_Subscription, CDistributedActorStreamContextSendState &_State);
		void f_Consume(CDistributedActorReadStream &_Stream);

		CActorSubscription &m_Subscription;
		CDistributedActorStreamContextSendState &m_State;
	};
	
	struct CDistributedActorStreamContextSending
	{
		CDistributedActorStreamContextSending();
		~CDistributedActorStreamContextSending();
		CDistributedActorStreamContextSending(NPtr::TCSharedPointer<CDistributedActorStreamContextSendState> &&_pState);
		CDistributedActorStreamContextSending(CDistributedActorStreamContextSending const &);
		CDistributedActorStreamContextSending(CDistributedActorStreamContextSending &&);
		CDistributedActorStreamContextSending& operator =(CDistributedActorStreamContextSending const &);
		CDistributedActorStreamContextSending& operator =(CDistributedActorStreamContextSending &&);
		
		bool f_ValueReceived();
		
		template <typename tf_CType>
		static tf_CType const &f_GetValueForFeed(tf_CType const &_Value)
		{
			return _Value;
		}

		template <typename tf_CType>
		static tf_CType &f_GetValueForConsume(tf_CType &_Value)
		{
			return _Value;
		}

		template <typename tf_CActor>
		auto f_GetValueForFeed(TCActor<tf_CActor> const &_Actor)
		{
			return CStreamActorSend{_Actor, *m_pState};
		}

		template <typename ...tf_CFunctionOptions>
		auto f_GetValueForFeed(NFunction::TCFunction<tf_CFunctionOptions...> const &_Function)
		{
			static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
			static_assert
				(
					TCIsContinuation<typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn>::mc_Value
					, "Only functions that return a continuation is supported"
				)
			;
			return CStreamFunctionSend{fg_Construct<TCStreamingFunction<NFunction::TCFunction<tf_CFunctionOptions...>>>(_Function), *m_pState};
		}

		auto f_GetValueForConsume(CActorSubscription &_Subscription)
		{
			return CStreamSubscriptionSend(_Subscription, *m_pState);
		}

#if DMibEnableSafeCheck > 0
		bool f_CorrectMagic() const
		{
			return m_Magic == constant_uint64(0x5541e982e952851c);
		}
		uint64 m_Magic = constant_uint64(0x5541e982e952851c);
#endif
		NPtr::TCSharedPointer<CDistributedActorStreamContextSendState> m_pState;
	};
	
	struct CStreamActorReceive
	{
		CStreamActorReceive(TCActor<CActor> &_Actor, CDistributedActorStreamContextReceiveState &_State);
		void f_Consume(CDistributedActorReadStream &_Stream);
		
		TCActor<CActor> &m_Actor;
		CDistributedActorStreamContextReceiveState &m_State;
	};
	
	template <typename t_FFunction, typename t_FFunctionSignature = typename NFunction::TCFunctionInfo<t_FFunction>::template TCCallType<0>>
	struct TCStreamFunctionReceive;

	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	struct TCStreamFunctionReceive<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>
	{
		TCStreamFunctionReceive(t_FFunction &_fFunction, CDistributedActorStreamContextReceiveState &_State);
		void f_Consume(CDistributedActorReadStream &_Stream);
		
		t_FFunction &m_fFunction;
		CDistributedActorStreamContextReceiveState &m_State;
	};

	struct CStreamSubscriptionReceive
	{
		CStreamSubscriptionReceive(CActorSubscription &&_Subscription, CDistributedActorStreamContextReceiveState &_State);
		void f_Feed(CDistributedActorWriteStream &_Stream) const;

		mutable CActorSubscription m_Subscription;
		CDistributedActorStreamContextReceiveState &m_State;
	};
	
	struct CDistributedActorStreamContextReceiving
	{
		CDistributedActorStreamContextReceiving();
		~CDistributedActorStreamContextReceiving();
		CDistributedActorStreamContextReceiving(CDistributedActorStreamContextReceiving const &);
		CDistributedActorStreamContextReceiving(CDistributedActorStreamContextReceiving &&);
		CDistributedActorStreamContextReceiving& operator =(CDistributedActorStreamContextReceiving const &);
		CDistributedActorStreamContextReceiving& operator =(CDistributedActorStreamContextReceiving &&);
		
		template <typename tf_CType>
		static tf_CType const &f_GetValueForFeed(tf_CType const &_Value)
		{
			return _Value;
		}

		template <typename tf_CType>
		static tf_CType &f_GetValueForConsume(tf_CType &_Value)
		{
			return _Value;
		}

		auto f_GetValueForFeed(CActorSubscription const &_Subscription)
		{
			return CStreamSubscriptionReceive(fg_Move(fg_RemoveQualifiers(_Subscription)), *m_pState);
		}
		
		template <typename tf_CActor>
		auto f_GetValueForConsume(TCActor<tf_CActor> &_Actor)
		{
			return CStreamActorReceive(_Actor, *m_pState);
		}
		
		template <typename ...tf_CFunctionOptions>
		auto f_GetValueForConsume(NFunction::TCFunction<tf_CFunctionOptions...> &_Function)
		{
			static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
			static_assert
				(
					TCIsContinuation<typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn>::mc_Value
					, "Only functions that return a continuation is supported"
				)
			;
			return TCStreamFunctionReceive<NFunction::TCFunction<tf_CFunctionOptions...>>(_Function, *m_pState);
		}
		
#if DMibEnableSafeCheck > 0
		bool f_CorrectMagic() const
		{
			return m_Magic == constant_uint64(0x3b9f0ef4263f81a5);
		}
		uint64 m_Magic = constant_uint64(0x3b9f0ef4263f81a5);
#endif
		NPtr::TCSharedPointer<CDistributedActorStreamContextReceiveState> m_pState;
	};
}

#include "Malterlib_Concurrency_DistributedActor_Private.hpp"
