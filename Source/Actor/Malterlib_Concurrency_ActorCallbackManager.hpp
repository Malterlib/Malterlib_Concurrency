// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorCallbackManager.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <typename ...tfp_CParam>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackHandle::CCallbackHandle(tfp_CParam && ...p_ExtraData)
		: t_CExtraData(fg_Forward<tfp_CParam>(p_ExtraData)...)
	{
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CInternal::CInternal(CActor *_pActor, bool _bDeferrCallbacks)
		: mp_pActor(_pActor)
		, mp_bDeferrCallbacks(_bDeferrCallbacks)
	{
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::TCActorSubscriptionManager(CActor *_pActor, bool _bDeferrCallbacks)
		: mp_pInternal(fg_Construct(_pActor, _bDeferrCallbacks))
	{
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::~TCActorSubscriptionManager()
	{
		auto &Internal = *mp_pInternal;
		Internal.mp_bDestroyed = true;
		Internal.mp_DeferredCallbacks.f_Clear();

		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto DestroyActor = Callback.m_Actor.f_Lock();
			if (!DestroyActor)
				DestroyActor = fg_ConcurrentActor();

			fg_Dispatch
				(
					DestroyActor
					, [pCallback = fg_Move(Callback.m_pCallback)]
					{
					}
				)
				> fg_DiscardResult()
			;

			Callback.m_Actor.f_Clear();
		}

		Internal.mp_Callbacks.f_Clear();
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_GetCallback(CActorSubscription const &_Subscription)
		-> CSharedCallback
	{
		CCallbackReference *pCallbackReference = static_cast<CCallbackReference *>(_Subscription.f_Get());
		return pCallbackReference->m_pHandle->m_pCallback;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <typename ...tfp_CParam>
	CActorSubscription TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Register
		(
		 	TCActor<CActor> _pActor
		 	, CSharedCallback const &_pCallback
		 	, tfp_CParam && ...p_ExtraData
		)
	{
		auto &Internal = *mp_pInternal;

		if constexpr (!t_bSupportMultiple)
		{
			if (!Internal.mp_Callbacks.f_IsEmpty())
				f_Clear();
		}

		CCallbackHandle &CallbackHandle = Internal.mp_Callbacks.f_Insert(fg_Construct(fg_Forward<tfp_CParam>(p_ExtraData)...));
		CallbackHandle.m_pCallback = _pCallback;
		CallbackHandle.m_Actor = fg_Move(_pActor);
		NStorage::TCUniquePointer<CCallbackReference> pRet = fg_Construct(&CallbackHandle, mp_pInternal, fg_ThisActor(Internal.mp_pActor));

		if (Internal.mp_bDeferrCallbacks)
		{
			Internal.mp_bDeferrCallbacks = false;
			while (!Internal.mp_DeferredCallbacks.f_IsEmpty())
			{
				auto &ToDispatch = Internal.mp_DeferredCallbacks.f_GetFirst();
				ToDispatch();
				Internal.mp_DeferredCallbacks.f_Remove(ToDispatch);
			}
		}

		return fg_Move(pRet);
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <typename ...tfp_CParam>
	CActorSubscription TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Register
		(
			TCActor<CActor> _pActor
			, NFunction::TCFunctionMovable<t_CReturn (tp_CCallbackParams...)> &&_fCallback
			, tfp_CParam && ...p_ExtraData
		)
	{
		return f_Register(_pActor, CSharedCallback(fg_Construct(fg_Move(_fCallback))), fg_Forward<tfp_CParam>(p_ExtraData)...);
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	bool TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_IsDeferring()
	{
		auto &Internal = *mp_pInternal;
		return Internal.mp_bDeferrCallbacks;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	bool TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_IsEmpty()
	{
		auto &Internal = *mp_pInternal;
		return Internal.mp_Callbacks.f_IsEmpty();
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	void TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Clear()
	{
		auto &Internal = *mp_pInternal;
		Internal.mp_DeferredCallbacks.f_Clear();
		Internal.mp_bDeferrCallbacks = false;
		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto DestroyActor = Callback.m_Actor.f_Lock();
			if (!DestroyActor)
				DestroyActor = fg_ConcurrentActor();

			fg_Dispatch
				(
					DestroyActor
					, [pCallback = fg_Move(Callback.m_pCallback)]
					{
					}
				)
				> fg_DiscardResult()
			;

			Callback.m_Actor.f_Clear();
		}
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	void TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_StopDeferring()
	{
		auto &Internal = *mp_pInternal;
		Internal.mp_DeferredCallbacks.f_Clear();
		Internal.mp_bDeferrCallbacks = false;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::operator ()(tp_CCallbackParams... p_Params)
	{
		return f_Call(fg_Forward<tp_CCallbackParams>(p_Params)...);
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <bool tf_bSupportMultiple>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Call(tp_CCallbackParams... p_Params)
		-> typename TCEnableIf<!tf_bSupportMultiple, TCFuture<CReturn>>::CType
	{
		TCPromise<CReturn> Promise;
		auto &Internal = *mp_pInternal;
		if (Internal.mp_Callbacks.f_IsEmpty() && Internal.mp_bDeferrCallbacks)
		{
			Internal.mp_DeferredCallbacks.f_Insert
				(
					[this, Promise, ...Params = fg_Forward<tp_CCallbackParams>(p_Params)]() mutable
					{
						bool bFound = false;
						auto &Internal = *mp_pInternal;
						for (auto &Callback : Internal.mp_Callbacks)
						{
							auto Actor = Callback.m_Actor.f_Lock();
							if (!Actor)
								continue;
							fg_Dispatch
								(
									Actor
									, [pCallback = Callback.m_pCallback, ...Params = fg_Move(Params)]() mutable mark_no_coroutine_debug -> t_CReturn
									{
										return (*pCallback)(fg_Move(Params)...);
									}
								)
								> Promise;
							;
							bFound = true;
							break;
						}
					}
				)
			;
			return Promise.f_MoveFuture();
		}
		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto Actor = Callback.m_Actor.f_Lock();
			if (!Actor)
				continue;
			fg_Dispatch
				(
					Actor
					, [pCallback = Callback.m_pCallback, ...Params = fg_Forward<tp_CCallbackParams>(p_Params)]() mutable mark_no_coroutine_debug -> t_CReturn
					{
						return (*pCallback)(fg_Move(Params)...);
					}
				)
				> Promise
			;
			return Promise.f_MoveFuture();
		}
		return Promise <<= DMibErrorInstance("No callback registered");
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <bool tf_bSupportMultiple>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Call(tp_CCallbackParams... p_Params)
		-> typename TCEnableIf<tf_bSupportMultiple, TCFuture<NContainer::TCVector<TCAsyncResult<CReturn>>>>::CType
	{
		TCPromise<NContainer::TCVector<TCAsyncResult<CReturn>>> Promise;

		auto &Internal = *mp_pInternal;
		if (Internal.mp_Callbacks.f_IsEmpty() && Internal.mp_bDeferrCallbacks)
		{
			Internal.mp_DeferredCallbacks.f_Insert
				(
					[this, Promise, ...Params = fg_Forward<tp_CCallbackParams>(p_Params)]() mutable
					{
						auto &Internal = *mp_pInternal;
						TCActorResultVector<CReturn> Results;
						for (auto &Callback : Internal.mp_Callbacks)
						{
							auto Actor = Callback.m_Actor.f_Lock();
							if (!Actor)
								continue;
							fg_Dispatch
								(
									Actor
									, [pCallback = Callback.m_pCallback, ...Params = Params]() mutable mark_no_coroutine_debug -> t_CReturn
									{
										return (*pCallback)(fg_Move(Params)...);
									}
								)
								> Results.f_AddResult();
							;
						}
						Results.f_GetResults() > Promise;
					}
				)
			;
			return Promise.f_MoveFuture();
		}

		TCActorResultVector<CReturn> Results;

		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto Actor = Callback.m_Actor.f_Lock();
			if (!Actor)
				continue;
			fg_Dispatch
				(
					Actor
					, [pCallback = Callback.m_pCallback, ...Params = p_Params]() mutable mark_no_coroutine_debug -> t_CReturn
					{
						return (*pCallback)(fg_Move(Params)...);
					}
				)
				> Results.f_AddResult();
			;
		}

		Results.f_GetResults() > Promise;
		return Promise.f_MoveFuture();
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <bool tf_bSupportMultiple, typename tf_FResult>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_CallSpecific
		(
		 	tf_FResult &&_fDoCall
		 	, t_CExtraData &_ExtraData
		 	, tp_CCallbackParams... p_Params
		)
		-> typename TCEnableIf<tf_bSupportMultiple>::CType
	{
		auto &Internal = *mp_pInternal;
		if (Internal.mp_bDeferrCallbacks)
			DMibError("Deferred callbacks not supported");
		auto &Callback = *static_cast<CCallbackHandle *>(&_ExtraData);

		auto Actor = Callback.m_Actor.f_Lock();
		if (!Actor)
			return;

		_fDoCall
			(
				[&]
				{
					auto pCallback = Callback.m_pCallback;
					return fg_Dispatch
						(
							Actor
							, [pCallback = fg_Move(pCallback), ...Params = fg_Forward<tp_CCallbackParams>(p_Params)]() mutable mark_no_coroutine_debug -> t_CReturn
							{
								return (*pCallback)(fg_Move(Params)...);
							}
						)
					;
				}
				, static_cast<t_CExtraData &>(Callback)
			)
		;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	template <bool tf_bSupportMultiple, typename tf_FResult>
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_CallEach(tf_FResult &&_fDoCall, tp_CCallbackParams... p_Params)
		-> typename TCEnableIf<tf_bSupportMultiple>::CType
	{
		auto &Internal = *mp_pInternal;
		if (Internal.mp_bDeferrCallbacks)
			DMibError("Deferred callbacks not supported");

		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto Actor = Callback.m_Actor.f_Lock();
			if (!Actor)
				continue;
			_fDoCall
				(
					[&]
				 	{
						auto pCallback = Callback.m_pCallback;
						return fg_Dispatch
							(
								Actor
								, [pCallback = fg_Move(pCallback), ...Params = p_Params]() mutable mark_no_coroutine_debug -> t_CReturn
								{
									return (*pCallback)(fg_Move(Params)...);
								}
							)
						;
					}
					, static_cast<t_CExtraData &>(Callback)
				)
			;
		}
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCFuture<void> TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::fp_RemoveCallback()
	{
		TCPromise<void> Promise;

		if (!m_pHandle)
			return Promise <<= g_Void;

		auto Actor = m_Actor.f_Lock();
		if (!Actor)
			return Promise <<= g_Void;

		auto pHandle = m_pHandle;
		m_pHandle = nullptr;

		return Promise <<= Actor
			(
				&CActor::fp_DisptachInternal
				, [pHandle, pCallbackManager = m_pCallbackManager]
				{
					if (!pCallbackManager->mp_bDestroyed)
					{
						auto DestroyActor = pHandle->m_Actor.f_Lock();
						if (!DestroyActor)
							DestroyActor = fg_ConcurrentActor();

						fg_Dispatch
							(
								DestroyActor
								, [pCallback = fg_Move(pHandle->m_pCallback)]
								{
								}
							)
							> fg_DiscardResult()
						;

						pCallbackManager->mp_Callbacks.f_Remove(*pHandle);
					}
				}
			)
		;
	}
	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference
		(
			CCallbackHandle *_pHandle
			, NStorage::TCSharedPointer<CInternal> const &_pManager
			, TCWeakActor<CActor> const &_Actor
		)
		: m_pHandle(_pHandle)
		, m_pCallbackManager(_pManager)
		, m_Actor(_Actor)
	{
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference()
		: m_pHandle(nullptr)
	{
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference(TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference &&_Other)
		: m_pHandle(_Other.m_pHandle)
	{
		_Other.m_pHandle = nullptr;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	void TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::fp_RemoveCurrent()
	{
		if (!m_pHandle)
			return;

		fp_RemoveCallback() > NPrivate::fg_DirectResultActor() / [](TCAsyncResult<void> &&_Result)
			{
				if (!_Result)
				{
					try
					{
						_Result.f_Access();
					}
					catch (CExceptionActorDeleted const &)
					{
					}
					catch (...)
					{
						DMibDTrace2("Callback reference failed to destroy: {}\n", _Result.f_GetExceptionStr());
						DMibFastCheck(false);
					}
				}
			}
		;
		m_pHandle = nullptr;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	typename TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference &
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::operator =(TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference &&_Other)
	{
		fp_RemoveCurrent();

		m_pHandle = _Other.m_pHandle;
		_Other.m_pHandle = nullptr;

		return *this;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::~CCallbackReference()
	{
		if (!m_pHandle)
			return;

		fp_RemoveCurrent();
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCFuture<void> TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::f_Destroy()
	{
		return fp_RemoveCallback();
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	bool TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::f_IsValid()
	{
		return m_pHandle;
	}
}
