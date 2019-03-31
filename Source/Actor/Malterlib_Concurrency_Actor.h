// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	class CActor;
	namespace NPrivate
	{
		struct CThisActor
		{
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;

			template <typename tf_FFunction>
			auto operator / (tf_FFunction &&_fFunction) const;

			operator TCActor<> () const;

			void *m_pThis = nullptr;
		private:
			friend class NConcurrency::CActor;

			CThisActor() = default;
			CThisActor(CThisActor &&) = default;
			CThisActor(CThisActor const &) = default;
			CThisActor &operator = (CThisActor &&) = default;
			CThisActor &operator = (CThisActor const &) = default;
		};
	}

	struct CActorInternal
	{
#if defined DMibContractConfigure_CheckEnabled
		CActorInternal();
		~CActorInternal();
		CActor *mp_pThis = nullptr;
#endif
	};

	class CActor /// \brief Base class for all types used with \ref TCActor
	{
		template <typename tf_CActor>
		friend TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);
		friend class CConcurrencyManager;
		template <typename t_CActor>
		friend class TCActorInternal;

		template <typename t_CCallbackSignature, bool _bSupportMultiple, typename t_CExtraData>
		friend class TCActorSubscriptionManager;

		friend class CActorHolder;

	protected:
		CConcurrencyManager *mp_pConcurrencyManager = nullptr;
		bool mp_bDestroyed = false;

	private:
		void fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);

		TCFuture<void> fp_DestroyInternal();

	protected:
		virtual void fp_Construct();
		virtual TCFuture<void> fp_Destroy();

		NException::CExceptionPointer fp_CheckDestroyed();

		template <typename tf_CType>
		bool fp_CheckDestroyed(TCPromise<tf_CType> const &o_Promise);

	public:
		NPrivate::CThisActor self;

		typedef CDefaultActorHolder CActorHolder;

		inline_always CConcurrencyManager &f_ConcurrencyManager() const
		{
			return *mp_pConcurrencyManager;
		}

		CActor();
		CActor(CActor &&) = delete;
		CActor(CActor const &) = delete;
		CActor &operator = (CActor &&) = delete;
		CActor &operator = (CActor const &) = delete;

		bool f_IsDestroyed() const;

		virtual ~CActor();
		void f_Dispatch(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);
		template <typename tf_CReturnType>
		tf_CReturnType f_DispatchWithReturn(NFunction::TCFunctionMovable<tf_CReturnType ()> &&_fToDisptach);

		enum
		{
			mc_bAllowInternalAccess = false
			, mc_bImmediateDelete = false
			, mc_Priority = EPriority_Normal
			, mc_bCanBeEmpty = false
		};
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
