// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CActor>
		TCActor<tf_CActor> CActorHolder::fp_GetAsActor()
		{
			return TCActorHolderSharedPointer<TCActorInternal<tf_CActor>>
				(
					fg_Explicit(static_cast<TCActorInternal<tf_CActor> *>(this))
				)
			;
		}

#if 0
		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> &&_ResultCall)
		{
			TCActor<CActor> pActor = fp_GetAsActor<CActor>();

			pActor(&CActor::fp_DestroyInternal)
				> fg_AnyConcurrentActor() / [pActor, ResultCall = fg_Move(_ResultCall)](TCAsyncResult<void> &&_Result) mutable
				{
					NFunction::TCFunctionNoAllocMutable<void ()> fOnDestroyed = NFunction::TCFunctionSmallMutable<void ()>
						{
							[ResultCall = fg_Move(ResultCall), Result = fg_Move(_Result)]() mutable
							{
								if (NTraits::TCIsSame<tf_CActor, TCActor<NPrivate::CDirectResultActor>>::mc_Value)
								{
									NPrivate::fg_CallResultFunctorDirect(ResultCall.mp_Functor, fg_Move(Result));
									return;
								}
								auto ResultActor = ResultCall.mp_Actor.f_GetActor();
								ResultActor->f_QueueProcess
									(
										[ResultCall = fg_Move(ResultCall), Result = fg_Move(Result)]() mutable
										{
											NPrivate::fg_CallResultFunctor(ResultCall.mp_Functor, ResultCall.mp_Actor.f_GetActor()->fp_GetActor(), fg_Move(Result));
										}
									)
								;
							}
						}
					;
					if (!pActor->fp_Terminate(fg_Move(fOnDestroyed)))
						fOnDestroyed();
				}
			;
		}

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CActor &&_Actor, tf_CFunctor &&_Functor)
		{
			f_Destroy(fg_Forward<tf_CActor>(_Actor) / fg_Forward<tf_CFunctor>(_Functor));
		}

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CActor *_pActor, tf_CFunctor &&_Functor)
		{
			f_Destroy(fg_ThisActor(_pActor) / fg_Forward<tf_CFunctor>(_Functor));
		}

		template <typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CFunctor &&_Functor)
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			f_Destroy(pActor / fg_Forward<tf_CFunctor>(_Functor));
		}
#endif
		
		CConcurrencyManager &CActorHolder::f_ConcurrencyManager() const
		{
			return *mp_pConcurrencyManager;
		}
		
		EPriority CActorHolder::f_GetPriority() const
		{
			return mp_Priority;
		}
	}
}
