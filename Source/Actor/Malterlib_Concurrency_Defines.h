// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Function/Function>

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
{
	/// Check if member function is callable with the specified parameters when called through the actor interface.
	/// Implies that all arguments will be converted to rvalue references when called
	template <typename t_CMemberFunction, typename t_CActor, typename... tp_CCallParams>
	concept cActorCallableWith = NTraits::cIsCallableWith
		<
			typename NTraits::TCAddPointer<typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<t_CMemberFunction>::CType>::CFunctionType>::CType
			, void (typename NTraits::TCAddRValueReference<typename NTraits::TCRemoveReferenceAndQualifiers<tp_CCallParams>::CType>::CType...)
		>
		&& NTraits::cIsBaseOfOrSame<t_CActor, typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<t_CMemberFunction>::CType>::CClass>
	;

	/// Check if functor is callable with the specified parameters when called through the actor interface.
	/// Implies that all arguments will be converted to rvalue references when called
	template <auto t_pMemberFunction, typename t_CActor, typename... tp_CCallParams>
	concept cActorCallableWithFunctor = cActorCallableWith<decltype(t_pMemberFunction), t_CActor, tp_CCallParams...>;

	/// Inherit from this class to allow unsafe this pointers in async calls
	struct CAllowUnsafeThis
	{
	};

	struct CActor;
	class CDefaultActorHolder;
	template <typename t_CActor>
	class TCActorInternal;

	class CConcurrencyManager;
	class CSeparateThreadActorHolder;
	struct CSeparateThreadActor;
	class CDelegatedActorHolder;
	struct CCurrentActorScope;
	struct CCurrentlyProcessingActorScope;

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

	struct CInternalActorAllocator : public NMemory::CAllocator_Heap
	{
		enum
		{
			mc_bIsDefault = false
		};
	};

	namespace NPrivate
	{
		template <typename t_CActor, bool t_bIsComplete = NTraits::TCIsComplete<t_CActor>::mc_Value>
		struct TCIsActorAlwaysAliveImpl
		{
			static constexpr bool mc_Value = false;
			static constexpr bool mc_bImpl = false;
		};

		template <typename t_CActor>
		struct TCIsActorAlwaysAliveImpl<t_CActor, true>
		{
			static constexpr bool mc_Value = t_CActor::mc_bIsAlwaysAlive;
			static constexpr bool mc_bImpl = t_CActor::mc_bIsAlwaysAliveImpl;
		};
	}

	template <typename t_CActor>
	struct TCIsActorAlwaysAlive : public NPrivate::TCIsActorAlwaysAliveImpl<t_CActor>
	{
	};

#define DMibDefineActorType(d_Type, d_AlwaysAlive) \
	template <>\
	struct ::NMib::NConcurrency::TCIsActorAlwaysAlive<d_Type>\
	{\
		static constexpr bool mc_Value = d_AlwaysAlive;\
		static constexpr bool mc_bImpl = d_AlwaysAlive;\
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

	template <typename tf_CActor>
	decltype(auto) fg_GetActorInternal(tf_CActor &&_Actor);

	struct CActorCommon
	{
		TCFuture<void> f_Destroy() &;
		TCFuture<void> f_Destroy() &&;
	};

	struct CDispatchHelperWithActor;

	template <typename t_CActor>
	class TCActor /// \brief Contain an instance of a CActor
		: public TCChooseType<TCIsActorAlwaysAlive<t_CActor>::mc_bImpl, CEmpty, CActorCommon>::CType
	{
		friend class CConcurrencyManager;
		template <typename t_CActor2>
		friend class TCActor;
		template <typename t_CActor2>
		friend class TCWeakActor;
		template <typename t_CActor2>
		friend class TCActorInternal;

		friend class CActorHolder;
		friend class CDelegatedActorHolder;
		friend struct CActorCommon;

		template <typename tf_CActor>
		friend decltype(auto) fg_GetActorInternal(tf_CActor &&_Actor);

	public:
		using CActorInternal = TCActorInternal<t_CActor>;
		using CContainedActor = t_CActor;

		static constexpr bool mc_bIsWeak = false;
		static constexpr bool mc_bIsAlwaysAlive = TCIsActorAlwaysAlive<t_CActor>::mc_bImpl;
		using CNonWeak = TCActor;
		using CWeak = TCWeakActor<t_CActor>;

	private:
		using CPointerType = typename TCChooseType
			<
				TCIsActorAlwaysAlive<t_CActor>::mc_bImpl
				, CActorInternal *
				, TCActorHolderSharedPointer<CActorInternal>
			>
			::CType
		;
		union
		{
			CPointerType m_pInternalActor = nullptr;
			uint8 m_Dummy[sizeof(TCActorHolderWeakPointer<CActorInternal>)];
		};

		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams, mint... tfp_Indidies>
		void fp_Construct(TCConstruct<void, TCConstruct<tf_CType, tfp_CParams...>, tfp_CHolderParams...> &&_Construct, NMeta::TCIndices<tfp_Indidies...> const&);

	public:
		TCActor();
		~TCActor();
		template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
		TCActor(TCConstruct<tf_CType, tfp_CParams...> &&_Construct, tfp_CHolderParams && ...p_HolderParams);
		TCActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor);
		TCActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor);
		TCActor(CNullPtr);
		TCActor(TCActor const &_Other);
		TCActor(TCActor &&_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCActor(TCActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCActor(TCActor<tf_CActor> &&_Other);
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
		DMibSuppressUndefinedSanitizer TCActor &operator =(TCActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCActor &operator =(TCActor<tf_CActor> &&_Other);
		CActorInternal *operator ->() const;
		CActorInternal *f_Get() const;
		void f_Clear();
		bool f_IsEmpty() const;
		TCWeakActor<t_CActor> f_Weak() const;

		CDispatchHelperWithActor f_Dispatch() const;

		template <typename ...tfp_CObject>
		TCFuture<void> f_DestroyObjectsOn(tfp_CObject && ...p_Objects);

		template <typename tf_CActor>
		COrdering_Strong operator <=> (TCActor<tf_CActor> const& _Right) const;
		template <typename tf_CActor>
		COrdering_Strong operator <=> (TCWeakActor<tf_CActor> const& _Right) const;

		template <typename tf_CActor>
		bool operator == (TCActor<tf_CActor> const& _Right) const;
		template <typename tf_CActor>
		bool operator == (TCWeakActor<tf_CActor> const& _Right) const;

		inline_small explicit operator bool() const;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const &
			requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) &&
			requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template
		<
			auto tf_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
			, typename... tfp_CCallParams
		>
		auto f_InternalCallActor(tfp_CCallParams &&... p_CallParams) const &
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template
		<
			auto tf_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
			, typename... tfp_CCallParams
		>
		auto f_InternalCallActor(tfp_CCallParams &&... p_CallParams) &&
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallDirect(tfp_CCallParams &&... p_CallParams) const &
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;
		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallDirect(tfp_CCallParams &&... p_CallParams) &&
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValue(tfp_CCallParams &&... p_CallParams) const &
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;
		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValue(tfp_CCallParams &&... p_CallParams) &&
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValueDirect(tfp_CCallParams &&... p_CallParams) const &
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;
		template <auto tf_pMemberFunction, typename... tfp_CCallParams>
		auto f_CallByValueDirect(tfp_CCallParams &&... p_CallParams) &&
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <typename tf_FResult>
		auto operator / (tf_FResult &&_fResult) const
			-> TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
		;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		DMibSuppressUndefinedSanitizer inline_always TCActorInternal<CActor> *f_GetRealActor() const;
	};

	template <typename tf_CActor, typename tf_CActorSource>
	TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> const &_Actor);

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<tf_CActor> &&_Actor, tfp_CParams && ...p_Params)
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CParams...>
	;

#	define f_CallActor(d_PointerToMemberFunction) f_InternalCallActor<DMibPointerToMemberFunctionForHash(d_PointerToMemberFunction)>

	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);

	template <typename tf_CActor>
	TCWeakActor<tf_CActor> fg_ThisActorWeak(tf_CActor const *_pActor);

	struct CConcurrentActor;
	struct CConcurrentActorLowPrio;
	struct CTimerActor;
	struct CThisConcurrentActor;
	struct CThisConcurrentActorLowPrio;
	struct COtherConcurrentActor;
	struct COtherConcurrentActorLowPrio;
	struct CDirectResultActor;

	template <typename t_CType, typename t_CLock>
	struct TCLockActor;

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

	TCFuture<void> fg_DestroySubscription(CActorSubscription &_Subscription);
	CConcurrencyManager &fg_CurrentConcurrencyManager();
	void fg_SetConcurrencyManagerDefaultExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority);
	CConcurrencyManager &fg_ConcurrencyManager();
	TCActor<CConcurrentActor> const &fg_ConcurrentActor();
	TCActor<CConcurrentActor> const &fg_ConcurrentActorLowPrio();
	TCActor<CActor> fg_CurrentActor();
	TCWeakActor<CActor> fg_CurrentActorWeak();

	TCActor<CTimerActor> fg_TimerActor();
}

DMibDefineActorType(NMib::NConcurrency::NPrivate::CDirectResultActor, true);
DMibDefineActorType(NMib::NConcurrency::CConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::CConcurrentActorLowPrio, true);
DMibDefineActorType(NMib::NConcurrency::CThisConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::CThisConcurrentActorLowPrio, true);
DMibDefineActorType(NMib::NConcurrency::COtherConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::COtherConcurrentActorLowPrio, true);

template <>
struct NMib::NStorage::TCHasIntrusiveRefCount<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::CCanDestroyTracker> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{
};

template <typename t_CActor>
struct NMib::NStorage::TCHasIntrusiveRefCount<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CActor>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::TCActorInternal<t_CActor>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
