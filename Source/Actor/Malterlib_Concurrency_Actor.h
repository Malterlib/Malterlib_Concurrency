// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/BitStorePointer>

namespace NMib::NConcurrency
{
	struct CActor;
	namespace NPrivate
	{ 
		struct CThisActor
		{
			template <typename tf_FFunction>
			auto operator / (tf_FFunction &&_fFunction) const;

			template <typename tf_FFunction, typename... tfp_CCallParams>
			auto f_Invoke(tf_FFunction &&_fFunction, tfp_CCallParams &&... p_CallParams) const;

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
		
		template <typename t_CType>
		struct TCIsFuture;

		template <typename t_CType>
		struct TCIsAsyncGenerator;
	}

	struct CActorInternal
	{
#if DMibEnableSafeCheck > 0
		CActorInternal();
		~CActorInternal();
	private:
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
		TCFuture<void> f_Dispatch(NFunction::TCFunctionMovable<void ()> _fToDisptach);

		template <typename tf_CReturnType, typename ...tfp_CParams>
		tf_CReturnType f_DispatchWithReturn
			(
				NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)> &&_fToDisptach
				, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
			)
			requires (NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value || NPrivate::TCIsFuture<tf_CReturnType>::mc_Value)
		;

		template <typename tf_CReturnType, typename ...tfp_CParams>
		tf_CReturnType f_DispatchWithReturn
			(
				NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)> &&_fToDisptach
				, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
			)
			requires (!(NPrivate::TCIsFuture<tf_CReturnType>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value))
		;

		template <typename tf_CReturnType, typename ...tfp_CParams>
		tf_CReturnType f_DispatchWithReturnShared
			(
				NStorage::TCSharedPointer
				<
					NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)>
				> &&_pToDisptach
				, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
			)
			requires (NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value || NPrivate::TCIsFuture<tf_CReturnType>::mc_Value)
		;

		template <typename tf_CReturnType, typename ...tfp_CParams>
		tf_CReturnType f_DispatchWithReturnShared
			(
				NStorage::TCSharedPointer
				<
					NFunction::TCFunctionMovable<tf_CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params)>
				> &&_pToDisptach
				, NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams> ...p_Params
			)
			requires (!(NPrivate::TCIsFuture<tf_CReturnType>::mc_Value || NPrivate::TCIsAsyncGenerator<tf_CReturnType>::mc_Value))
		;

		inline_always CConcurrencyManager &f_ConcurrencyManager() const;

		bool f_SuspendCoroutine(CFutureCoroutineContext &_CoroutineContext);
		CFutureCoroutineContext::COnResumeScopeAwaiter f_CheckDestroyedOnResume() const;

	protected:
		template <typename tf_CReturnType, typename tf_CFunction, typename ...tfp_CParams>
		inline_always tf_CReturnType fp_DispatchWithReturnCoroutine
			(
				tf_CFunction &&_fToDisptach
				, tfp_CParams ...p_Params
			)
		;

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

		friend class NConcurrency::CActorHolder;
		friend struct NConcurrency::CActorCommon;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		TCFuture<void> fp_AbortSuspendedCoroutinesWithAsyncDestroy();
		void fp_AbortSuspendedCoroutines();

		TCFuture<void> fp_DestroyInternal();

	public:
		static constexpr bool mc_bAllowInternalAccess = false;
		static constexpr bool mc_bImmediateDelete = false;
		static constexpr EPriority mc_Priority = EPriority_Normal;
		static constexpr bool mc_bIsAlwaysAlive = false;
		static constexpr bool mc_bIsAlwaysAliveImpl = false;

		NPrivate::CThisActor self;

	private:
		DMibListLinkDS_List(CFutureCoroutineContext, m_Link) mp_SuspendedCoroutines;
	};

	class CSeparateThreadActorHolder;

	struct CSeparateThreadActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;
	};

	class CDispatchingActorHolder;

	struct CDispatchingActor : public CActor
	{
		using CActorHolder = CDispatchingActorHolder;
	};

	struct CDispatchHelperWithActor;

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

		mark_nodebug TCActor<t_CActor> const &operator *() const;
		mark_nodebug TCActor<t_CActor> const *operator -> () const;
		CDispatchHelperWithActor f_Dispatch() const;

	private:
		NContainer::TCVector<TCActor<t_CActor>> mp_Actors;
		mutable mint mp_iCurrentActor = 0;
	};
}
