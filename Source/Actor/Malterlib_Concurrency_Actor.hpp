// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	TCActor<t_CActor>::TCActor()
	{
	}

	template <typename t_CActor>
	TCActor<t_CActor>::~TCActor()
	{
		if constexpr (!mc_bIsAlwaysAlive)
			m_pInternalActor.~CPointerType();
	}


	template <typename t_CActor>
	template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
	TCActor<t_CActor>::TCActor(TCConstruct<tf_CType, tfp_CParams...> &&_Construct, tfp_CHolderParams && ...p_HolderParams)
		: TCActor(fg_CurrentConcurrencyManager().f_ConstructActor(fg_MakeConcreteConstruct<t_CActor>(fg_Move(_Construct)), fg_Forward<tfp_CHolderParams>(p_HolderParams)...))
	{
	}

	template <typename t_CActor>
	template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams, mint... tfp_Indidies>
	void TCActor<t_CActor>::fp_Construct(TCConstruct<void, TCConstruct<tf_CType, tfp_CParams...>, tfp_CHolderParams...> &&_Construct, NMeta::TCIndices<tfp_Indidies...> const&)
	{
		*this = fg_CurrentConcurrencyManager().f_ConstructActor
			(
				fg_MakeConcreteConstruct<t_CActor>(fg_Move(fg_Get<0>(_Construct.m_Params)))
				, fg_Forward<tfp_CHolderParams>(fg_Get<tfp_Indidies>(_Construct.m_Params))...
			)
		;
	}

	template <typename t_CActor>
	template <typename tf_CType, typename ...tfp_CParams, typename ...tfp_CHolderParams>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCConstruct<void, TCConstruct<tf_CType, tfp_CParams...>, tfp_CHolderParams...> &&_Construct)
	{
		fp_Construct(fg_Move(_Construct), NMeta::TCConsecutiveIndices<sizeof...(tfp_CHolderParams) + 1, 1>());
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CType, typename ...tfp_CParams>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCConstruct<tf_CType, tfp_CParams...> &&_Construct)
	{
		*this = fg_CurrentConcurrencyManager().f_ConstructActor(fg_MakeConcreteConstruct<t_CActor>(fg_Move(_Construct)));
		return *this;
	}

	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(CNullPtr)
		: m_pInternalActor(nullptr)
	{
	}

	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor)
		: m_pInternalActor
		(
			[&]
			{
				if constexpr (mc_bIsAlwaysAlive)
					return _pActor.f_Get();
				else
					return _pActor;
			}
			()
		)
	{
	}

	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
		: m_pInternalActor
		(
			[&]
			{
				if constexpr (mc_bIsAlwaysAlive)
					return _pActor.f_Get();
				else
					return fg_Move(_pActor);
			}
			()
		)
	{
	}

	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActor const &_Other)
		: m_pInternalActor(_Other.m_pInternalActor)
	{
	}

	template <typename t_CActor>
	CDispatchHelperWithActor TCActor<t_CActor>::f_Dispatch() const
	{
		return CDispatchHelperWithActor(*this);
	}

	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActor &&_Other)
		: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
	{
		if constexpr (mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
	}

	template <typename tf_CActor>
	decltype(auto) fg_GetActorInternal(tf_CActor &&_Actor)
	{
		return _Actor.m_pInternalActor;
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderSharedPointer<TCActorInternal<tf_CActorSource>> const &_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> const &>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderSharedPointer<TCActorInternal<tf_CActorSource>> &&_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> &&>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorInternal<tf_CActorSource> *_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder);
		else
			return TCActorHolderSharedPointer<TCActorInternal<tf_CActor>>(fg_Explicit(reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder)));
	}

	template <typename tf_CActor, typename tf_CActorSource>
	TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> const &_Actor)
	{
		return TCActor<tf_CActor>(fg_StaticCast<tf_CActor>(fg_GetActorInternal(_Actor)));
	}

	template <typename tf_CActor, typename tf_CActorSource>
	TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> &&_Actor)
	{
		return TCActor<tf_CActor>(fg_StaticCast<tf_CActor>(fg_GetActorInternal(fg_Move(_Actor))));
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor>::TCActor(TCActor<tf_CActor> const &_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(_Other.m_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor>::TCActor(TCActor<tf_CActor> &&_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(_Other.m_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
	}

	template <typename t_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActorHolderSharedPointer<CActorInternal> const &_pActor)
	{
		m_pInternalActor = _pActor;
		return *this;
	}

	template <typename t_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
	{
		m_pInternalActor = fg_Move(_pActor);
		return *this;
	}

	template <typename t_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor const &_Other)
	{
		m_pInternalActor = _Other.m_pInternalActor;
		return *this;
	}

	template <typename t_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor &&_Other)
	{
		m_pInternalActor = fg_Move(_Other.m_pInternalActor);

		if constexpr (mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(CNullPtr)
	{
		m_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor<tf_CActor> const &_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_StaticCast<t_CActor>(_Other.m_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");

		m_pInternalActor = fg_StaticCast<t_CActor>(fg_Move(_Other.m_pInternalActor));

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	inline_always typename TCActor<t_CActor>::CActorInternal *TCActor<t_CActor>::operator ->() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return m_pInternalActor;
		else
			return m_pInternalActor.f_Get();
	}

	template <typename t_CActor>
	inline_always typename TCActor<t_CActor>::CActorInternal *TCActor<t_CActor>::f_Get() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return m_pInternalActor;
		else
			return m_pInternalActor.f_Get();
	}

	template <typename t_CActor>
	auto TCActor<t_CActor>::f_Unsafe_AccessInternal() -> TCActorHolderSharedPointer<CActorInternal> &
	{
		static_assert(!mc_bIsAlwaysAlive);
		return m_pInternalActor;
	}

	template <typename t_CActor>
	inline_always TCActorInternal<CActor> *TCActor<t_CActor>::f_GetRealActor() const
	{
		return m_pInternalActor->fp_GetRealActor((TCActorInternal<CActor> *)this->f_Get());
	}

	inline_always TCActorInternal<CActor> *CActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return static_cast<TCActorInternal<CActor> *>(_pActorInternal);
	}

	inline_always TCActorInternal<CActor> *CThisConcurrentActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal).f_Get();
	}

	inline_always TCActorInternal<CActor> *CThisConcurrentActorLowPrio::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Low).f_Get();
	}

	inline_always TCActorInternal<CActor> *CThisConcurrentActorHighCPU::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_NormalHighCPU).f_Get();
	}

	inline_always TCActorInternal<CActor> *COtherConcurrentActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForOtherThread(EPriority_Normal).f_Get();
	}

	inline_always TCActorInternal<CActor> *COtherConcurrentActorLowPrio::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForOtherThread(EPriority_Low).f_Get();
	}

	inline_always TCActorInternal<CActor> *COtherConcurrentActorHighCPU::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForOtherThread(EPriority_NormalHighCPU).f_Get();
	}

	inline_always TCActorInternal<CActor> *CDynamicConcurrentActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActor().f_Get();
	}

	inline_always TCActorInternal<CActor> *CDynamicConcurrentActorLowPrio::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorLowPrio().f_Get();
	}

	inline_always TCActorInternal<CActor> *CDynamicConcurrentActorHighCPU::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorHighCPU().f_Get();
	}

	template <typename t_CActor>
	void TCActor<t_CActor>::f_Clear()
	{
		if constexpr (mc_bIsAlwaysAlive)
			m_pInternalActor = nullptr;
		else
			m_pInternalActor.f_Clear();
	}

	template <typename t_CActor>
	bool TCActor<t_CActor>::f_IsEmpty() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return !m_pInternalActor;
		else
			return m_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> TCActor<t_CActor>::f_Weak() const
	{
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Strong TCActor<t_CActor>::operator <=> (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			<=> (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Strong TCActor<t_CActor>::operator <=> (TCWeakActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			<=> (TCActorHolderWeakPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCActor<t_CActor>::operator == (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			== (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCActor<t_CActor>::operator == (TCWeakActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			== (TCActorHolderWeakPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}


	template <typename t_CActor>
	TCActor<t_CActor>::operator bool() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return !!m_pInternalActor;
		else
			return !m_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	template <typename tf_FResult>
	auto TCActor<t_CActor>::operator / (tf_FResult &&_fResult) const
		-> TCActorResultCall<TCActor, NTraits::TCRemoveReference<tf_FResult>>
	{
		return TCActorResultCall<TCActor, NTraits::TCRemoveReference<tf_FResult>>
			(
				*this
				, fg_Forward<tf_FResult>(_fResult)
			)
		;
	}

	namespace NPrivate
	{
		template <typename t_CFunction, bool t_bIsMemberFunctionPointer>
		struct TCGetMemberOrNonMemberFunctionPointerTraits
		{
			using CType = NTraits::TCMemberFunctionPointerTraits<t_CFunction>;
		};

		template <typename t_CFunction>
		struct TCGetMemberOrNonMemberFunctionPointerTraits<t_CFunction, false>
		{
			using CType = NTraits::TCFunctionTraits<NTraits::TCRemovePointer<t_CFunction>>;
		};

		template
		<
			bool tf_bDirectCall
			, EVirtualCall tf_VirtualCall
			, typename tf_CActor
			, typename tf_CFunctor
			, typename tf_CPromiseParam
			, typename ...tfp_CParams
		>
		auto fg_CallActorFuture
			(
				tf_CActor &&_Actor
				, tf_CFunctor const &_pFunctionPointer
				, tf_CPromiseParam &&_PromiseParam
				, tfp_CParams && ...p_Params
			)
			-> typename TCCallActorGetReturnType<typename TCGetMemberOrNonMemberFunctionPointerTraits<tf_CFunctor>::CType::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
		{
			constexpr static bool c_bIsMemberPointer = NTraits::cIsMemberFunctionPointer<tf_CFunctor>;

			using CMemberPointerTraits = NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>;
			using CFunctionType = TCConditional<c_bIsMemberPointer, typename CMemberPointerTraits::CFunctionType, NTraits::TCRemovePointer<tf_CFunctor>>;

			static_assert(c_bIsMemberPointer || NTraits::cIsFunction<CFunctionType>);

			using CFunctionTraits = NTraits::TCFunctionTraits<CFunctionType>;
			using CReturn = typename NPrivate::TCGetReturnType<typename CFunctionTraits::CReturn>::CType;
			using CFullActor = NTraits::TCRemoveReferenceAndQualifiers<tf_CActor>;
			using CContainedActor = typename CFullActor::CContainedActor;
			using CPromiseParam = NTraits::TCDecay<tf_CPromiseParam>;

			constexpr static bool c_bIsVoid = TCCallActorGetReturnType
				<
					typename CFunctionTraits::CReturn
					, CPromiseParam
				>::mc_bIsVoid
			;
			constexpr static bool c_bIsDiscardResult = NTraits::cIsSame<CPromiseParam, CPromiseConstructDiscardResult>;
			constexpr static bool c_bIsFunctor = !c_bIsDiscardResult && !NTraits::cIsSame<CPromiseParam, CPromiseConstructNoConsume>;

			[[maybe_unused]] TCConditional<CFullActor::mc_bIsWeak, typename CFullActor::CNonWeak, void *> Actor;
			TCActorInternal<CActor> *pRealActor;

			if constexpr (CFullActor::mc_bIsWeak)
			{
				Actor = _Actor.f_Lock();
				if (!Actor)
				{
					if constexpr (c_bIsVoid)
					{
						if constexpr (c_bIsDiscardResult)
							return;
						else
						{
							static_assert(c_bIsFunctor);

							TCAsyncResult<CReturn> Result;
							Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted"));
							_PromiseParam(fg_Move(Result));

							return;
						}
					}
					else
						return DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted");
				}

				// If you get this, use f_CallActor instead of calling directly
				DMibFastCheck
					(
						(NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
						|| !Actor->f_GetDistributedActorData()
						|| Actor->f_GetDistributedActorData()->f_IsValidForCall()
					)
				;

				pRealActor = Actor.f_GetRealActor();
			}
			else
				pRealActor = _Actor.f_GetRealActor();

			auto Cleanup = g_OnScopeExit / [&]() inline_always_lambda
				{
					if constexpr (NTraits::cIsRValueReference<tf_CActor &&>)
						_Actor.f_Clear();
				}
			;

			using CBoundFunctor = TCActorCallWithParams<TCConditional<c_bIsMemberPointer, tf_CFunctor, CFunctionType>>;

			using CReportLocalFuture = TCReportLocalFuture<CContainedActor, CReturn, CBoundFunctor>;

			using CFunctorReturn = typename CFunctionTraits::CReturn;
			if constexpr
				(
					(tf_VirtualCall == EVirtualCall::mc_NotVirtual || !c_bIsMemberPointer)
					&& (CReportLocalFuture::mc_bIsFuture || CReportLocalFuture::mc_bIsAsyncGenerator)
#if defined(DMibSanitizerEnabled_UndefinedBehavior)
					&& false
#endif
				)
			{
				return fg_CallActorFutureDirectCoroutine<tf_bDirectCall, CFunctorReturn, CContainedActor>
					(
						pRealActor
						, _pFunctionPointer
						, fg_Forward<tf_CPromiseParam>(_PromiseParam)
						, typename CFunctionTraits::CParams()
						, fg_Forward<tfp_CParams>(p_Params)...
					)
				;
			}
			else
			{
#if defined(DMibCanDetectVirtualMemberFunctions_Dynamic) && !defined(DMibSanitizerEnabled_UndefinedBehavior)
				if constexpr (tf_VirtualCall == EVirtualCall::mc_Dynamic && (CReportLocalFuture::mc_bIsFuture || CReportLocalFuture::mc_bIsAsyncGenerator))
				{
					if (!NFunction::fg_IsVirtualCall(_pFunctionPointer))
					{
						return fg_CallActorFutureDirectCoroutine<tf_bDirectCall, CFunctorReturn, CContainedActor>
							(
								pRealActor
								, _pFunctionPointer
								, fg_Forward<tf_CPromiseParam>(_PromiseParam)
								, typename CFunctionTraits::CParams()
								, fg_Forward<tfp_CParams>(p_Params)...
							)
						;
					}
				}
#endif
				auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();

				TCPromise<CReturn> Promise{fg_Forward<tf_CPromiseParam>(_PromiseParam), NPrivate::CPromiseDataBaseRefCount::mc_FutureBit, ConcurrencyThreadLocal};
				auto Future = Promise.fp_AttachFutureIgnoreFutureGotten();

				if constexpr (tf_bDirectCall)
				{
					CReportLocalFuture
						{
							CBoundFunctor(_pFunctionPointer, fg_Forward<tfp_CParams>(p_Params)...)
							, (TCActorInternal<CContainedActor> *)pRealActor
							, fg_Move(Promise)
							, ConcurrencyThreadLocal.m_SystemThreadLocal
							, false
						}
						.f_CallNoDelete(ConcurrencyThreadLocal)
					;
				}
				else
				{
					auto pReport = fg_ConstructObject<CReportLocalFuture>
						(
							NMemory::CDefaultAllocator()
							, CBoundFunctor(_pFunctionPointer, fg_Forward<tfp_CParams>(p_Params)...)
							, (TCActorInternal<CContainedActor> *)pRealActor
							, fg_Move(Promise)
							, ConcurrencyThreadLocal.m_SystemThreadLocal
							, true
						)
					;
					pRealActor->f_QueueProcessEntry
						(
							CConcurrentRunQueueEntryHolder(pReport)
							, ConcurrencyThreadLocal
						)
					;
				}

				if constexpr (!c_bIsVoid)
					return Future;
			}
		}
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) &&
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
	{
#if DMibEnableSafeCheck > 0
		using CMemberPointerTraits = NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<tf_CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<tf_CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<tf_CMemberFunction>
				, TCActor<t_CActor> &&
				, tf_CMemberFunction
				, CBindActorOptions::fs_Default()
				, false
				, tfp_CCallParams...
			>
			{
				.mp_pMemberFunction = _pMemberFunction
				, .mp_Actor{fg_Move(*this)}
				, .mp_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const &
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
	{
#if DMibEnableSafeCheck > 0
		using CMemberPointerTraits = NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<tf_CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<tf_CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<tf_CMemberFunction>
				, TCActor<t_CActor> const &
				, tf_CMemberFunction
				, CBindActorOptions::fs_Default()
				, false
				, tfp_CCallParams...
			>
			{
				.mp_pMemberFunction = _pMemberFunction
				, .mp_Actor{*this}
				, .mp_Params{p_CallParams...}
			}
		;
	}

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename tf_CActor
		, typename... tfp_CCallParams
	>
	auto fg_CallActor(TCActor<tf_CActor> &&_Actor, tfp_CCallParams && ...p_CallParams)
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CCallParams...>
	{
#if DMibEnableSafeCheck > 0
		using CMemberFunction = decltype(tf_pMemberFunction);
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!_Actor.f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				(NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !_Actor->f_GetDistributedActorData()
				|| _Actor->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return NPrivate::fg_CallActorFuture<false, NPrivate::gc_VirtualCallDetection<tf_pMemberFunction, tf_VirtualCall>>
			(
				fg_Move(_Actor)
				, tf_pMemberFunction
				, CPromiseConstructNoConsume()
				, fg_Forward<tfp_CCallParams>(p_CallParams)...
			)
		;
	}

	template <typename t_CActor>
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename... tfp_CCallParams
	>
	auto TCActor<t_CActor>::f_InternalCallActor(tfp_CCallParams &&... p_CallParams) const &
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash), tf_VirtualCall>(fg_TempCopy(*this), fg_Forward<tfp_CCallParams>(p_CallParams)...);
	}

	template <typename t_CActor>
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename... tfp_CCallParams
	>
	auto TCActor<t_CActor>::f_InternalCallActor(tfp_CCallParams &&... p_CallParams) &&
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash), tf_VirtualCall>(fg_Move(*this), fg_Forward<tfp_CCallParams>(p_CallParams)...);
	}

	template <typename t_CActor>
	template <typename ...tfp_CObject>
	TCFuture<void> TCActor<t_CActor>::f_DestroyObjectsOn(tfp_CObject && ...p_Objects)
	{
		return fg_Dispatch
			(
				*this
				, [...p_Objects = fg_Forward<tfp_CObject>(p_Objects)]() mutable -> TCFuture<void>
				{
					TCFutureVector<void> Results;

					(
						[&]
						{
							fg_Move(p_Objects).f_Destroy() > Results;
						}
						()
						, ...
					);

					co_await fg_AllDone(Results);
				}
			)
			.f_Future()
		;
	}

	template <typename t_CActor>
	template <typename tf_CStr>
	void TCActor<t_CActor>::f_Format(tf_CStr &o_Str) const
	{
		if (m_pInternalActor)
		{
			auto *pDistributedData = m_pInternalActor->f_GetDistributedActorData().f_Get();
			if (pDistributedData)
				o_Str += typename tf_CStr::CFormat("Remote({}) 0x{}") << pDistributedData->m_ActorID << f_Get();
			else
				o_Str += typename tf_CStr::CFormat("Local 0x{}") << f_Get();
		}
		else
			o_Str += typename tf_CStr::CFormat("Null 0x{}") << f_Get();
	}

	template <typename tf_CType>
	bool CActor::fp_CheckDestroyed(TCPromise<tf_CType> const &o_Promise)
	{
		if (f_IsDestroyed())
		{
			o_Promise.f_SetException(fp_CheckDestroyed());
			return true;
		}
		return false;
	}

	inline_always CConcurrencyManager &CActor::f_ConcurrencyManager() const
	{
		return *(self.m_pThis->mp_pConcurrencyManager);
	}

	template <typename tf_CReturnType, typename tf_CFunction, typename ...tfp_CParams>
	mark_no_coroutine_debug inline_always tf_CReturnType CActor::fp_DispatchWithReturnCoroutine
		(
			tf_CFunction &&_fToDisptach
			, tfp_CParams ...p_Params
		)
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		struct CGeneratorFunctionKeepAlive final : public CPromiseKeepAlive
		{
			CGeneratorFunctionKeepAlive
				(
					tf_CFunction &&_fToDisptach
				)
				: CPromiseKeepAlive(sizeof(*this))
				, m_fToDisptach(fg_Move(_fToDisptach))
			{
			}

			tf_CFunction m_fToDisptach;
		};

		auto pKeepAlive = new CGeneratorFunctionKeepAlive(fg_Move(_fToDisptach));

#if DMibEnableSafeCheck > 0
		bool bPreviousExpectCoroutineCall = PromiseThreadLocal.m_bExpectCoroutineCall;
		PromiseThreadLocal.m_bExpectCoroutineCall = true;
		auto CleanupExpectCoroutineCall = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
			}
		;

		auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy;
		PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = nullptr;

		auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
		PromiseThreadLocal.m_bCaptureDebugException = true;

		auto bPreviousSafeCall = PromiseThreadLocal.m_bSafeCall;
		PromiseThreadLocal.m_bSafeCall = true;

		auto CleanupConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
				PromiseThreadLocal.m_bSafeCall = bPreviousSafeCall;
			}
		;
#endif

		auto pPreviousKeepAlive = PromiseThreadLocal.m_pKeepAlive;
		PromiseThreadLocal.m_pKeepAlive = pKeepAlive;

		auto Cleanup = g_OnScopeExit / [&]
			{
				if (PromiseThreadLocal.m_pKeepAlive)
				{
					DMibFastCheck(PromiseThreadLocal.m_pKeepAlive == pKeepAlive);
					fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pKeepAlive);
				}
				PromiseThreadLocal.m_pKeepAlive = pPreviousKeepAlive;
			}
		;

#if DMibEnableSafeCheck > 0
		auto Return = pKeepAlive->m_fToDisptach(fg_Move(p_Params)...);
		DMibFastCheck
			(
				PromiseThreadLocal.m_bExpectCoroutineCall
				|| !PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy
				|| Return.f_Debug_HasData(PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy)
			)
		;
		return Return;
#else
		return pKeepAlive->m_fToDisptach(fg_Move(p_Params)...);
#endif

	}

	template <typename tf_CReturnType, typename ...tfp_CParams>
	mark_no_coroutine_debug tf_CReturnType CActor::f_DispatchWithReturn
		(
			NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)> &&_fToDisptach
			, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
		)
		requires (NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value || NPrivate::TCIsFuture<tf_CReturnType>::mc_Value)
	{
		return fp_DispatchWithReturnCoroutine<tf_CReturnType>(fg_Move(_fToDisptach), fg_Move(p_Params)...);
	}

	template <typename tf_CReturnType, typename ...tfp_CParams>
	mark_no_coroutine_debug tf_CReturnType CActor::f_DispatchWithReturn
		(
			NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)> &&_fToDisptach
			, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
		)
		requires (!(NPrivate::TCIsFuture<tf_CReturnType>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value))
	{
		return _fToDisptach(fg_Move(p_Params)...);
	}

	template <typename tf_CReturnType, typename ...tfp_CParams>
	mark_no_coroutine_debug tf_CReturnType CActor::f_DispatchWithReturnShared
		(
			NStorage::TCSharedPointer
			<
				NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)>
			> &&_pToDisptach
			, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
		)
		requires (NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value || NPrivate::TCIsFuture<tf_CReturnType>::mc_Value)
	{
		return fp_DispatchWithReturnCoroutine<tf_CReturnType>(fg_Move(_pToDisptach), fg_Move(p_Params)...);
	}

	template <typename tf_CReturnType, typename ...tfp_CParams>
	mark_no_coroutine_debug tf_CReturnType CActor::f_DispatchWithReturnShared
		(
			NStorage::TCSharedPointer
			<
				NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)>
			> &&_pToDisptach
			, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
		)
		requires (!(NPrivate::TCIsFuture<tf_CReturnType>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value))
	{
		return (*_pToDisptach)(fg_Move(p_Params)...);
	}

	template <typename t_CActor>
	TCRoundRobinActors<t_CActor>::TCRoundRobinActors(mint _nActors)
	{
		DMibFastCheck(_nActors > 0);
		mp_Actors.f_SetLen(_nActors);
	}

	template <typename t_CActor>
	CDispatchHelperWithActor TCRoundRobinActors<t_CActor>::f_Dispatch() const
	{
		return CDispatchHelperWithActor(**this);
	}

	template <typename t_CActor>
	template <typename tf_CParam>
	void TCRoundRobinActors<t_CActor>::f_Construct(tf_CParam &&_Param)
	{
		for (auto &Actor : mp_Actors)
			Actor = fg_TempCopy(_Param);
	}

	template <typename t_CActor>
	bool TCRoundRobinActors<t_CActor>::f_IsConstructed() const
	{
		return mp_Actors.f_GetLen() && !!mp_Actors[0];
	}

	template <typename t_CActor>
	template <typename tf_CParam>
	void TCRoundRobinActors<t_CActor>::f_ConstructFunctor(tf_CParam &&_fConstruct)
	{
		for (auto &Actor : mp_Actors)
			Actor = _fConstruct();
	}

	template <typename t_CActor>
	TCFuture<void> TCRoundRobinActors<t_CActor>::f_Destroy()
	{
		TCFutureVector<void> Results;
		for (auto &Actor : mp_Actors)
		{
			if (Actor)
				fg_Move(Actor).f_Destroy() > Results;
		}

		TCPromiseFuturePair<void> Promise;
		fg_AllDoneWrapped(Results).f_OnResultSet(fg_Move(Promise.m_Promise).f_ReceiveAny());
		return fg_Move(Promise.m_Future);
	}

	template <typename t_CActor>
	mark_nodebug TCActor<t_CActor> const &TCRoundRobinActors<t_CActor>::operator *() const
	{
		mint iCurrentActor = mp_iCurrentActor;
		mp_iCurrentActor = (mp_iCurrentActor + 1) % fg_Max(mp_Actors.f_GetLen(), 1u);
		DMibFastCheck(mp_Actors[iCurrentActor]);
		return mp_Actors[iCurrentActor];
	}

	template <typename t_CActor>
	mark_nodebug TCActor<t_CActor> const *TCRoundRobinActors<t_CActor>::operator -> () const
	{
		return &**this;
	}

	struct [[nodiscard("You need to co_await the ownership transfer")]] CCoroutineTransferOwnershipAwaiter
	{
		CCoroutineTransferOwnershipAwaiter(TCActor<> &&_Actor)
			: mp_Actor(fg_Move(_Actor))
		{
		}

		bool await_ready() noexcept
		{
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;

			DMibFastCheck(fg_CurrentActor(ConcurrencyThreadLocal));

			auto &CoroutineContext = _Handle.promise();

			CoroutineContext.f_Suspend(ThreadLocal, false);
			auto pCleanup = g_OnScopeExitShared / [KeepAlive = CoroutineContext.f_KeepAliveImplicit()]() mutable
				{
					DMibFastCheck(KeepAlive.f_HasValidCoroutine());
					auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();
#if DMibEnableSafeCheck > 0
					auto &CoroutineContext = Handle.promise();
					CoroutineContext.f_SetOwner(fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder);
#endif
					NPrivate::fg_DestroyCoroutine(Handle);
				}
			;
			auto pRealActor = mp_Actor.f_GetRealActor();
			pRealActor->f_QueueProcess
				(
					[
						KeepAlive = CoroutineContext.f_KeepAliveImplicit()
#if DMibEnableSafeCheck > 0
						, ProcessingActor = fg_ThisActor(pRealActor)
#endif
						, pCleanup = fg_Move(pCleanup)
					]
					(CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						DMibFastCheck(KeepAlive.f_HasValidCoroutine());
						DMibFastCheck(ProcessingActor.f_Get() == _ThreadLocal.m_pCurrentlyProcessingActorHolder);

						auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();

						pCleanup->f_ClearFunctor();

						auto &CoroutineContext = Handle.promise();

						if constexpr (NPrivate::TCIsAsyncGeneratorCoroutineContext<tf_CCoroutineContext>::mc_Value)
						{
							auto &GeneratorContext = KeepAlive.template f_Coroutine
								<
									NPrivate::TCAsyncGeneratorCoroutineContext<typename NPrivate::TCIsAsyncGeneratorCoroutineContext<tf_CCoroutineContext>::CReturnType>
								>().promise()
							;
							DMibLock(GeneratorContext.m_pRunState->m_Lock);
							GeneratorContext.m_pRunState->m_AsyncGeneratorOwner = fg_ThisActor(_ThreadLocal.m_pCurrentlyProcessingActorHolder);
						}

#if DMibEnableSafeCheck > 0
						CoroutineContext.f_SetOwner(_ThreadLocal.m_pCurrentlyProcessingActorHolder);
#endif
						{
							bool bAborted = false;
							auto RestoreStates = CoroutineContext.f_Resume(_ThreadLocal.m_SystemThreadLocal, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor, bAborted);
							if (!bAborted)
								Handle.resume();
						}
					}
					, ConcurrencyThreadLocal
				)
			;

			return true;
		}

		void await_resume() noexcept
		{
		}

	private:
		TCActor<> mp_Actor;
	};

	struct [[nodiscard("You need to co_await the ownership transfer")]] CCoroutineTransferOwnership
	{
		CCoroutineTransferOwnership(TCActor<> &&_Actor)
			: mp_Actor(fg_Move(_Actor))
		{
		}

		auto operator co_await()
		{
			return CCoroutineTransferOwnershipAwaiter(fg_Move(mp_Actor));
		}

	private:
		TCActor<> mp_Actor;
	};

	CCoroutineTransferOwnership fg_ContinueRunningOnActor(TCActor<> const &_Actor);
	CCoroutineTransferOwnership fg_ContinueRunningOnActor(CBlockingActorCheckout const &_Checkout);
}
