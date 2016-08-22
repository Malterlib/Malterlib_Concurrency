// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ActorCallbackManager.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		template <typename ...tfp_CParam>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackHandle::CCallbackHandle(tfp_CParam && ...p_ExtraData)
			: t_CExtraData(fg_Forward<tfp_CParam>(p_ExtraData)...)
		{
		}



		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CInternal::CInternal(CActor *_pActor, bool _bDeferrCallbacks)
			: mp_pActor(_pActor)
			, mp_bDeferrCallbacks(_bDeferrCallbacks)
		{
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::TCActorSubscriptionManager(CActor *_pActor, bool _bDeferrCallbacks)
			: mp_pInternal(fg_Construct(_pActor, _bDeferrCallbacks))
		{
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::~TCActorSubscriptionManager()
		{
			auto &Internal = *mp_pInternal;
			Internal.mp_bDestroyed = true;
			Internal.mp_DeferredCallbacks.f_Clear();
			Internal.mp_Callbacks.f_Clear();
		}
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		template <typename ...tfp_CParam>
		CActorSubscription TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Register
			(
				TCActor<CActor> _pActor
				, NFunction::TCFunction<void (NFunction::CThisTag &, tp_CCallbackParams...)> &&_fCallback
				, tfp_CParam && ...p_ExtraData
			)
		{
			auto &Internal = *mp_pInternal;

			DMibCheck(t_bSupportMultiple || Internal.mp_Callbacks.f_IsEmpty());
			
			CCallbackHandle &CallbackHandle = Internal.mp_Callbacks.f_Insert(fg_Construct(fg_Forward<tfp_CParam>(p_ExtraData)...));
			CallbackHandle.m_fCallback = fg_Move(_fCallback);
			CallbackHandle.m_Actor = fg_Move(_pActor);
			NPtr::TCUniquePointer<CCallbackReference> pRet = fg_Construct(&CallbackHandle, mp_pInternal, fg_ThisActor(Internal.mp_pActor));
			
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
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		bool TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_IsEmpty()
		{
			auto &Internal = *mp_pInternal;
			return Internal.mp_Callbacks.f_IsEmpty();
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		void TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_Clear()
		{
			auto &Internal = *mp_pInternal;
			Internal.mp_DeferredCallbacks.f_Clear();
			Internal.mp_bDeferrCallbacks = false;
			for (auto &Callback : Internal.mp_Callbacks)
			{
				Callback.m_Actor.f_Clear();
				Callback.m_fCallback.f_Clear();
			}
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		void TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::f_StopDeferring()
		{
			auto &Internal = *mp_pInternal;
			Internal.mp_DeferredCallbacks.f_Clear();
			Internal.mp_bDeferrCallbacks = false;
		}


		namespace NPrivate
		{
			template <typename tf_CCallback, mint... tfp_Indices, typename... tfp_CParams, typename... tfp_COriginalTypes>
			void fg_CallCallback
				(
					tf_CCallback &&_Callback
					, DMibTupleTemplate<tfp_CParams...> &&_Params
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_COriginalTypes...> const &
				)
			{
				_Callback(fg_Forward<tfp_COriginalTypes>(NContainer::fg_Get<tfp_Indices>(_Params))...);
			}
		}
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		template <bool tf_bSupportMultiple>
		typename TCEnableIf<tf_bSupportMultiple>::CType TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::operator ()(tp_CCallbackParams... p_Params)
		{
			auto &Internal = *mp_pInternal;
			if (Internal.mp_Callbacks.f_IsEmpty() && Internal.mp_bDeferrCallbacks)
			{
				auto Params = NContainer::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);
				auto LambdaParams = fg_LambdaMove(Params);
				
				Internal.mp_DeferredCallbacks.f_Insert
					(
						[this, LambdaParams]
						{
							auto &Internal = *mp_pInternal;
							auto OriginalParams = fg_Move(*LambdaParams);
							for (auto &Callback : Internal.mp_Callbacks)
							{
								auto Actor = Callback.m_Actor.f_Lock();
								if (!Actor)
									continue;
								auto fCallback = Callback.m_fCallback;
								auto Params = OriginalParams;
								auto LambdaParams = fg_LambdaMove(Params);
								auto CallbackMove = fg_LambdaMove(fCallback);
								Actor
									(
										&CActor::fp_DisptachInternal
										, [CallbackMove, LambdaParams]()
										{
											 NPrivate::fg_CallCallback
												(
													fg_Move(*CallbackMove)
													, fg_Move(*LambdaParams)
													, 
				#ifndef DCompiler_MSVC
													typename 
				#endif
													NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
													, NMeta::TCTypeList<tp_CCallbackParams...>()
												)
											;
										}
									)
									> fg_DiscardResult()
								;
							}
						}
					)
				;
				return;
			}
			auto OriginalParams = NContainer::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);
			for (auto &Callback : Internal.mp_Callbacks)
			{
				auto Actor = Callback.m_Actor.f_Lock();
				if (!Actor)
					continue;
				auto fCallback = Callback.m_fCallback;
				auto Params = OriginalParams;
				auto LambdaParams = fg_LambdaMove(Params);
				auto CallbackMove = fg_LambdaMove(fCallback);
				Actor
					(
						&CActor::fp_DisptachInternal
						, [CallbackMove, LambdaParams]()
						{
							 NPrivate::fg_CallCallback
								(
									fg_Move(*CallbackMove)
									, fg_Move(*LambdaParams)
									, 
#ifndef DCompiler_MSVC
									typename 
#endif
									NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
									, NMeta::TCTypeList<tp_CCallbackParams...>()
								)
							;
						}
					)
					> fg_DiscardResult()
				;
			}
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		template <bool tf_bSupportMultiple>
		typename TCEnableIf<!tf_bSupportMultiple>::CType TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::operator ()(tp_CCallbackParams... p_Params)
		{
			auto &Internal = *mp_pInternal;
			if (Internal.mp_Callbacks.f_IsEmpty() && Internal.mp_bDeferrCallbacks)
			{
				auto Params = NContainer::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);
				auto LambdaParams = fg_LambdaMove(Params);
				
				Internal.mp_DeferredCallbacks.f_Insert
					(
						[this, LambdaParams]
						{
							auto &Internal = *mp_pInternal;
							for (auto &Callback : Internal.mp_Callbacks)
							{
								auto Actor = Callback.m_Actor.f_Lock();
								if (!Actor)
									continue;
								auto fCallback = Callback.m_fCallback;
								auto CallbackMove = fg_LambdaMove(fCallback);
								
								Actor
									(
										&CActor::fp_DisptachInternal
										, [CallbackMove, LambdaParams]()
										{
											 NPrivate::fg_CallCallback
												(
													fg_Move(*CallbackMove)
													, fg_Move(*LambdaParams)
													, 
				#ifndef DCompiler_MSVC
													typename 
				#endif
													NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
													, NMeta::TCTypeList<tp_CCallbackParams...>()
												)
											;
										}
									)
									> fg_DiscardResult()
								;
							}
						}
					)
				;
				return;
			}
			for (auto &Callback : Internal.mp_Callbacks)
			{
				auto Actor = Callback.m_Actor.f_Lock();
				if (!Actor)
					continue;
				auto fCallback = Callback.m_fCallback;
				auto Params = NContainer::fg_Tuple(fg_Forward<tp_CCallbackParams>(p_Params)...);;
				auto LambdaParams = fg_LambdaMove(Params);
				auto CallbackMove = fg_LambdaMove(fCallback);
				Actor
					(
						&CActor::fp_DisptachInternal
						, [CallbackMove, LambdaParams]()
						{
							 NPrivate::fg_CallCallback
								(
									fg_Move(*CallbackMove)
									, fg_Move(*LambdaParams)
									, 
#ifndef DCompiler_MSVC
									typename 
#endif
									NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCallbackParams)>::CType()
									, NMeta::TCTypeList<tp_CCallbackParams...>()
								)
							;
						}
					)
					> fg_DiscardResult()
				;
			}
		}
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		void TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::fp_RemoveCallback()
		{
			if (m_pHandle)
			{
				auto Actor = m_Actor.f_Lock();
				if (Actor)
				{
					auto pHandle = m_pHandle;
					auto pCallbackManager = m_pCallbackManager;
					Actor
						(
							&CActor::fp_DisptachInternal
							, [pHandle, pCallbackManager]()
							{
								if (!pCallbackManager->mp_bDestroyed)
									pCallbackManager->mp_Callbacks.f_Remove(*pHandle);
							}
						)
						> Actor / [](TCAsyncResult<void> &&_Result)
						{
							_Result.f_Get();
						}
					;
				}
			}
		}
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference
			(
				CCallbackHandle *_pHandle
				, NPtr::TCSharedPointer<CInternal> const &_pManager
				, TCWeakActor<CActor> const &_Actor
			)
			: m_pHandle(_pHandle)
			, m_pCallbackManager(_pManager)
			, m_Actor(_Actor)
		{
		}
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference()
			: m_pHandle(nullptr)
		{
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::CCallbackReference(CCallbackReference &&_Other)
			: m_pHandle(_Other.m_pHandle)
		{
			_Other.m_pHandle = nullptr;
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		typename TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference &
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::operator =(CCallbackReference &&_Other)
		{ 
			fp_RemoveCallback();
			m_pHandle = _Other.m_pHandle;
			_Other.m_pHandle = nullptr;
			return *this;
		}

		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::~CCallbackReference()
		{
			fp_RemoveCallback();
		}
		
		template <typename... tp_CCallbackParams, bool t_bSupportMultiple, typename t_CExtraData>
		bool TCActorSubscriptionManager<void (tp_CCallbackParams...), t_bSupportMultiple, t_CExtraData>::CCallbackReference::f_IsValid()
		{
			return m_pHandle;
		}
	}
}
