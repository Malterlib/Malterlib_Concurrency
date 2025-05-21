// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Function/Function>
#include <Mib/Function/Traits>

#include "Malterlib_Concurrency_Configuration.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCAsyncGenerator;

	template <typename t_CReturnValue, ECoroutineFlag t_Flags>
	struct TCFutureWithFlags;

	class CActorHolder;

	constexpr static mint gc_FutureOnResultStorage = sizeof(void *) * 3;

//#define DMibCheckOnResultSizes
#ifdef DMibCheckOnResultSizes

	template <typename t_CReturnValue>
	using TCFutureOnResult = NFunction::TCFunction
		<
			void (NFunction::CThisTag &, TCAsyncResult<t_CReturnValue> &&_AsyncResult)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				false
				, gc_FutureOnResultStorage
				, sizeof(void *)
				, true
			>
		>
	;

	template <typename t_CReturnValue>
	using TCFutureOnResultOverSized = NFunction::TCFunctionFastCall
		<
			void (NFunction::CThisTag &, TCAsyncResult<t_CReturnValue> &&_AsyncResult)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, 0
				, sizeof(void *)
				, true
			>
		>
	;

#else

	template <typename t_CReturnValue>
	using TCFutureOnResult = NFunction::TCFunction
		<
			void (NFunction::CThisTag &, TCAsyncResult<t_CReturnValue> &&_AsyncResult)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, gc_FutureOnResultStorage
				, sizeof(void *)
				, false
			>
		>
	;

	template <typename t_CReturnValue>
	using TCFutureOnResultOverSized = TCFutureOnResult<t_CReturnValue>;

#endif

	namespace NPrivate
	{
		template <typename t_CTypeList>
		struct TCOneIsReference;

		template <typename ...tp_CParams>
		struct TCOneIsReference<NMeta::TCTypeList<tp_CParams...>>
		{
			static constexpr bool mc_Value =
				(
					... || NTraits::cIsReference<tp_CParams>
				)
			;
		};

		template <typename t_CType>
		struct TCIsFuture
		{
			using CType = t_CType;
			static constexpr bool mc_Value = false;
		};

		template <typename t_CType>
		struct TCIsFuture<TCFuture<t_CType>>
		{
			using CType = t_CType;
			static constexpr bool mc_Value = true;
		};

		template <typename t_CType, ECoroutineFlag t_Flags>
		struct TCIsFuture<TCFutureWithFlags<t_CType, t_Flags>>
		{
			using CType = t_CType;
			static constexpr bool mc_Value = true;
		};

		template <typename t_CType>
		struct TCIsUnsafeFuture
		{
			using CType = t_CType;
			static constexpr bool mc_Value = false;
		};

		template <typename t_CType>
		struct TCIsUnsafeFuture<TCUnsafeFuture<t_CType>>
		{
			using CType = t_CType;
			static constexpr bool mc_Value = true;
		};

		template <typename t_CType>
		struct TCIsAsyncGenerator
		{
			using CType = t_CType;
			static constexpr bool mc_Value = false;
		};

		template <typename t_CType>
		struct TCIsAsyncGenerator<TCAsyncGenerator<t_CType>>
		{
			using CType = t_CType;
			static constexpr bool mc_Value = true;
		};
	}

	/// Check if member function is callable with the specified parameters when called through the actor interface.
	/// Implies that all arguments will be converted to rvalue references when called
	template <typename t_CMemberFunction, typename t_CActor, typename... tp_CCallParams>
	concept cActorCallableWith =
		(
			NTraits::cIsCallableWith
			<
				NTraits::TCAddPointer<typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<t_CMemberFunction>>::CFunctionType>
				, void (NTraits::TCRemoveQualifiersAndAddRValueReference<tp_CCallParams>...)
			>
			&& NTraits::cIsBaseOfOrSame<t_CActor, typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<t_CMemberFunction>>::CClass>
		)
		|| NTraits::cIsCallableWith
		<
			t_CMemberFunction
			, void (NTraits::TCRemoveQualifiersAndAddRValueReference<tp_CCallParams>...)
		>
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

	template <typename t_CActor = CActor>
	class TCActor;

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CReturnValue>
	struct TCPromise;

	struct CInternalActorAllocator : public NMemory::CAllocator_Heap
	{
		enum
		{
			mc_bIsDefault = false
		};
	};

	enum class EVirtualCall : uint8
	{
		mc_Virtual = 0
		, mc_NotVirtual
		, mc_Dynamic
		, mc_CannotBeDetected
	};

	namespace NPrivate
	{
		template <typename t_CActor, bool t_bIsComplete = NTraits::cIsComplete<t_CActor>>
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

		template <typename t_CType>
		struct TCRemoveFuture
		{
			using CType = t_CType;
		};

		template <typename t_CType>
		struct TCRemoveFuture<TCFuture<t_CType>>
		{
			using CType = t_CType;
		};

		template <typename t_CType, ECoroutineFlag t_Flags>
		struct TCRemoveFuture<TCFutureWithFlags<t_CType, t_Flags>>
		{
			using CType = t_CType;
		};

		template <typename t_CFunction, bool t_bIsMemberFunctionPointer = NTraits::cIsMemberFunctionPointer<t_CFunction>>
		struct TCGetMemberOrNonMemberFunctionPointerTraits;

		template <typename t_CMemberFunction>
		using TCFutureReturn = TCFuture
			<
				typename NPrivate::TCRemoveFuture<typename TCGetMemberOrNonMemberFunctionPointerTraits<NTraits::TCRemoveReference<t_CMemberFunction>>::CType::CReturn>::CType
			>
		;

#ifdef DMibCanDetectVirtualMemberFunctions_Constexpr
		template <EVirtualCall t_Detected, EVirtualCall t_Specified>
		consteval EVirtualCall fg_CheckVirtualCallDetection()
		{
			static_assert(t_Specified == EVirtualCall::mc_Dynamic || t_Detected == t_Specified);

			return t_Detected;
		}
#endif

		template <auto t_pMemberFunctionPointer, EVirtualCall t_ForceDetection>
		constexpr static EVirtualCall gc_VirtualCallDetection =
#ifdef DMibCanDetectVirtualMemberFunctions_Constexpr
			fg_CheckVirtualCallDetection
				<
					NTraits::cIsVirtualMemberFunction<t_pMemberFunctionPointer> ? EVirtualCall::mc_Virtual : EVirtualCall::mc_NotVirtual
					, t_ForceDetection
				>
			(
			)
#else
	#ifdef DMibCanDetectVirtualMemberFunctions_Dynamic
			t_ForceDetection == EVirtualCall::mc_Dynamic ? EVirtualCall::mc_Dynamic : t_ForceDetection
	#else
			t_ForceDetection == EVirtualCall::mc_Dynamic ? EVirtualCall::mc_CannotBeDetected : t_ForceDetection
	#endif
#endif
		;

		constexpr static EVirtualCall gc_VirtualCallDetectionDynamic =
#ifdef DMibCanDetectVirtualMemberFunctions_Dynamic
			EVirtualCall::mc_Dynamic
#else
			EVirtualCall::mc_CannotBeDetected
#endif
		;
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
		, EPriority_NormalHighCPU
		, EPriority_Normal
		, EPriority_Max
	};

	template <typename t_CActor>
	class TCWeakActor;

	template <typename tf_CActor>
	decltype(auto) fg_GetActorInternal(tf_CActor &&_Actor);

	struct CActorCommon
	{
		TCFuture<void> f_Destroy() &&;
		
	private:
		friend class CConcurrencyManager;
		
		TCFuture<void> fp_DestroyUnused() &&;
	};

	struct CDispatchHelperWithActor;

	enum class EActorCallType : uint8
	{
		mc_Normal
		, mc_Direct
	};

	struct CDiscardResult
	{
	};

	extern CDiscardResult g_DiscardResult;

	template <typename t_FOnResult>
	struct TCDirectResultFunctor
	{
		t_FOnResult &&m_fOnResult;
	};

	struct CDirectResult
	{
		template <typename tf_FOnResult>
		TCDirectResultFunctor<tf_FOnResult> operator / (tf_FOnResult &&_fOnResult)
		{
			return {.m_fOnResult = fg_Move(_fOnResult)};
		}
	};

	extern CDirectResult g_DirectResult;

	struct CBindActorOptions
	{
		constexpr CBindActorOptions() = default;

		constexpr CBindActorOptions(EVirtualCall _VirtualCall)
			: m_VirtualCall(_VirtualCall)
		{
		}

		constexpr CBindActorOptions(EActorCallType _CallType)
			: m_CallType(_CallType)
		{
		}

		constexpr CBindActorOptions(EVirtualCall _VirtualCall, EActorCallType _CallType)
			: m_VirtualCall (_VirtualCall)
			, m_CallType(_CallType)
		{
		}

		constexpr CBindActorOptions(EActorCallType _CallType, EVirtualCall _VirtualCall)
			: m_VirtualCall (_VirtualCall)
			, m_CallType(_CallType)
		{
		}


		constexpr static CBindActorOptions fs_Default()
		{
			return {};
		}

		EVirtualCall m_VirtualCall = EVirtualCall::mc_Dynamic;
		EActorCallType m_CallType = EActorCallType::mc_Normal;
	};

	template <typename t_CType>
	struct TCFutureVector;

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCBoundActorCall;

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform = CVoidTag>
	struct TCBoundActorCallAwaiter;

	template <typename t_CActor>
	class TCActor /// \brief Contain an instance of a CActor
		: public TCConditional<TCIsActorAlwaysAlive<t_CActor>::mc_bImpl, CEmpty, CActorCommon>
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
		using CPointerType = TCConditional
			<
				TCIsActorAlwaysAlive<t_CActor>::mc_bImpl
				, CActorInternal *
				, TCActorHolderSharedPointer<CActorInternal>
			>
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

		TCActorHolderSharedPointer<CActorInternal> &f_Unsafe_AccessInternal();

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
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<tf_CMemberFunction>
				, TCActor<t_CActor> const &
				, tf_CMemberFunction
				, CBindActorOptions::fs_Default()
				, false
				, tfp_CCallParams...
			>
			requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) &&
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<tf_CMemberFunction>
				, TCActor<t_CActor> &&
				, tf_CMemberFunction
				, CBindActorOptions::fs_Default()
				, false
				, tfp_CCallParams...
			>
			requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template
		<
			auto tf_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
			, EVirtualCall tf_VirtualCall = EVirtualCall::mc_Dynamic
			, typename... tfp_CCallParams
		>
		auto f_InternalCallActor(tfp_CCallParams &&... p_CallParams) const &
			-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template
		<
			auto tf_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
			, EVirtualCall tf_VirtualCall = EVirtualCall::mc_Dynamic
			, typename... tfp_CCallParams
		>
		auto f_InternalCallActor(tfp_CCallParams &&... p_CallParams) &&
			-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
			requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_Bind(tfp_CCallParams &&... p_CallParams) const &
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor> const &
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_Bind(tfp_CCallParams &&... p_CallParams) &&
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor> &&
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_BindByValue(tfp_CCallParams &&... p_CallParams) const &
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_BindByValue(tfp_CCallParams &&... p_CallParams) &&
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <typename tf_FResult>
		auto operator / (tf_FResult &&_fResult) const
			-> TCActorResultCall<TCActor, NTraits::TCRemoveReference<tf_FResult>>
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
		, EVirtualCall tf_VirtualCall
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<tf_CActor> &&_Actor, tfp_CParams && ...p_Params)
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CParams...>
	;

#	define f_CallActor(d_PointerToMemberFunction, ...) f_InternalCallActor<DMibPointerToMemberFunctionForHash(d_PointerToMemberFunction)>
#	define f_CallActorNotVirtual(d_PointerToMemberFunction, ...) f_InternalCallActor<DMibPointerToMemberFunctionForHash(d_PointerToMemberFunction), EVirtualCall::mc_NotVirtual>

	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);

	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(TCActorInternal<tf_CActor> *_pActor);

	inline_always TCActor<CActor> fg_ThisActor(CActorHolder *_pActor);

	template <typename tf_CActor>
	TCWeakActor<tf_CActor> fg_ThisActorWeak(tf_CActor const *_pActor);

	struct CConcurrentActor;
	struct CConcurrentActorLowPrio;
	struct CConcurrentActorHighCPU;
	struct CTimerActor;
	struct CThisConcurrentActor;
	struct CThisConcurrentActorLowPrio;
	struct CThisConcurrentActorHighCPU;
	struct COtherConcurrentActor;
	struct COtherConcurrentActorLowPrio;
	struct COtherConcurrentActorHighCPU;

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

	using CActorSubscription = NStorage::TCUniquePointer<CActorSubscriptionReference>;

	struct CBlockingActorStorage;
	struct CBlockingActor;

	struct CBlockingActorCheckout
	{
		CBlockingActorCheckout(CConcurrencyManager *_pConcurrencyManager, CBlockingActorStorage *_pBlockingActorStorage);
		CBlockingActorCheckout(CBlockingActorCheckout const &) = delete;
		CBlockingActorCheckout(CBlockingActorCheckout &&_Other);
		~CBlockingActorCheckout();

		CBlockingActorCheckout &operator = (CBlockingActorCheckout &&_Other);
		
		void f_Clear();
		bool f_IsValid() const;

		TCActor<CBlockingActor> const &f_Actor() const;
		TCFutureOnResultOverSized<void> f_MoveResultHandler(NStr::CStr const &_Category, NStr::CStr const &_Error);

	private:
		static void fsp_LogError(NStr::CStr const &_Category, NStr::CStr const &_Error, CAsyncResult const &_Result);

		CConcurrencyManager *mp_pConcurrencyManager;
		CBlockingActorStorage *mp_pBlockingActorStorage;
	};

	struct CRoundRobinBlockingActors
	{
		CRoundRobinBlockingActors(mint _Capacity);

		CBlockingActorCheckout &operator *();

		NContainer::TCVector<CBlockingActorCheckout> m_Checkouts;
		mint m_Capacity = 1;
		mint m_iCheckout = 0;
	};

	TCFuture<void> fg_DestroySubscription(CActorSubscription &_Subscription);
	CConcurrencyManager &fg_CurrentConcurrencyManager();
	void fg_SetConcurrencyManagerDefaultExecutionPriority(EPriority _Priority, EExecutionPriority _ExecutionPriority);
	CConcurrencyManager &fg_ConcurrencyManager();
	TCActor<CConcurrentActor> const &fg_ConcurrentActor();
	TCActor<CConcurrentActor> const &fg_ConcurrentActorLowPrio();
	TCActor<CConcurrentActor> const &fg_ConcurrentActorHighCPU();
	CBlockingActorCheckout fg_BlockingActor();
	mark_nodebug TCActor<CActor> fg_CurrentActor();
	mark_nodebug TCActor<CActor> fg_CurrentActor(CConcurrencyThreadLocal &_ThreadLocal);
	mark_nodebug TCWeakActor<CActor> fg_CurrentActorWeak();
	mark_nodebug TCWeakActor<CActor> fg_CurrentActorWeak(CConcurrencyThreadLocal &_ThreadLocal);

	TCActor<CTimerActor> fg_TimerActor();
}

DMibDefineActorType(NMib::NConcurrency::CConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::CConcurrentActorLowPrio, true);
DMibDefineActorType(NMib::NConcurrency::CConcurrentActorHighCPU, true);
DMibDefineActorType(NMib::NConcurrency::CThisConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::CThisConcurrentActorLowPrio, true);
DMibDefineActorType(NMib::NConcurrency::CThisConcurrentActorHighCPU, true);
DMibDefineActorType(NMib::NConcurrency::COtherConcurrentActor, true);
DMibDefineActorType(NMib::NConcurrency::COtherConcurrentActorLowPrio, true);
DMibDefineActorType(NMib::NConcurrency::COtherConcurrentActorHighCPU, true);

template <>
struct NMib::NStorage::TCHasIntrusiveRefCountOverride<NMib::NConcurrency::CCanDestroyTracker>
{
	constexpr static bool mc_bValue = true;
};

template <>
struct NMib::NTraits::TCHasVirtualDestructorOverride<NMib::NConcurrency::CCanDestroyTracker>
{
	constexpr static bool mc_Value = false;
};

template <typename t_CActor>
struct NMib::NStorage::TCHasIntrusiveRefCountOverride<NMib::NConcurrency::TCActorInternal<t_CActor>>
{
	constexpr static bool mc_bValue = true;
};

template <typename t_CActor>
struct NMib::NTraits::TCHasVirtualDestructorOverride<NMib::NConcurrency::TCActorInternal<t_CActor>>
{
	constexpr static bool mc_Value = true;
};

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
