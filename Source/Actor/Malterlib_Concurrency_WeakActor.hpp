// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
			mp_pInternalActor.~CPointerType();
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor)
		: mp_pInternalActor(_pActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor)
		: mp_pInternalActor(fg_Move(_pActor))
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderWeakPointer<CActorInternal> const &_pActor)
		: mp_pInternalActor(_pActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActorHolderWeakPointer<CActorInternal> &&_pActor)
		: mp_pInternalActor(fg_Move(_pActor))
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor const &_Other)
		: mp_pInternalActor(_Other.mp_pInternalActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor &&_Other)
		: mp_pInternalActor(fg_Move(_Other.mp_pInternalActor))
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> const &_Other)
		: mp_pInternalActor(_Other.mp_pInternalActor)
	{
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> &&_Other)
		: mp_pInternalActor(fg_Move(_Other.mp_pInternalActor))
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
		: mp_pInternalActor(fg_StaticCast<t_CActor>(_Other.mp_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> &&_Other)
		: mp_pInternalActor(fg_StaticCast<t_CActor>(fg_Move(_Other.mp_pInternalActor)))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> const &_Other)
		: mp_pInternalActor(fg_StaticCast<t_CActor>(_Other.mp_pInternalActor))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> &&_Other)
		: mp_pInternalActor(fg_StaticCast<t_CActor>(fg_Move(_Other.mp_pInternalActor)))
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActorHolderSharedPointer<CActorInternal> const &_pActor)
	{
		mp_pInternalActor = _pActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActorHolderSharedPointer<CActorInternal> &&_pActor)
	{
		mp_pInternalActor = fg_Move(_pActor);
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActorHolderWeakPointer<CActorInternal> const &_pActor)
	{
		mp_pInternalActor = _pActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActorHolderWeakPointer<CActorInternal> &&_pActor)
	{
		mp_pInternalActor = fg_Move(_pActor);
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCWeakActor const &_Other)
	{
		mp_pInternalActor = _Other.mp_pInternalActor;
		return *this;
	}

	template <typename t_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCWeakActor &&_Other)
	{
		mp_pInternalActor = fg_Move(_Other.mp_pInternalActor);

		if constexpr (mc_bIsAlwaysAlive)
			_Other.mp_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCWeakActor<tf_CActor> const &_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		mp_pInternalActor = fg_StaticCast<t_CActor>(_Other.mp_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCWeakActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		mp_pInternalActor = fg_StaticCast<t_CActor>(fg_Move(_Other.mp_pInternalActor));

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.mp_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActor<tf_CActor> const &_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		mp_pInternalActor = fg_StaticCast<t_CActor>(_Other.mp_pInternalActor);
		return *this;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator = (TCActor<tf_CActor> &&_Other)
	{
		static_assert(NStorage::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		mp_pInternalActor = fg_StaticCast<t_CActor>(fg_Move(_Other.mp_pInternalActor));

		if constexpr (tf_CActor::mc_bIsAlwaysAlive)
			_Other.mp_pInternalActor = nullptr;
		return *this;
	}

	template <typename t_CActor>
	void TCWeakActor<t_CActor>::f_Clear()
	{
		mp_pInternalActor.f_Clear();
	}

	template <typename t_CActor>
	bool TCWeakActor<t_CActor>::f_IsEmpty() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return !mp_pInternalActor;
		else
			return mp_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	TCActor<t_CActor> TCWeakActor<t_CActor>::f_Lock() const
	{
		TCActor<t_CActor> Ret;
		if constexpr (mc_bIsAlwaysAlive)
			Ret.mp_pInternalActor = mp_pInternalActor;
		else
			Ret.mp_pInternalActor = mp_pInternalActor.f_Lock();
		return Ret;
	}

	template <typename t_CActor>
	auto TCWeakActor<t_CActor>::f_Unsafe_AccessInternal() -> TCActorHolderWeakPointer<CActorInternal> &
	{
		static_assert(!mc_bIsAlwaysAlive);
		return mp_pInternalActor;
	}

	template <typename t_CActor>
	inline_always typename TCWeakActor<t_CActor>::CActorInternal *TCWeakActor<t_CActor>::fp_Get() const
	{
		if constexpr (mc_bIsAlwaysAlive)
			return mp_pInternalActor;
		else
			return mp_pInternalActor.f_UnsafeGetPointerValue();
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Strong TCWeakActor<t_CActor>::operator <=> (TCWeakActor<tf_CActor> const &_Right) const noexcept
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)mp_pInternalActor <=> (TCActorHolderWeakPointer<CActorHolder> const &)_Right.mp_pInternalActor;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	COrdering_Strong TCWeakActor<t_CActor>::operator <=> (TCActor<tf_CActor> const &_Right) const noexcept
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)mp_pInternalActor <=> (TCActorHolderSharedPointer<CActorHolder> const &)_Right.mp_pInternalActor;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCWeakActor<t_CActor>::operator == (TCWeakActor<tf_CActor> const &_Right) const noexcept
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)mp_pInternalActor == (TCActorHolderWeakPointer<CActorHolder> const &)_Right.mp_pInternalActor;
	}

	template <typename t_CActor>
	template <typename tf_CActor>
	bool TCWeakActor<t_CActor>::operator == (TCActor<tf_CActor> const &_Right) const noexcept
	{
		return (TCActorHolderWeakPointer<CActorHolder> const &)mp_pInternalActor == (TCActorHolderSharedPointer<CActorHolder> const &)_Right.mp_pInternalActor;
	}

	template <typename t_CActor>
	inline_small TCWeakActor<t_CActor>::operator bool () const
	{
		return !mp_pInternalActor.f_IsEmpty();
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams && ...p_CallParams) const &
		-> TCBoundActorCall<NPrivate::TCFutureReturn<tf_CMemberFunction>, TCWeakActor<t_CActor> const &, tf_CMemberFunction, CBindActorOptions::fs_Default(), false, tfp_CCallParams...>
		requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());
		return TCBoundActorCall<NPrivate::TCFutureReturn<tf_CMemberFunction>, TCWeakActor<t_CActor> const &, tf_CMemberFunction, CBindActorOptions::fs_Default(), false, tfp_CCallParams...>
			{
				.m_pMemberFunction = _pMemberFunction
				, .m_Actor{*this}
				, .m_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <typename tf_CMemberFunction, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams && ...p_CallParams) &&
		-> TCBoundActorCall<NPrivate::TCFutureReturn<tf_CMemberFunction>, TCWeakActor<t_CActor> &&, tf_CMemberFunction, CBindActorOptions::fs_Default(), false, tfp_CCallParams...>
		requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());
		return TCBoundActorCall<NPrivate::TCFutureReturn<tf_CMemberFunction>, TCWeakActor<t_CActor> &&, tf_CMemberFunction, CBindActorOptions::fs_Default(), false, tfp_CCallParams...>
			{
				.m_pMemberFunction = _pMemberFunction
				, .m_Actor{fg_Move(*this)}
				, .m_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::f_Bind(tfp_CCallParams && ...p_CallParams) const &
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCWeakActor<t_CActor> const &
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, false
			, tfp_CCallParams...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor> const &
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			{
				.m_pMemberFunction = tf_pFunctionPointer
				, .m_Actor{*this}
				, .m_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::f_Bind(tfp_CCallParams && ...p_CallParams) &&
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCWeakActor<t_CActor> &&
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, false
			, tfp_CCallParams...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor> &&
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			{
				.m_pMemberFunction = tf_pFunctionPointer
				, .m_Actor{fg_Move(*this)}
				, .m_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::f_BindByValue(tfp_CCallParams && ...p_CallParams) const &
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCWeakActor<t_CActor>
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, true
			, NTraits::TCDecay<tfp_CCallParams>...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			{
				.m_pMemberFunction = tf_pFunctionPointer
				, .m_Actor{*this}
				, .m_Params{fg_Forward<tfp_CCallParams>(p_CallParams)...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename ...tfp_CCallParams>
	auto TCWeakActor<t_CActor>::f_BindByValue(tfp_CCallParams && ...p_CallParams) &&
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCWeakActor<t_CActor>
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, true
			, NTraits::TCDecay<tfp_CCallParams>...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
		DMibFastCheck(!f_IsEmpty());

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			{
				.m_pMemberFunction = tf_pFunctionPointer
				, .m_Actor{fg_Move(*this)}
				, .m_Params{fg_Forward<tfp_CCallParams>(p_CallParams)...}
			}
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
		using FFunctionType = NTraits::TCRemoveReference<tf_FToDispatch>;

		return NPrivate::fg_DispatchGenericImpl<false>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&FFunctionType::operator ())>::CParams()
				, _Actor
				, fg_Forward<tf_FToDispatch>(_fDispatch)
			)
		;
	}
}
