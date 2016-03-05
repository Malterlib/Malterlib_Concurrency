// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{

		template <typename t_CResultCall>
		template <typename tf_CResultCall>
		COnDestroyActor::TCCallback<t_CResultCall>::TCCallback(tf_CResultCall &&_ResultCall)
			: m_ResultCall(fg_Forward<tf_CResultCall>(_ResultCall))
		{
		}

		template <typename t_CResultCall>
		void COnDestroyActor::TCCallback<t_CResultCall>::f_DoCallback()
		{
			fg_AnyConcurrentActor()(&CConcurrentActor::f_Call)
				> fg_Move(m_ResultCall)
			;
		}

		template <typename tf_CResultCall>
		COnDestroyActor::CCallbackReference COnDestroyActor::f_OnDestroy(tf_CResultCall const &_ActorCall) 
		{
			DMibRequire(this);

			auto pCallbackHandle = &m_OnDestroyCallbacks.f_Insert();

			pCallbackHandle->m_pCallback = fg_Construct<TCCallback<typename NTraits::TCRemoveReference<tf_CResultCall>::CType>>(fg_Move(_ActorCall));

			CCallbackReference Ret;

			Ret.m_pHandle = pCallbackHandle;
			Ret.m_Actor = fg_ThisActor(this);

			return Ret;
		}


		template <typename tf_CActor, typename tf_FCallback, typename tf_CResultCall>
		void fg_OnActorDestroyed(TCActor<tf_CActor> const &_ActorToOnDestroy, tf_FCallback const &_Callback, tf_CResultCall &&_ResultCall)
		{
			auto pOnDestroyFunc = &tf_CActor::template f_OnDestroy<tf_FCallback &&>;
			_ActorToOnDestroy(pOnDestroyFunc, _Callback)
				> fg_Forward<tf_CResultCall>(_ResultCall)
			;
		}

	}
}
