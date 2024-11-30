// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor = CActor>
	class TCWeakActor
	{
		friend class CConcurrencyManager;
		template <typename t_CActor2>
		friend class TCActor;
		template <typename t_CActor2>
		friend class TCWeakActor;
		friend class CConcurrencyManager;

	public:
		using CActorInternal = TCActorInternal<t_CActor>;
		using CContainedActor = t_CActor;
		using CNonWeak = TCActor<t_CActor>;
		using CWeak = TCWeakActor;

		static constexpr bool mc_bIsWeak = true;
		static constexpr bool mc_bIsAlwaysAlive = TCIsActorAlwaysAlive<CContainedActor>::mc_bImpl;

	private:
		using CPointerType = typename TCChooseType
			<
				mc_bIsAlwaysAlive
				, CActorInternal *
				, TCActorHolderWeakPointer<CActorInternal>
			>
			::CType
		;
		union
		{
			CPointerType m_pInternalActor = nullptr;
			uint8 m_Dummy[sizeof(TCActorHolderWeakPointer<CActorInternal>)];
		};

		CActorInternal *fp_Get() const;

	public:
		TCWeakActor();
		~TCWeakActor();
		TCWeakActor(TCActorHolderSharedPointer<CActorInternal> const &_pActor);
		TCWeakActor(TCActorHolderSharedPointer<CActorInternal> &&_pActor);
		TCWeakActor(TCActorHolderWeakPointer<CActorInternal> const &_pActor);
		TCWeakActor(TCActorHolderWeakPointer<CActorInternal> &&_pActor);
		TCWeakActor(TCWeakActor const &_Other);
		TCWeakActor(TCWeakActor &&_Other);

		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor(TCWeakActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor(TCWeakActor<tf_CActor> &&_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer explicit TCWeakActor(TCActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer explicit TCWeakActor(TCActor<tf_CActor> &&_Other);

		TCWeakActor(TCActor<t_CActor> const &_Other);
		TCWeakActor(TCActor<t_CActor> &&_Other);

		TCWeakActor &operator =(TCActorHolderSharedPointer<CActorInternal> const &_pActor);
		TCWeakActor &operator =(TCActorHolderSharedPointer<CActorInternal> &&_pActor);
		TCWeakActor &operator =(TCActorHolderWeakPointer<CActorInternal> const &_pActor);
		TCWeakActor &operator =(TCActorHolderWeakPointer<CActorInternal> &&_pActor);
		TCWeakActor &operator =(TCWeakActor const &_Other);
		TCWeakActor &operator =(TCWeakActor &&_Other);

		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor &operator =(TCWeakActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor &operator =(TCWeakActor<tf_CActor> &&_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor &operator =(TCActor<tf_CActor> const &_Other);
		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCWeakActor &operator =(TCActor<tf_CActor> &&_Other);

		void f_Clear();
		bool f_IsEmpty() const;

		TCActor<t_CActor> f_Lock() const;

		TCActorHolderWeakPointer<CActorInternal> &f_Unsafe_AccessInternal();

		template <typename tf_CActor>
		COrdering_Strong operator <=> (TCWeakActor<tf_CActor> const& _Right) const;
		template <typename tf_CActor>
		COrdering_Strong operator <=> (TCActor<tf_CActor> const& _Right) const;

		template <typename tf_CActor>
		bool operator == (TCWeakActor<tf_CActor> const& _Right) const;
		template <typename tf_CActor>
		bool operator == (TCActor<tf_CActor> const& _Right) const;

		inline_small explicit operator bool() const;

		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const &
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<tf_CMemberFunction>
				, TCWeakActor<t_CActor> const &
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
				, TCWeakActor<t_CActor> &&
				, tf_CMemberFunction
				, CBindActorOptions::fs_Default()
				, false
				, tfp_CCallParams...
			>
			requires cActorCallableWith<tf_CMemberFunction, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_Bind(tfp_CCallParams &&... p_CallParams) const &
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
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_Bind(tfp_CCallParams &&... p_CallParams) &&
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
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_BindByValue(tfp_CCallParams &&... p_CallParams) const &
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecayType<tfp_CCallParams>...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions = CBindActorOptions{}, typename... tfp_CCallParams>
		auto f_BindByValue(tfp_CCallParams &&... p_CallParams) &&
			-> TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCWeakActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecayType<tfp_CCallParams>...
			>
			requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
		;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;
	};
}
