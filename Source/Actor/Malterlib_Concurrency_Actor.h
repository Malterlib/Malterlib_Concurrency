// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CActor;
	namespace NPrivate
	{
		struct CThisActor
		{
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;

			template <typename tf_FFunction>
			auto operator / (tf_FFunction &&_fFunction) const;

			operator TCActor<> () const;

			NStorage::TCBitStorePointer<CActorHolder> m_pThis = nullptr;
		private:
			friend struct NConcurrency::CActor;

			CThisActor() = default;
			CThisActor(CThisActor &&) = default;
			CThisActor(CThisActor const &) = default;
			CThisActor &operator = (CThisActor &&) = default;
			CThisActor &operator = (CThisActor const &) = default;
		};
	}

	struct CActorInternal
	{
#if DMibEnableSafeCheck > 0
		CActorInternal();
		~CActorInternal();
		CActor *mp_pThis = nullptr;
#endif
	};

	struct CActor /// \brief Base class for all types used with \ref TCActor
	{
		using CActorHolder = CDefaultActorHolder;

		CActor();
		CActor(CActor &&) = delete;
		CActor(CActor const &) = delete;
		virtual ~CActor();

		CActor &operator = (CActor &&) = delete;
		CActor &operator = (CActor const &) = delete;

		bool f_IsDestroyed() const;
		void f_Dispatch(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);
		template <typename tf_CReturnType, typename ...tfp_CParams>
		tf_CReturnType f_DispatchWithReturn
			(
				NFunction::TCFunctionMovable<tf_CReturnType (typename NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>::CType ...p_Params)> &&_fToDisptach
				, typename NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>::CType ...p_Params
			)
		;
		inline_always CConcurrencyManager &f_ConcurrencyManager() const;

		void f_SuspendCoroutine(CFutureCoroutineContext &_CoroutineContext);

	protected:
		virtual void fp_Construct();
		virtual TCFuture<void> fp_Destroy();
#if DMibEnableSafeCheck > 0
		virtual void fp_CheckDestroy();
#endif

		NException::CExceptionPointer fp_CheckDestroyed();

		template <typename tf_CType>
		bool fp_CheckDestroyed(TCPromise<tf_CType> const &o_Promise);

	private:
		template <typename tf_CActor>
		friend TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);
		friend class CConcurrencyManager;
		template <typename t_CActor>
		friend class TCActorInternal;

		template <typename t_CCallbackSignature, bool _bSupportMultiple, typename t_CExtraData>
		friend class TCActorSubscriptionManager;

		friend class NConcurrency::CActorHolder;
		friend struct NConcurrency::CActorCommon;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		void fp_AbortSuspendedCoroutines();

		void fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);

		TCFuture<void> fp_DestroyInternal();

	public:
		static constexpr bool mc_bAllowInternalAccess = false;
		static constexpr bool mc_bImmediateDelete = false;
		static constexpr EPriority mc_Priority = EPriority_Normal;
		static constexpr bool mc_bIsAlwaysAlive = false;

		NPrivate::CThisActor self;

	private:
		DMibListLinkDS_List(CFutureCoroutineContext, m_Link) mp_SuspendedCoroutines;
	};

	class CSeparateThreadActorHolder;

	struct CSeparateThreadActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;
	};

	template <typename t_CActor>
	struct TCRoundRobinActors
	{
		TCRoundRobinActors(mint _nActors);

		bool f_IsConstructed() const;

		template <typename tf_CParam>
		void f_Construct(tf_CParam &&_Param);

		template <typename tf_CParam>
		void f_ConstructFunctor(tf_CParam &&_fConstruct);

		TCFuture<void> f_Destroy();

		TCActor<t_CActor> const &operator *() const;

	private:
		NContainer::TCVector<TCActor<t_CActor>> mp_Actors;
		mutable mint mp_iCurrentActor = 0;
	};
}
