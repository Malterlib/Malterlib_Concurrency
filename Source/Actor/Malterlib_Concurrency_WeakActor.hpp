// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename t_CType>
		struct TCIsPromise;

		template <typename t_CType>
		struct TCIsFuture;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor()
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::~TCWeakActor()
	{
		if constexpr (!mc_bIsAlwaysAlive)
			m_pInternalActor.~CPointerType();
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor)
		: m_pInternalActor(_pActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
		: m_pInternalActor(fg_Move(_pActor))
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderWeakPointer<CActorInternal> const &_pActor)
		: m_pInternalActor(_pActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderWeakPointer<CActorInternal> &&_pActor)
		: m_pInternalActor(fg_Move(_pActor))
	{
	}
	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor const &_Other)
		: m_pInternalActor(_Other.m_pInternalActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor &&_Other)
		: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> const &_Other)
		: m_pInternalActor(_Other.m_pInternalActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> &&_Other)
		: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
	{
		_Other.f_Clear();
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderWeakPointer<TCActorInternal<tf_CActorSource>> const &_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderWeakPointer<TCActorInternal<tf_CActor>> const &>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderWeakPointer<TCActorInternal<tf_CActorSource>> &&_pActorHolder)
	{
		auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<tf_CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderWeakPointer<TCActorInternal<tf_CActor>> &&>(_pActorHolder);
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> const &_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(_Other.m_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> &&_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(fg_Move(_Other.m_pInternalActor)))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> const &_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(_Other.m_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> &&_Other)
		: m_pInternalActor(fg_StaticCast<t_CActor>(fg_Move(_Other.m_pInternalActor)))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActorHolderSharedPointer<CActorInternal> const &_pActor)
	{
		m_pInternalActor = _pActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
	{
		m_pInternalActor = fg_Move(_pActor);
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActorHolderWeakPointer<CActorInternal> const &_pActor)
	{
		m_pInternalActor = _pActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActorHolderWeakPointer<CActorInternal> &&_pActor)
	{
		m_pInternalActor = fg_Move(_pActor);
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor const &_Other)
	{
		m_pInternalActor = _Other.m_pInternalActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor &&_Other)
	{
		m_pInternalActor = fg_Move(_Other.m_pInternalActor);

		if constexpr (mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
		return *this;
	}


	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor<tf_CActor> const &_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_StaticCast<t_CActor>(_Other.m_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_StaticCast<t_CActor>(fg_Move(_Other.m_pInternalActor));

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> const &_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_StaticCast<t_CActor>(_Other.m_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		m_pInternalActor = fg_StaticCast<t_CActor>(fg_Move(_Other.m_pInternalActor));

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.m_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	void TCWeakActor<t_CActor>::f_Clear()
	{
		m_pInternalActor.f_Clear();
	}

	template <typename t_CActor>
	bool TCWeakActor<t_CActor>::f_IsEmpty() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return !m_pInternalActor;
		else
			return m_pInternalActor.f_IsEmpty();

	}

	template <typename t_CActor>
	TCActor<t_CActor> TCWeakActor<t_CActor>::f_Lock() const
	{
		TCActor<t_CActor> Ret;
		if constexpr (mc_bIsAlwaysAlive)
			Ret.m_pInternalActor = m_pInternalActor;
		else
			Ret.m_pInternalActor = m_pInternalActor.f_Lock();
		return Ret;
	}

	template <typename t_CActor>
	inline_always typename TCWeakActor<t_CActor>::CActorInternal *TCWeakActor<t_CActor>::fp_Get() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return m_pInternalActor;
		else
			return m_pInternalActor.f_UnsafeGetPointerValue();
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Weak  TCWeakActor<t_CActor>::operator <=> (TCWeakActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)m_pInternalActor
			<=> (TCActorHolderWeakPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Weak TCWeakActor<t_CActor>::operator <=> (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)m_pInternalActor
			<=> (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCWeakActor<t_CActor>::operator == (TCWeakActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)m_pInternalActor
			== (TCActorHolderWeakPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCWeakActor<t_CActor>::operator == (TCActor<tf_CActor> const& _Right) const
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)m_pInternalActor
			== (TCActorHolderSharedPointer<CActorHolder> const &)_Right.m_pInternalActor
		;
	}

	template <typename t_CActor>
	inline_small TCWeakActor<t_CActor>::operator bool() const
	{
		return !m_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename... tfp_CCallParams>
	auto TCWeakActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const &
		requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());
		return TCActorCall
			<
				TCWeakActor
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
	auto TCWeakActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) &&
		requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());
		return TCActorCall
			<
				TCWeakActor
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
	template <auto tf_pMemberFunction, typename... tfp_CCallParams>
	auto TCWeakActor<t_CActor>::f_CallByValue(tfp_CCallParams &&... p_CallParams) const &
		requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
		DMibFastCheck(!f_IsEmpty());
		return TCActorCall
			<
				TCWeakActor
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
	auto TCWeakActor<t_CActor>::f_CallByValue(tfp_CCallParams &&... p_CallParams) &&
		requires cActorCallableWithFunctor<tf_pMemberFunction, t_CActor, tfp_CCallParams...>
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
		DMibFastCheck(!f_IsEmpty());
		return TCActorCall
			<
				TCWeakActor
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

	template <typename t_CActor>
	template <typename tf_CStr>
	void TCWeakActor<t_CActor>::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Weak 0x{}") << fp_Get();
	}

	template <typename tf_FToDispatch, typename ...tfp_CParams>
	inline_always auto fg_Dispatch(TCWeakActor<> const &_Actor, tf_FToDispatch &&_fDispatch, tfp_CParams && ...p_Params)
	{
		using CFunctionType = typename NTraits::TCRemoveReference<tf_FToDispatch>::CType;

		return NPrivate::fg_DispatchGenericImpl<false>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, _Actor
				, fg_Forward<tf_FToDispatch>(_fDispatch)
			)
		;
	}
}
