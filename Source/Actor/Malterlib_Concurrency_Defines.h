// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Function/Function>

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
{
#	define DMibConcurrency_CheckFunctionCalls

	class CActor;
	class CDefaultActorHolder;
	template <typename t_CActor>
	class TCActorInternal;

	class CConcurrencyManager;
	class CSeparateThreadActorHolder;
	struct CSeparateThreadActor;
	class CDelegatedActorHolder;
	struct CCurrentActorScope;

	template <typename t_CActor, typename t_CFunctor>
	struct TCActorResultCall;

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool tf_bDirectCall>
	struct TCActorCall;

	template <typename t_CActor = CActor>
	class TCActor;

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CReturnValue>
	struct TCPromise;

	template <typename t_CReturn, bool t_bDirectCall = false>
	using TCDispatchedActorCall =
		TCActorCall
		<
			TCActor<CActor>
			, TCFuture<t_CReturn> (CActor::*)(NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()> &&)
			, NStorage::TCTuple<NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()>>
			, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()>>
			, t_bDirectCall
		>
	;

	template <typename t_CReturn, bool t_bDirectCall = false>
	using TCDispatchedWeakActorCall =
		TCActorCall
		<
			TCWeakActor<CActor>
			, TCFuture<t_CReturn> (CActor::*)(NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()> &&)
			, NStorage::TCTuple<NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()>>
			, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCFuture<t_CReturn> ()>>
			, t_bDirectCall
		>
	;

	struct CInternalActorAllocator : public NMemory::CAllocator_Heap
	{
		enum
		{
			mc_bIsDefault = false
		};
	};

	template <typename t_CHolder>
	using TCActorHolderSharedPointer = NStorage::TCSharedPointer<t_CHolder, NStorage::CSupportWeakTag, CInternalActorAllocator>;

	template <typename t_CHolder>
	using TCActorHolderWeakPointer = NStorage::TCWeakPointer<t_CHolder, CInternalActorAllocator>;

	enum EPriority : mint
	{
		EPriority_Low
		, EPriority_Normal
		, EPriority_Max
	};

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CActor>
	class TCActor /// \brief Contain an instance of a CActor
	{
		friend class CConcurrencyManager;
		template <typename t_CActor2>
		friend class TCActor;
		template <typename t_CActor2>
		friend class TCWeakActor;
		template <typename t_CActor2>
		friend class TCActorInternal;

		friend class CDelegatedActorHolder;

	public:
		using CActorInternal = TCActorInternal<t_CActor>;
		using CContainedActor = t_CActor;

	private:
		TCActorHolderSharedPointer<CActorInternal> m_pInternalActor;

		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams, mint... tfp_Indidies>
		void fp_Construct(TCConstruct<void, TCConstruct<tf_CType, tfp_CParams...>, tfp_CHolderParams...> &&_Construct, NMeta::TCIndices<tfp_Indidies...> const&);

	public:
		TCActor();
		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
		TCActor(TCConstruct<tf_CType, tfp_CParams...> &&_Construct, tfp_CHolderParams && ...p_HolderParams);
		TCActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor);
		TCActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor);
		TCActor(CNullPtr);
		TCActor(TCActor const &_Other);
		TCActor(TCActor &&_Other);
		template <typename tf_CActor>
		TCActor(TCActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		TCActor(TCActor<tf_CActor> &&_Other);
		TCActor &operator =(TCActorHolderSharedPointer<CActorInternal> const &_pActor);
		TCActor &operator =(TCActorHolderSharedPointer<CActorInternal> &&_pActor);
		TCActor &operator =(TCActor const &_Other);
		TCActor &operator =(TCActor &&_Other);
		TCActor &operator =(CNullPtr);
		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
		TCActor &operator =(TCConstruct<void, TCConstruct<tf_CType, tfp_CParams...>, tfp_CHolderParams...> &&_Construct);
		template <typename tf_CType, typename ...tfp_CParams>
		TCActor &operator =(TCConstruct<tf_CType, tfp_CParams...> &&_Construct);

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
		auto f_CallDirect(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValue(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValueDirect(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;

		template <typename tf_FResult>
		auto operator / (tf_FResult &&_fResult) const
			-> TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
		;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		inline_always TCActor<CActor> const &f_GetActor() const;
	};

	template <typename tf_CActor, typename tf_CActorSource>
	TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> const &_Actor);


	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);

	class CConcurrentActor;
	class CConcurrentActorLowPrio;
	struct CTimerActor;

	template <typename t_CType, typename t_CLock>
	class TCLockActor;

	struct CActorDestroyEventLoop
	{
		NFunction::TCFunction<void ()> m_fProcess;
		NFunction::TCFunction<void ()> m_fWake;
	};

	struct CCanDestroyTracker;

	class CActorSubscriptionReference
	{
	public:
		virtual ~CActorSubscriptionReference();
		virtual TCFuture<void> f_Destroy() = 0;
	};

	typedef NStorage::TCUniquePointer<CActorSubscriptionReference> CActorSubscription;

	CConcurrencyManager &fg_CurrentConcurrencyManager();
	CConcurrencyManager &fg_ConcurrencyManager();
	TCActor<CConcurrentActor> const &fg_ConcurrentActor();
	TCActor<CConcurrentActor> const &fg_ConcurrentActorLowPrio();
	TCActor<CActor> fg_CurrentActor();

	TCActor<CTimerActor> fg_TimerActor();
}

template <>
struct NMib::NStorage::TCHasIntrusiveRefcount<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{ 
};

template <typename t_CActor>
struct NMib::NStorage::TCHasIntrusiveRefcount<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CActor>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{ 
};

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
