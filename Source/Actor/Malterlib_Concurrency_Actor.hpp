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
		fp_Construct(fg_Move(_Construct), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CHolderParams) + 1, 1>::CType());
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

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_Value)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> const &>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderSharedPointer<TCActorInternal<tf_CActorSource>> &&_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_Value)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> &&>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorInternal<tf_CActorSource> *_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_Value)
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
	inline_always TCActorInternal<CActor> *TCActor<t_CActor>::f_GetRealActor() const
	{
		return m_pInternalActor->fp_GetRealActor((TCActorInternal<CActor> *)this->f_Get());
	}

	inline_always TCActorInternal<CActor> *CActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return static_cast<TCActorInternal<CActor> *>(_pActorInternal);
	}

	inline_always TCActorInternal<CActor> *CAnyConcurrentActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal).f_Get();
	}

	inline_always TCActorInternal<CActor> *CAnyConcurrentActorLowPrio::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Low).f_Get();
	}

	inline_always TCActorInternal<CActor> *CDynamicConcurrentActor::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActor().f_Get();
	}

	inline_always TCActorInternal<CActor> *CDynamicConcurrentActorLowPrio::fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal)
	{
		return (TCActorInternal<CActor> *)fg_ConcurrencyManager().f_GetConcurrentActorLowPrio().f_Get();
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
	COrdering_Weak TCActor<t_CActor>::operator <=> (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			<=> (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Weak TCActor<t_CActor>::operator <=> (TCWeakActor<tf_CActor> const& _Right) const
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
		return !m_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	template <typename tf_FResult>
	auto TCActor<t_CActor>::operator / (tf_FResult &&_fResult) const
		-> TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
	{
		return TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
			(
				*this
				, fg_Forward<tf_FResult>(_fResult)
			)
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) &&
	{
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				fg_Move(*this)
				, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const &
	{
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				*this
				, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallDirect(tfp_CCallParams &&... p_CallParams) const &
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				*this
				, tf_pMemberFunction
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallDirect(tfp_CCallParams &&... p_CallParams) &&
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				fg_Move(*this)
				, tf_pMemberFunction
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValue(tfp_CCallParams &&... p_CallParams) const &
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;

#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				*this
				, tf_pMemberFunction
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValue(tfp_CCallParams &&... p_CallParams) &&
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;

#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				fg_Move(*this)
				, tf_pMemberFunction
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename tf_CActor
		, typename... tfp_CCallParams
	>
	auto fg_CallActor(TCActor<tf_CActor> &&_Actor, tfp_CCallParams && ...p_CallParams)
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!_Actor.f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
			 	(NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value)
			 	|| !_Actor->f_GetDistributedActorData()
			 	|| _Actor->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCActorCall
			<
				TCActor<tf_CActor>
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				fg_Move(_Actor)
				, tf_pMemberFunction
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename... tfp_CCallParams
	>
	auto TCActor<t_CActor>::f_InternalCallActor(tfp_CCallParams &&... p_CallParams) const &
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash)>(fg_TempCopy(*this), fg_Forward<tfp_CCallParams>(p_CallParams)...);
	}

	template <typename t_CActor>
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename... tfp_CCallParams
	>
	auto TCActor<t_CActor>::f_InternalCallActor(tfp_CCallParams &&... p_CallParams) &&
	{
		return fg_CallActor<tf_pMemberFunction DMibIfNotSupportMemberNameFromMemberPointer(, tf_NameHash)>(fg_Move(*this), fg_Forward<tfp_CCallParams>(p_CallParams)...);
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValueDirect(tfp_CCallParams &&... p_CallParams) const &
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;

#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				*this
				, tf_pMemberFunction
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValueDirect(tfp_CCallParams &&... p_CallParams) &&
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
#if DMibEnableSafeCheck > 0 || defined(DMibConcurrency_CheckFunctionCalls)
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<CMemberFunction>::CType>;
#endif
#ifdef DMibConcurrency_CheckFunctionCalls
		static_assert
			(
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCAddPointer<typename CMemberPointerTraits::CFunctionType>::CType
					, void (tfp_CCallParams...)
				>::mc_Value
				, "Invalid params for function"
			)
		;

#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
			 	(NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value)
			 	|| !(*this)->f_GetDistributedActorData()
			 	|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				fg_Move(*this)
				, tf_pMemberFunction
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
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
					TCPromise<void> Promise;

					TCActorResultVector<void> Results;
					TCInitializerList<bool> Dummy =
						{
							[&]
							{
								fg_Move(p_Objects).f_Destroy() > Results.f_AddResult();
								return false;
							}
							()...
						}
					;
					(void)Dummy;

					Results.f_GetResults() > NPrivate::fg_DirectResultActor() / [Promise](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
						{
							if (!fg_CombineResults(Promise, fg_Move(_Results)))
								return;

							Promise.f_SetResult();
						}
					;

					return Promise.f_MoveFuture();
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

	template <typename tf_CReturnType, typename ...tfp_CParams>
	mark_no_coroutine_debug tf_CReturnType CActor::f_DispatchWithReturn
		(
			NFunction::TCFunctionMovable<tf_CReturnType (typename NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>::CType ...p_Params)> &&_fToDisptach
			, typename NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>::CType ...p_Params
		)
	{
		return _fToDisptach(fg_Move(p_Params)...);
	}

	template <typename t_CActor>
	TCRoundRobinActors<t_CActor>::TCRoundRobinActors(mint _nActors)
	{
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
		TCPromise<void> Promise;

		TCActorResultVector<void> Results;
		for (auto &Actor : mp_Actors)
		{
			if (Actor)
				Actor.f_Destroy() > Results.f_AddResult();
		}

		Results.f_GetResults() > Promise.f_ReceiveAny();
		return Promise.f_MoveFuture();
	}

	template <typename t_CActor>
	TCActor<t_CActor> const &TCRoundRobinActors<t_CActor>::operator *() const
	{
		mint iCurrentActor = mp_iCurrentActor;
		mp_iCurrentActor = (mp_iCurrentActor + 1) % fg_Max(mp_Actors.f_GetLen(), 1);
		return mp_Actors[iCurrentActor];
	}

	template <typename t_CActor>
	TCActor<t_CActor> const *TCRoundRobinActors<t_CActor>::operator -> () const
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
			DMibFastCheck(fg_CurrentActor());
			DMibFastCheck(fg_CurrentActorProcessingOrOverridden());

			auto &CoroutineContext = _Handle.promise();

			using CCoroutineContext = typename NTraits::TCRemoveReference<decltype(CoroutineContext)>::CType;

			static_assert(CCoroutineContext::mc_bSupportOwnershipTransfer, "Ownership transfer not supported on this coroutine");

			CoroutineContext.f_Suspend(false);
			auto pCleanup = g_OnScopeExitShared > [_Handle, KeepAlive = CoroutineContext.f_KeepAlive(fg_TempCopy(mp_Actor))]() mutable
				{
					DMibFastCheck(KeepAlive.f_HasValidCoroutine());
#if DMibEnableSafeCheck > 0
					auto &CoroutineContext = _Handle.promise();
					CoroutineContext.f_SetOwner(fg_CurrentActor().f_Weak());
#endif
					_Handle.destroy();
				}
			;
			auto pRealActor = mp_Actor.f_GetRealActor();
			pRealActor->f_QueueProcess
				(
					[
						this
						, _Handle
						, KeepAlive = CoroutineContext.f_KeepAlive(fg_TempCopy(mp_Actor))
						, ProcessingActor = fg_ThisActor(pRealActor)
						, pCleanup = fg_Move(pCleanup)
					]
					() mutable
					{
						DMibFastCheck(KeepAlive.f_HasValidCoroutine());
						DMibFastCheck(ProcessingActor == fg_CurrentActor());

						pCleanup->f_Clear();

						(void)this;
						auto &CoroutineContext = _Handle.promise();
						CCurrentActorScope CurrentActorScope(ProcessingActor);
#if DMibEnableSafeCheck > 0
						CoroutineContext.f_SetOwner(ProcessingActor.f_Weak());
#endif
						bool bAborted = false;
						auto RestoreStates = CoroutineContext.f_Resume(bAborted, false);
						if (!bAborted)
							_Handle.resume();
					}
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
}
