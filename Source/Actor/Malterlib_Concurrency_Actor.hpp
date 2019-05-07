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
		: m_pInternalActor(_pActor)
	{
	}
	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
		: m_pInternalActor(fg_Move(_pActor))
	{
	}
	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActor const &_Other)
		: m_pInternalActor(_Other.m_pInternalActor)
	{
	}
	template <typename t_CActor>
	TCActor<t_CActor>::TCActor(TCActor &&_Other)
		: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
	{
	}

	template <typename tf_CActor, typename tf_CActorSource>
	TCActor<tf_CActor> fg_StaticCast(TCActor<tf_CActorSource> const &_Actor)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		return reinterpret_cast<TCActor<tf_CActor> const &>(_Actor);
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor>::TCActor(TCActor<tf_CActor> const &_Other)
		: m_pInternalActor(reinterpret_cast<TCActor const &>(_Other).m_pInternalActor)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}
	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor>::TCActor(TCActor<tf_CActor> &&_Other)
		: m_pInternalActor(fg_Move(reinterpret_cast<TCActor &>(_Other).m_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
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
		m_pInternalActor = reinterpret_cast<TCActor const &>(_Other).m_pInternalActor;
		return *this;
	}
	template <typename t_CActor>
	template <typename tf_CActor>
	TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_Move(reinterpret_cast<TCActor &>(_Other).m_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	typename TCActor<t_CActor>::CActorInternal *TCActor<t_CActor>::operator ->() const
	{
		return &(*m_pInternalActor);
	}

	template <typename t_CActor>
	inline_always TCActor<CActor> const &TCActor<t_CActor>::f_GetActor() const
	{
		return *reinterpret_cast<TCActor<CActor> const *>(this);
	}

	template <>
	inline_always TCActor<CActor> const &TCActor<CAnyConcurrentActor>::f_GetActor() const
	{
		return *reinterpret_cast<TCActor<CActor> const *>(&fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal));
	}
	template <>
	inline_always TCActor<CActor> const &TCActor<CAnyConcurrentActorLowPrio>::f_GetActor() const
	{
		return *reinterpret_cast<TCActor<CActor> const *>(&fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Low));
	}

	template <typename t_CActor>
	void TCActor<t_CActor>::f_Clear()
	{
		m_pInternalActor.f_Clear();
	}

	template <typename t_CActor>
	bool TCActor<t_CActor>::f_IsEmpty() const
	{
		return m_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> TCActor<t_CActor>::f_Weak() const
	{
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCActor<t_CActor>::operator < (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			< (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCActor<t_CActor>::operator < (TCWeakActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderSharedPointer<CActorHolder> const &)m_pInternalActor
			< (TCActorHolderWeakPointer<CActorHolder> const &)_Right.m_pInternalActor
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
	auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
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
		DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);

		// If you get this, use DCallActor instead of calling directly
		DMibFastCheck(f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());


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
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallDirect(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
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
		DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);

		// If you get this, use DCallActor instead of calling directly
		DMibFastCheck(f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());


		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				*this
				, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
				, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValue(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
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
		DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);

		// If you get this, use DCallActor instead of calling directly
		DMibFastCheck(f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, false
			>
			(
				*this
				, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_CallByValueDirect(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
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
		DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);

		// If you get this, use DCallActor instead of calling directly
		DMibFastCheck(f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value) || !(*this)->f_GetDistributedActorData() || (*this)->f_GetDistributedActorData()->f_IsValidForCall());

		return TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				, NMeta::TCTypeList<tfp_CCallParams...>
				, true
			>
			(
				*this
				, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
				, NStorage::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
			)
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
				o_Str += typename tf_CStr::CFormat("Remote({}) 0x{}") << pDistributedData->m_ActorID << m_pInternalActor.f_Get();
			else
				o_Str += typename tf_CStr::CFormat("Local 0x{}") << m_pInternalActor.f_Get();
		}
		else
			o_Str += typename tf_CStr::CFormat("Null 0x{}") << m_pInternalActor.f_Get();
	}

	template <typename tf_CType>
	bool CActor::fp_CheckDestroyed(TCPromise<tf_CType> const &o_Promise)
	{
		if (mp_bDestroyed)
		{
			o_Promise.f_SetException(fp_CheckDestroyed());
			return true;
		}
		return false;
	}

#if DMibEnableSafeCheck > 0
	namespace NPrivate
	{
		mint fg_WrapDispatchWithReturn(NFunction::TCFunction<void ()> const &_fDoDisptach);
	}

	template <typename tf_CReturnType>
	inline_never tf_CReturnType CActor::f_DispatchWithReturn(NFunction::TCFunctionMovable<tf_CReturnType ()> &&_fToDisptach)
	{
		tf_CReturnType Return;

		NPrivate::fg_WrapDispatchWithReturn
			(
				[&]
				{
					Return = _fToDisptach();
				}
			)
		;
		return Return;
	}
#else
	template <typename tf_CReturnType>
	tf_CReturnType CActor::f_DispatchWithReturn(NFunction::TCFunctionMovable<tf_CReturnType ()> &&_fToDisptach)
	{
		return _fToDisptach();
	}
#endif

	template <typename t_CActor>
	TCRoundRobinActors<t_CActor>::TCRoundRobinActors(mint _nActors)
	{
		mp_Actors.f_SetLen(_nActors);
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
		TCActorResultVector<void> Results;
		for (auto &Actor : mp_Actors)
		{
			if (Actor)
				Actor->f_Destroy() > Results.f_AddResult();
		}

		TCPromise<void> Promise;
		Results.f_GetResults() > Promise.f_ReceiveAny();
		return Promise.f_MoveFuture();
	}

	template <typename t_CActor>
	TCActor<t_CActor> const &TCRoundRobinActors<t_CActor>::operator *() const
	{
		mint iCurrentActor = mp_iCurrentActor;
		mp_iCurrentActor = (mp_iCurrentActor + 1) % mp_Actors.f_GetLen();
		return mp_Actors[iCurrentActor];
	}
}
