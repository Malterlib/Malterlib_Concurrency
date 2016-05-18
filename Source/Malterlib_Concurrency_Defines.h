// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Function/Function>

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib
{
	namespace NConcurrency
	{
#		define DMibConcurrency_CheckFunctionCalls 
		class CActor;
		class CDefaultActorHolder;
		template <typename t_CActor>
		class TCActorInternal;

		class CConcurrencyManager;

		template <typename t_CActor, typename t_CFunctor>
		struct TCActorResultCall;
		
		template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
		struct TCActorCall;
		
		struct CInternalActorAllocator : public NMem::CAllocator_Heap
		{
		};
		
		enum EPriority : mint
		{
			EPriority_Low
			, EPriority_Normal
			, EPriority_Max
		};
		
		template <typename t_CActor>
		class TCWeakActor;
		
		template <typename t_CActor>
		class TCActor
		{
			friend class CConcurrencyManager;
			template <typename t_CActor2>
			friend class TCActor;
			template <typename t_CActor2>
			friend class TCWeakActor;
			template <typename t_CActor2>
			friend class TCActorInternal;

		public:
			using CActorInternal = TCActorInternal<t_CActor>;
			using CContainedActor = t_CActor;
			
		private:
			NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> m_pInternalActor;
			
		public:
			TCActor();
			TCActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor);
			TCActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor);
			TCActor(CNullPtr);
			TCActor(TCActor const &_Other);
			TCActor(TCActor &&_Other);
			template <typename tf_CActor>
			TCActor(TCActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCActor(TCActor<tf_CActor> &&_Other);
			TCActor &operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor);
			TCActor &operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor);
			TCActor &operator =(TCActor const &_Other);
			TCActor &operator =(TCActor &&_Other);
			TCActor &operator =(CNullPtr);

			template <typename tf_CActor>
			TCActor &operator =(TCActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCActor &operator =(TCActor<tf_CActor> &&_Other);
			CActorInternal *operator ->() const;
			void f_Clear();
			bool f_IsEmpty() const;
			TCWeakActor<t_CActor> f_Weak() const;
			
			template <typename tf_CActor>
			bool operator < (TCActor<tf_CActor> const& _Right) const;
			template <typename tf_CActor>
			bool operator < (TCWeakActor<tf_CActor> const& _Right) const;

			template <typename tf_CActor>
			bool operator == (TCActor<tf_CActor> const& _Right) const;
			template <typename tf_CActor>
			bool operator == (TCWeakActor<tf_CActor> const& _Right) const;

			inline_small explicit operator bool() const;
			
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
			
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto f_CallByValue(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
			
			template <typename tf_FResult>
			auto operator / (tf_FResult &&_fResult) const
				-> TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
			;
			
			inline_always TCActor<CActor> const &f_GetActor() const;
		};

		template <typename tf_CActor, typename tf_CActorSource>
		TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> const &_Actor);

		
		template <typename tf_CActor>
		TCActor<tf_CActor> fg_ThisActor(tf_CActor *_pActor);

		class CConcurrentActor;
		class CConcurrentActorLowPrio;
		class CTimerActor;

		template <typename t_CType, typename t_CLock>
		class TCLockActor;

		struct CActorDestroyEventLoop
		{
			NFunction::TCFunction<void ()> m_fProcess;
			NFunction::TCFunction<void ()> m_fWake;
		};
		
		struct CCanDestroyTracker;
		
		class CActorCallbackReference
		{
		public:
			virtual ~CActorCallbackReference()
			{
			}
		};
		
		typedef NPtr::TCUniquePointer<CActorCallbackReference> CActorCallback;
		
		CConcurrencyManager &fg_ConcurrencyManager();
		TCActor<CConcurrentActor> const &fg_ConcurrentActor();
		TCActor<CConcurrentActor> const &fg_ConcurrentActorLowPrio();

		TCActor<CTimerActor> fg_TimerActor();
	}
}

template <>
struct NMib::NPtr::TCHasIntrusiveRefcount<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{
};

template <>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{ 
};

template <typename t_CActor>
struct NMib::NPtr::TCHasIntrusiveRefcount<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CActor>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{ 
};

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif