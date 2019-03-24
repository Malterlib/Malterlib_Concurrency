// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/WeakActor>

namespace NMib::NConcurrency
{
	template <typename t_CCallbackSignature, bool t_bSupportMultiple = false, typename t_CExtraData = CEmpty>
	class TCActorSubscriptionManager;

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	class TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>
	{
	public:
		using CSharedCallback = NStorage::TCSharedPointer<NFunction::TCFunctionMovable<t_CReturn (tp_CCallbackParams...)>>;
		struct CCallbackHandle : public t_CExtraData
		{
			template <typename ...tfp_CParam>
			CCallbackHandle(tfp_CParam && ...p_ExtraData);
			TCWeakActor<CActor> m_Actor;
			CSharedCallback m_pCallback;
		};
		using CReturn = typename NPrivate::TCRemoveFuture<t_CReturn>::CType;
	public:
		TCActorSubscriptionManager(CActor *_pActor, bool _bDeferrCallbacks);
		~TCActorSubscriptionManager();

		template <typename ...tfp_CParam>
		CActorSubscription f_Register(TCActor<CActor> _pActor, NFunction::TCFunctionMovable<t_CReturn (tp_CCallbackParams...)> &&_fCallback, tfp_CParam && ...p_ExtraData);
		template <typename ...tfp_CParam>
		CActorSubscription f_Register(TCActor<CActor> _pActor, CSharedCallback const &_pCallback, tfp_CParam && ...p_ExtraData);
		CSharedCallback f_GetCallback(CActorSubscription const &_Subscription);

		bool f_IsEmpty();
		bool f_IsDeferring();
		void f_StopDeferring();
		void f_Clear();

		auto operator () (tp_CCallbackParams... p_Params);

		template <bool tf_bSupportMultiple = t_bSupportMultiple>
		typename TCEnableIf<!tf_bSupportMultiple, TCFuture<CReturn>>::CType f_Call(tp_CCallbackParams... p_Params);

		template <bool tf_bSupportMultiple = t_bSupportMultiple>
		typename TCEnableIf<tf_bSupportMultiple, TCFuture<NContainer::TCVector<TCAsyncResult<CReturn>>>>::CType f_Call(tp_CCallbackParams... p_Params);

		template <bool tf_bSupportMultiple = t_bSupportMultiple, typename tf_FResult>
		typename TCEnableIf<tf_bSupportMultiple>::CType f_CallEach(tf_FResult &&_fDoCall, tp_CCallbackParams... p_Params);

		template <bool tf_bSupportMultiple = t_bSupportMultiple, typename tf_FResult>
		typename TCEnableIf<tf_bSupportMultiple>::CType f_CallSpecific(tf_FResult &&_fDoCall, t_CExtraData &_ExtraData, tp_CCallbackParams... p_Params);

	private:
		struct CInternal
		{
			CInternal(CActor *_pActor, bool _bDeferrCallbacks);

			NContainer::TCLinkedList<CCallbackHandle> mp_Callbacks;
			NContainer::TCLinkedList<NFunction::TCFunctionMovable<void ()>> mp_DeferredCallbacks;
			CActor *mp_pActor;
			bool mp_bDeferrCallbacks;
			bool mp_bDestroyed = false;
		};

		class CCallbackReference : public CActorSubscriptionReference
		{
			CCallbackHandle *m_pHandle;
			NStorage::TCSharedPointer<CInternal> m_pCallbackManager;
			TCWeakActor<CActor> m_Actor;

			CCallbackReference(CCallbackReference const &_Other);
			CCallbackReference &operator =(CCallbackReference const &_Other);

			TCFuture<void> fp_RemoveCallback();
			void fp_RemoveCurrent();
		public:
			CCallbackReference(CCallbackHandle *_pHandle, NStorage::TCSharedPointer<CInternal> const &_pManager, TCWeakActor<CActor> const &_Actor);
			CCallbackReference();
			CCallbackReference(CCallbackReference &&_Other);
			CCallbackReference &operator =(CCallbackReference &&_Other);
			~CCallbackReference();
			TCFuture<void> f_Destroy() override;
			bool f_IsValid();
		};
	private:

	private:

		NStorage::TCSharedPointer<CInternal> mp_pInternal;
	};

	class CCombinedCallbackReference : public CActorSubscriptionReference
	{
	public:
		CCombinedCallbackReference();
		~CCombinedCallbackReference();
		TCFuture<void> f_Destroy() override;

		NContainer::TCVector<CActorSubscription> m_References;
	};

	template <typename... tf_CParam>
	CActorSubscription fg_CombinedCallbackReference(tf_CParam &&...p_Params)
	{
		NStorage::TCUniquePointer<CCombinedCallbackReference> pCombinedReference = fg_Construct();
		fg_Swallow(pCombinedReference->m_References.f_Insert(fg_Move(p_Params))...);
		return fg_Move(pCombinedReference);
	}
}

#include "Malterlib_Concurrency_ActorCallbackManager.hpp"
