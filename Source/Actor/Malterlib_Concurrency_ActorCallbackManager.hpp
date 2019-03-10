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
					, [fCallback = fg_Move(Callback.m_fCallback)]
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
	template <typename ...tfp_CParam>
	CActorSubscription TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Register
		(
			TCActor<CActor> _pActor
			, NFunction::TCFunctionMutable<t_CReturn (tp_CCallbackParams...)> &&_fCallback
			, tfp_CParam && ...p_ExtraData
		)
	{
		auto &Internal = *mp_pInternal;

		if (!t_bSupportMultiple && !Internal.mp_Callbacks.f_IsEmpty())
			f_Clear();

		CCallbackHandle &CallbackHandle = Internal.mp_Callbacks.f_Insert(fg_Construct(fg_Forward<tfp_CParam>(p_ExtraData)...));
		CallbackHandle.m_fCallback = fg_Move(_fCallback);
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
					, [fCallback = fg_Move(Callback.m_fCallback)]
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


	namespace NPrivate
	{
		template <typename tf_CCallback, mint... tfp_Indices, typename... tfp_CParams, typename... tfp_COriginalTypes>
		auto fg_CallCallback
			(
				tf_CCallback &&_Callback
				, DMibTupleTemplate<tfp_CParams...> &&_Params
				, NMeta::TCIndices<tfp_Indices...> const &
				, NMeta::TCTypeList<tfp_COriginalTypes...> const &
			)
		{
			return _Callback(fg_Forward<tfp_COriginalTypes>(fg_Get<tfp_Indices>(_Params))...);
		}
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
					[this, Promise, Params = NStorage::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...)]() mutable
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
									, [fCallback = Callback.m_fCallback, Params = fg_Move(Params)]() mutable
									{
										 return NPrivate::fg_CallCallback
											(
												fg_Move(fCallback)
												, fg_Move(Params)
												,
												typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
												, NMeta::TCTypeList<tp_CCallbackParams...>()
											)
										;
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
					, [fCallback = Callback.m_fCallback, Params = NStorage::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...)]() mutable
					{
						 return NPrivate::fg_CallCallback
							(
								fg_Move(fCallback)
								, fg_Move(Params)
								, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
								, NMeta::TCTypeList<tp_CCallbackParams...>()
							)
						;
					}
				)
				> Promise
			;
			return Promise.f_MoveFuture();
		}
		return DMibErrorInstance("No callback registered");
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
					[this, Promise, Params = NStorage::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...)]() mutable
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
									, [fCallback = Callback.m_fCallback, Params]() mutable
									{
										 return NPrivate::fg_CallCallback
											(
												fg_Move(fCallback)
												, fg_Move(Params)
												,
												typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
												, NMeta::TCTypeList<tp_CCallbackParams...>()
											)
										;
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

		auto Params = NStorage::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);

		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto Actor = Callback.m_Actor.f_Lock();
			if (!Actor)
				continue;
			fg_Dispatch
				(
					Actor
					, [fCallback = Callback.m_fCallback, Params]() mutable
					{
						 return NPrivate::fg_CallCallback
							(
								fg_Move(fCallback)
								, fg_Move(Params)
								, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
								, NMeta::TCTypeList<tp_CCallbackParams...>()
							)
						;
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
	auto TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_CallEach(tf_FResult &&_fDoCall, tp_CCallbackParams... p_Params)
		-> typename TCEnableIf<tf_bSupportMultiple>::CType
	{
		auto &Internal = *mp_pInternal;
		if (Internal.mp_bDeferrCallbacks)
			DMibError("Deferred callbacks not supported");

		auto Params = NStorage::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);

		for (auto &Callback : Internal.mp_Callbacks)
		{
			auto Actor = Callback.m_Actor.f_Lock();
			if (!Actor)
				continue;
			_fDoCall
				(
					fg_Dispatch
					(
						Actor
						, [fCallback = Callback.m_fCallback, Params]() mutable
						{
							 return NPrivate::fg_CallCallback
								(
									fg_Move(fCallback)
									, fg_Move(Params)
									,
									typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
									, NMeta::TCTypeList<tp_CCallbackParams...>()
								)
							;
						}
					)
					, static_cast<t_CExtraData &>(Callback)
				)
			;
		}
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	TCFuture<void> TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::fp_RemoveCallback()
	{
		if (!m_pHandle)
			return fg_Explicit();

		auto Actor = m_Actor.f_Lock();
		if (!Actor)
			return fg_Explicit();

		auto pHandle = m_pHandle;
		m_pHandle = nullptr;

		return Actor
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
							 	, [fCallback = fg_Move(pHandle->m_fCallback)]
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
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference(CCallbackReference &&_Other)
		: m_pHandle(_Other.m_pHandle)
	{
		_Other.m_pHandle = nullptr;
	}

	template <typename t_CReturn, typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
	void TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::fp_RemoveCurrent()
	{
		if (!m_pHandle)
			return;
		fg_AnyConcurrentActor().f_CallByValue
			(
				&CActor::f_DispatchWithReturn<TCFuture<void>>
				, [Future = fp_RemoveCallback()]() mutable
				{
					return fg_Move(Future);
				}
			)
			> NPrivate::fg_DirectResultActor() / [](TCAsyncResult<void> &&_Result)
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
	TCActorSubscriptionManager<t_CReturn (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::operator =(CCallbackReference &&_Other)
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
